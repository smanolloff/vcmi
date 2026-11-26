/*
 * BattleCheckpoint.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "ServerPlugin.h"

#include "lib/CStack.h"
#include "lib/StartInfo.h"
#include "lib/battle/BattleInfo.h"
#include "lib/battle/CObstacleInstance.h"
#include "lib/callback/GameRandomizer.h"
#include "lib/gameState/CGameState.h"
#include "lib/mapObjects/army/CStackBasicDescriptor.h"
#include "lib/serializer/CMemorySerializer.h"
#include "server/CGameHandler.h"

namespace ML
{
namespace
{
constexpr BattleSide battleSides[] = {BattleSide::ATTACKER, BattleSide::DEFENDER};

std::vector<Bonus> copyLocalBonuses(const CBonusSystemNode & node)
{
	std::vector<Bonus> result;
	result.reserve(node.getExportedBonusList().size());
	for(const auto & bonus : node.getExportedBonusList())
		result.push_back(*bonus);
	return result;
}

void addLocalBonuses(CBonusSystemNode & node, const std::vector<Bonus> & bonuses)
{
	for(const auto & bonus : bonuses)
		node.addNewBonus(std::make_shared<Bonus>(bonus));
}

std::vector<std::shared_ptr<CObstacleInstance>> copyObstacles(const std::vector<std::shared_ptr<CObstacleInstance>> & obstacles)
{
	std::vector<std::shared_ptr<CObstacleInstance>> result;
	result.reserve(obstacles.size());
	for(const auto & obstacle : obstacles)
	{
		if(const auto * spell = dynamic_cast<const SpellCreatedObstacle *>(obstacle.get()))
			result.push_back(std::make_shared<SpellCreatedObstacle>(*spell));
		else
			result.push_back(std::make_shared<CObstacleInstance>(*obstacle));
	}
	return result;
}

struct StackCheckpoint
{
	uint32_t id;
	CreatureID creature;
	int32_t baseAmount;
	PlayerColor owner;
	BattleSide side;
	SlotID slot;
	BattleHex initialPosition;
	bool hasBase;
	bool removed;
	std::vector<Bonus> bonuses;
	JsonNode state;
};
}

class BattleCheckpoint
{
	friend class ServerPlugin;

	const CGameState * const gameState;
	const BattleID battleID;
	const int32_t round;
	const int32_t activeStack;
	const BattleSide tacticsSide;
	const uint8_t tacticDistance;
	const BattleSideArray<SideInBattle> sides;
	const SiegeInfo siege;
	const std::vector<std::shared_ptr<CObstacleInstance>> obstacles;
	const std::vector<Bonus> bonuses;
	BattleSideArray<int32_t> heroMana{};
	std::vector<StackCheckpoint> stacks;
	std::vector<std::byte> rng;

public:
	BattleCheckpoint(const CGameState & gameState, const BattleInfo & battle, const GameRandomizer * randomizer)
		: gameState(&gameState)
		, battleID(battle.battleID)
		, round(battle.round)
		, activeStack(battle.activeStack)
		, tacticsSide(battle.tacticsSide)
		, tacticDistance(battle.tacticDistance)
		, sides(battle.sides)
		, siege(battle.si)
		, obstacles(copyObstacles(battle.obstacles))
		, bonuses(copyLocalBonuses(battle))
	{
		for(BattleSide side : battleSides)
			if(const auto * hero = battle.getSideHero(side))
				heroMana.at(side) = hero->mana;

		stacks.reserve(battle.stacks.size());
		for(const auto & stack : battle.stacks)
		{
			stacks.push_back({stack->unitId(), stack->creatureId(), stack->unitBaseAmount(), stack->unitOwner(),
				stack->unitSide(), stack->unitSlot(), stack->initialPosition, stack->base != nullptr, stack->ghost,
				copyLocalBonuses(*stack), stack->save()});
		}

		if(randomizer)
		{
			CMemorySerializer serializer;
			serializer.oser & *randomizer;
			rng = serializer.extractBuffer();
		}
	}

	void restore(CGameState & state, GameRandomizer * randomizer) const
	{
		auto * liveBattle = state.getBattle(battleID);
		if(gameState != &state || !liveBattle || (randomizer == nullptr) != rng.empty())
			throw std::logic_error("Checkpoint does not belong to this battle state");
		auto & battle = *liveBattle;
		std::vector<std::unique_ptr<CStack>> restoredStacks;
		restoredStacks.reserve(stacks.size());
		for(const auto & saved : stacks)
		{
			const CStackBasicDescriptor descriptor(saved.creature, saved.baseAmount);
			auto stack = std::make_unique<CStack>(&descriptor, saved.owner, saved.id, saved.side, saved.slot);
			stack->initialPosition = saved.initialPosition;
			if(saved.hasBase)
				stack->postDeserialize(battle.getSideArmy(saved.side));
			restoredStacks.push_back(std::move(stack));
		}

		battle.stacks.clear();
		battle.removeBonuses(Selector::all);
		battle.round = round;
		battle.activeStack = activeStack;
		battle.tacticsSide = tacticsSide;
		battle.tacticDistance = tacticDistance;
		battle.sides = sides;
		battle.si = siege;
		battle.obstacles = copyObstacles(obstacles);
		for(BattleSide side : battleSides)
			if(auto * hero = battle.battleGetFightingHero(side))
				hero->mana = heroMana.at(side);

		addLocalBonuses(battle, bonuses);
		battle.stacks = std::move(restoredStacks);
		for(size_t i = 0; i < stacks.size(); ++i)
		{
			auto & stack = *battle.stacks.at(i);
			const auto & saved = stacks.at(i);
			stack.localInit(&battle);
			addLocalBonuses(stack, saved.bonuses);
			stack.load(saved.state);
			if(saved.removed)
				stack.detachFromAll();
		}
		battle.nodeHasChanged();

		if(randomizer)
		{
			CMemorySerializer serializer(rng);
			serializer.iser & *randomizer;
		}
	}
};

std::shared_ptr<const BattleCheckpoint> captureClientBattle(const CGameState & state, BattleID battleID)
{
	const auto * battle = state.getBattle(battleID);
	if(!battle)
		throw std::logic_error("Cannot checkpoint a missing client battle");
	return std::make_shared<BattleCheckpoint>(state, *battle, nullptr);
}

void restoreClientBattle(CGameState & state, const BattleCheckpoint & checkpoint)
{
	checkpoint.restore(state, nullptr);
}

std::shared_ptr<const BattleCheckpoint> ServerPlugin::captureCheckpoint(BattleID battleID) const
{
	const auto * battle = gameState.getBattle(battleID);
	if(!battle || gameState.currentBattles.size() != 1 || endingBattle == battleID || rolloutResult(battleID) || battle->battleIsFinished())
		throw std::logic_error("Checkpoint requires one ongoing battle at a decision boundary");
	if(gameState.getStartInfo()->turnTimerInfo.isBattleEnabled())
		throw std::logic_error("Battle checkpoints require battle timers to be disabled");
	for(BattleSide side : battleSides)
		if(!battle->getSide(side).initialArtifactCharges.empty())
			throw std::logic_error("Battle checkpoints do not support artifacts that spend charges during combat");

	return std::make_shared<BattleCheckpoint>(gameState, *battle, gh->randomizer.get());
}

std::shared_ptr<const BattleCheckpoint> ServerPlugin::checkpoint(BattleID battleID)
{
	auto saved = captureCheckpoint(battleID);
	beginRollout(battleID, {});
	return saved;
}

void ServerPlugin::beginRollout(BattleID battleID, std::function<void(RolloutResult)> onTerminal)
{
	if(gameState.replayLog.isRecordingPacks())
		throw std::logic_error("Battle rollouts require replay recording to be disabled");
	rolloutBattle = battleID;
	terminalResult.reset();
	notifyRolloutEnded = std::move(onTerminal);
}

void ServerPlugin::restore(const BattleCheckpoint & checkpoint)
{
	if(checkpoint.gameState != &gameState || !isRollingOut(checkpoint.battleID))
		throw std::logic_error("Checkpoint does not belong to the active battle rollout");
	checkpoint.restore(gameState, gh->randomizer.get());
	terminalResult.reset();
}

void ServerPlugin::finishRollout(const BattleCheckpoint & root)
{
	restore(root);
	rolloutBattle.reset();
	notifyRolloutEnded = {};
}

bool ServerPlugin::isRollingOut(BattleID battleID) const
{
	return rolloutBattle == battleID;
}

std::optional<RolloutResult> ServerPlugin::rolloutResult(BattleID battleID) const
{
	return isRollingOut(battleID) ? terminalResult : std::nullopt;
}

bool ServerPlugin::captureRolloutResult(BattleID battleID, EBattleResult result, BattleSide winner)
{
	if(!isRollingOut(battleID))
		return false;

	terminalResult = RolloutResult{result, winner};
	if(notifyRolloutEnded)
		notifyRolloutEnded(*terminalResult);
	return true;
}
}
