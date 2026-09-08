// =============================================================================
// Copyright 2026 Simeon Manolov <s.manolloff@gmail.com>. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include "StdInc.h" // IWYU pragma: keep

#include "BAI/v15/fastbfs.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "CStack.h"
#include "battle/BattleAction.h"
#include "battle/BattleAttackInfo.h"
#include "battle/BattleHex.h"
#include "callback/CBattleCallback.h"
#include "lib/callback/AIFactory.h"

#include "HARBot.h"

#include <bitset>
#include <functional>
#include <limits>
#include <tuple>

namespace MMAI::BAI
{

using FastBFS = MMAI::BAI::V15::FastBFS;
using UnitNode = MMAI::BAI::V15::Graph::Nodes::Unit;

namespace
{
	constexpr int64_t SHOOTER_PRIORITY_MULTIPLIER = 4;
	constexpr int64_t SIGNIFICANT_ENEMY_VALUE_DENOMINATOR = 10;
	constexpr int64_t RETREAT_EXPOSURE_NUMERATOR = 3;
	constexpr int64_t RETREAT_EXPOSURE_DENOMINATOR = 10;
	constexpr int MAX_CONSECUTIVE_RETREATS = 2;

	bool IsWarMachine(const battle::Unit * unit)
	{
		return unit->hasBonusOfType(BonusType::SIEGE_WEAPON);
	}

	int64_t UnitValue(const battle::Unit * unit)
	{
		return UnitNode::GetValue(unit->unitType(), unit->isClone(), unit->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER);
	}

	int64_t StackValue(const CStack * stack)
	{
		return static_cast<int64_t>(stack->getCount()) * UnitValue(stack);
	}

	bool IsEligibleEnemy(const CStack * enemy)
	{
		return enemy->alive() && !IsWarMachine(enemy) && !enemy->isInvincible() && enemy->getPosition().isValid();
	}

	int64_t TotalEnemyValue(const TStacks & enemies)
	{
		int64_t result = 0;
		for(const auto * enemy : enemies)
			if(IsEligibleEnemy(enemy))
				result += StackValue(enemy);
		return result;
	}

	int64_t ExpectedDamage(const DamageRange & damage, const battle::Unit * unit)
	{
		return std::min((damage.min + damage.max) / 2, unit->getAvailableHealth());
	}

	int64_t ExpectedDamageValue(const DamageRange & damage, const battle::Unit * unit)
	{
		const auto effectiveDamage = ExpectedDamage(damage, unit);
		const auto maxHealth = unit->getMaxHealth();
		const auto unitValue = UnitValue(unit);
		const auto fullUnitsValue = effectiveDamage / maxHealth * unitValue;
		const auto partialUnitValue = effectiveDamage % maxHealth * unitValue / maxHealth;
		return fullUnitsValue + partialUnitValue;
	}

	struct ExposureTurnScope
	{
		std::unordered_set<const battle::Unit *> thisRound;
		std::unordered_set<const battle::Unit *> beforeNextTurn;
	};

	ExposureTurnScope GetExposureTurnScope(const std::shared_ptr<CPlayerBattleCallback> & battle, const CStack * stack)
	{
		std::vector<battle::Units> turnOrder;
		battle->battleGetTurnOrder(turnOrder, std::numeric_limits<size_t>::max(), 2);

		ExposureTurnScope result;
		if(!turnOrder.empty())
		{
			for(const auto * unit : turnOrder.front())
				if(unit != stack && unit->unitSide() != stack->unitSide())
					result.thisRound.insert(unit);
		}

		bool foundCurrentTurn = false;
		for(const auto & turn : turnOrder)
		{
			for(const auto * unit : turn)
			{
				if(unit == stack)
				{
					if(foundCurrentTurn)
						return result;
					foundCurrentTurn = true;
					continue;
				}

				if(foundCurrentTurn && unit->unitSide() != stack->unitSide())
					result.beforeNextTurn.insert(unit);
			}
		}
		return result;
	}

	struct AttackOption
	{
		std::bitset<GameConstants::BFIELD_SIZE> footprint;
		int64_t expectedDamage;
	};

	struct MeleeThreat
	{
		const CStack * enemy;
		std::vector<AttackOption> attackOptions;
	};

	struct MeleeSelection
	{
		int attackerCount = 0;
		int64_t expectedDamage = -1;
		std::vector<int64_t> damageByThreat;
	};

	std::vector<AttackOption> GetMeleeAttackOptions(
		const std::shared_ptr<CPlayerBattleCallback> & battle,
		const CStack * enemy,
		const CStack * target,
		const BattleHex & targetPosition,
		const BattleHexArray & vacatedHexes,
		int64_t survivingCountTwice
	)
	{
		BattleHexArray knownAccessible = vacatedHexes;
		for(const auto & hex : enemy->getHexes())
			knownAccessible.checkAndPush(hex);
		const auto reachabilityParams = ReachabilityInfo::Parameters(enemy->unitSide(), enemy, enemy->getPosition(), knownAccessible);
		const auto reachability = battle->getReachability(reachabilityParams);
		const auto availableHexes = battle->battleGetAvailableHexes(reachability, enemy, false);

		BattleHexArray targetHexes;
		targetHexes.insert(targetPosition);
		if(target->doubleWide())
			targetHexes.insert(target->occupiedHex(targetPosition));

		std::vector<AttackOption> result;
		for(const auto & targetHex : targetHexes)
		{
			for(int directionIndex = 0; directionIndex < 8; ++directionIndex)
			{
				const auto direction = BattleHex::EDir(directionIndex);
				if(!battle->battleCanAttackHex(availableHexes, enemy, targetHex, direction))
					continue;

				const auto attackFrom = battle->fromWhichHexAttack(enemy, targetHex, direction);
				if(!attackFrom.isValid())
					continue;

				std::bitset<GameConstants::BFIELD_SIZE> footprint;
				footprint.set(attackFrom.toInt());
				const auto occupiedHex = enemy->occupiedHex(attackFrom);
				if(occupiedHex.isValid())
					footprint.set(occupiedHex.toInt());

				BattleAttackInfo attackInfo(enemy, target, static_cast<int>(reachability.distances.at(attackFrom.toInt())), false);
				attackInfo.attackerPos = attackFrom;
				attackInfo.defenderPos = targetPosition;
				const auto damage = ExpectedDamage(battle->battleEstimateDamage(attackInfo).damage, target);
				const auto expectedDamage = damage * survivingCountTwice / (static_cast<int64_t>(enemy->getCount()) * 2);
				const auto existing = std::ranges::find_if(
					result,
					[&footprint](const auto & option)
					{
						return option.footprint == footprint;
					}
				);
				if(existing == result.end())
					result.push_back({footprint, expectedDamage});
				else
					existing->expectedDamage = std::max(existing->expectedDamage, expectedDamage);
			}
		}
		return result;
	}

	MeleeSelection SelectMeleeThreats(const std::vector<MeleeThreat> & threats)
	{
		MeleeSelection best{.damageByThreat = std::vector<int64_t>(threats.size(), 0)};
		std::vector<int64_t> selectedDamage(threats.size(), 0);
		std::function<void(size_t, std::bitset<GameConstants::BFIELD_SIZE>, int, int64_t)> select;
		select = [&](size_t index, std::bitset<GameConstants::BFIELD_SIZE> occupiedHexes, int attackerCount, int64_t expectedDamage)
		{
			if(index == threats.size())
			{
				if(expectedDamage > best.expectedDamage || (expectedDamage == best.expectedDamage && attackerCount > best.attackerCount))
				{
					best.attackerCount = attackerCount;
					best.expectedDamage = expectedDamage;
					best.damageByThreat = selectedDamage;
				}
				return;
			}

			selectedDamage.at(index) = 0;
			select(index + 1, occupiedHexes, attackerCount, expectedDamage);
			for(const auto & option : threats.at(index).attackOptions)
			{
				if((occupiedHexes & option.footprint).any())
					continue;

				selectedDamage.at(index) = option.expectedDamage;
				select(index + 1, occupiedHexes | option.footprint, attackerCount + 1, expectedDamage + option.expectedDamage);
				selectedDamage.at(index) = 0;
			}
		};
		select(0, {}, 0, 0);
		return best;
	}
}

HARBot::HARBot(const std::string & fallback) : fallback(fallback), fallbackBot(AIFactory::createBattleAI(fallback))
{
	logAi->debug("HARBot: constructed with %s fallback", fallback);
}

void HARBot::initBattleInterface(std::shared_ptr<Environment> env, std::shared_ptr<CBattleCallback> cb_, AICombatOptions aiCombatOptions)
{
	cb = cb_;
	colorName = cb->getPlayerID()->toString();
	logAi->info("HARBot [%s]: initializing battle interface (spells=%d)", colorName, aiCombatOptions.enableSpellsUsage);
	fallbackBot->initBattleInterface(env, cb, aiCombatOptions);
}

void HARBot::battleStart(
	const BattleID & battleID,
	const CCreatureSet * army1,
	const CCreatureSet * army2,
	int3 tile,
	const CGHeroInstance * hero1,
	const CGHeroInstance * hero2,
	BattleSide side,
	bool replayAllowed
)
{
	battle = cb->getBattle(battleID);
	fastbfs = std::make_unique<const FastBFS>(*battle, battle->getAccessibility());
	primaryStack = nullptr;
	int64_t primaryValue = -1;
	for(const auto * stack : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE))
	{
		if(!stack->alive() || IsWarMachine(stack) || stack->unitType()->getGrowth() <= 0)
			continue;

		const auto value = StackValue(stack);
		logAi->debug("HARBot [%s]: primary candidate %s has AI value %lld", colorName, stack->getDescription(), value);
		if(value > primaryValue)
		{
			primaryStack = stack;
			primaryValue = value;
		}
	}
	mustRetreat.clear();
	consecutiveRetreats.clear();
	logAi->info(
		"HARBot [%s]: battle started as side=%d with %d friendly and %d enemy stacks",
		colorName,
		static_cast<int>(side),
		static_cast<int>(battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE).size()),
		static_cast<int>(battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY).size())
	);
	if(primaryStack)
		logAi->info("HARBot [%s]: selected primary stack %s with AI value %lld", colorName, primaryStack->getDescription(), primaryValue);
	else
		logAi->warn("HARBot [%s]: no regular primary stack found; all stack actions will use %s", colorName, fallback);
	fallbackBot->battleStart(battleID, army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void HARBot::yourTacticPhase(const BattleID & battleID, int distance)
{
	logAi->info("HARBot [%s]: delegating tactics phase (distance=%d) to BattleAI", colorName, distance);
	fallbackBot->yourTacticPhase(battleID, distance);
}

void HARBot::battleNewRound(const BattleID & battleID)
{
	logAi->info(
		"HARBot [%s]: starting round %d (%d stacks marked to retreat)", colorName, battle ? battle->battleGetRound() : -1, static_cast<int>(mustRetreat.size())
	);
}

void HARBot::actionStarted(const BattleID & battleID, const BattleAction & action)
{
	logAi->debug("HARBot [%s]: action started: %s", colorName, action.toString());
}

void HARBot::activeStack(const BattleID & battleID, const CStack * stack)
{
	logAi->info(
		"HARBot [%s]: active stack %s at hex %d (round=%d, speed=%d, waited=%d, willMove=%d, retreatPending=%d)",
		colorName,
		stack->getDescription(),
		stack->getPosition().toInt(),
		battle ? battle->battleGetRound() : -1,
		stack->getMovementRange(),
		stack->waitedThisTurn,
		stack->willMove(),
		mustRetreat.contains(stack)
	);

	if(!battle)
	{
		delegate(battleID, stack, "battle callback is unavailable");
		return;
	}

	fastbfs = std::make_unique<const FastBFS>(*battle, battle->getAccessibility());
	retreatExposureCache.clear();

	if(stack != primaryStack)
	{
		delegate(battleID, stack, primaryStack ? "stack is not HARBot's primary army stack" : "HARBot has no regular primary army stack");
		return;
	}

	if(!stack->willMove())
	{
		delegate(battleID, stack, "stack cannot perform normal movement");
		return;
	}

	if(IsWarMachine(stack))
	{
		delegate(battleID, stack, "stack is a siege weapon");
		return;
	}

	if(stack->getMovementRange() == 0)
	{
		delegate(battleID, stack, "stack has zero movement range");
		return;
	}

	bool actAsAlreadyRetreated = false;
	if(handlePendingRetreat(battleID, stack, actAsAlreadyRetreated))
		return;
	if(handleFollowUpRetreat(battleID, stack, actAsAlreadyRetreated))
		return;
	if(handlePreWaitTurn(battleID, stack))
		return;

	logAi->debug("HARBot [%s]: %s has already waited; searching for a melee attack", colorName, stack->getDescription());
	if(attackAndMarkForRetreat(battleID, stack, {}))
		return;

	logAi->info("HARBot [%s]: %s has no reachable melee target and will advance", colorName, stack->getDescription());
	if(advanceTowardsEnemy(battleID, stack))
		return;

	delegate(battleID, stack, "no legal advance toward an enemy was found");
}

bool HARBot::handlePendingRetreat(const BattleID & battleID, const CStack * stack, bool & actAsAlreadyRetreated)
{
	if(mustRetreat.erase(stack) == 0)
		return false;

	logAi->info("HARBot [%s]: %s is due to retreat after its previous attack", colorName, stack->getDescription());
	if(!canEnemyThreatenThisRound(stack))
	{
		logAi->info("HARBot [%s]: no non-shooter enemy acting later this round can threaten %s; skipping retreat", colorName, stack->getDescription());
		consecutiveRetreats[stack] = 1;
		actAsAlreadyRetreated = true;
		return false;
	}

	const auto result = retreat(battleID, stack);
	if(result == RetreatResult::MOVED)
		consecutiveRetreats[stack] = 1;
	else if(result == RetreatResult::ATTACKED)
	{
		consecutiveRetreats.erase(stack);
		logAi->info("HARBot [%s]: attacking during retreat resets the consecutive retreat counter for %s", colorName, stack->getDescription());
	}
	else
	{
		attackImmediatelyOrDelegate(
			battleID, stack, result == RetreatResult::NOT_VIABLE ? "retreat is no longer viable" : "no legal retreat destination was found"
		);
	}
	return true;
}

bool HARBot::handleFollowUpRetreat(const BattleID & battleID, const CStack * stack, bool actAsAlreadyRetreated)
{
	const auto retreatIt = consecutiveRetreats.find(stack);
	const auto retreatCount = retreatIt == consecutiveRetreats.end() ? 0 : retreatIt->second;
	if(!actAsAlreadyRetreated && retreatCount > 0 && retreatCount < MAX_CONSECUTIVE_RETREATS && canEnemyReachNextTurn(stack))
	{
		logAi->info("HARBot [%s]: %s is reachable after %d consecutive retreat(s) and will retreat again", colorName, stack->getDescription(), retreatCount);
		const auto result = retreat(battleID, stack);
		if(result == RetreatResult::MOVED)
			consecutiveRetreats[stack] = retreatCount + 1;
		else if(result == RetreatResult::ATTACKED)
		{
			consecutiveRetreats.erase(stack);
			logAi->info("HARBot [%s]: attacking during follow-up retreat resets the consecutive retreat counter for %s", colorName, stack->getDescription());
		}
		else
		{
			attackImmediatelyOrDelegate(
				battleID,
				stack,
				result == RetreatResult::NOT_VIABLE ? "follow-up retreat is no longer viable" : "no legal follow-up retreat destination was found"
			);
		}
		return true;
	}

	if(retreatCount >= MAX_CONSECUTIVE_RETREATS)
	{
		logAi->info("HARBot [%s]: %s reached the limit of %d consecutive retreats", colorName, stack->getDescription(), retreatCount);
		consecutiveRetreats.erase(stack);
		if(attackAndMarkForRetreat(battleID, stack, {.force = true, .requireNoExposureIncrease = true}))
			return true;

		logAi->info("HARBot [%s]: no immediate attack avoids increasing exposure for %s; waiting instead", colorName, stack->getDescription());
		const auto action = stack->waitedThisTurn ? BattleAction::makeDefend(stack) : BattleAction::makeWait(stack);
		cb->battleMakeUnitAction(battleID, action);
		return true;
	}

	if(retreatCount > 0)
		logAi->info("HARBot [%s]: %s is no longer threatened next turn and resumes its attack cycle", colorName, stack->getDescription());
	return false;
}

bool HARBot::handlePreWaitTurn(const BattleID & battleID, const CStack * stack)
{
	if(stack->waitedThisTurn)
		return false;

	consecutiveRetreats.erase(stack);
	const auto waiting = calculateExposure(stack, stack->getPosition(), {.currentRoundOnly = true});
	logAi->info(
		"HARBot [%s]: current-round pre-wait exposure for %s is %lld/%lld expected HP damage (%.1f%% of remaining health)",
		colorName,
		stack->getDescription(),
		waiting.expectedDamage,
		waiting.remainingHealth,
		waiting.remainingHealth > 0 ? 100.0 * static_cast<double>(waiting.expectedDamage) / static_cast<double>(waiting.remainingHealth) : 0.0
	);
	if(waiting.expectedDamage > 0)
	{
		const auto retreatPlan = findBestRetreatFrom(stack, stack->getPosition(), {.currentRoundOnly = true});
		int64_t bestAttackExposure = std::numeric_limits<int64_t>::max();
		const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
		const auto distances = battle->battleGetDistances(stack, stack->getPosition());
		for(const auto * enemy : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY))
		{
			if(!enemy->alive() || enemy->isInvincible() || IsWarMachine(enemy))
				continue;

			for(const auto & hex : availableHexes)
			{
				if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
					continue;

				BattleAttackInfo attackInfo(stack, enemy, static_cast<int>(distances.at(hex.toInt())), false);
				attackInfo.attackerPos = hex;
				attackInfo.defenderPos = enemy->getPosition();
				const auto attack = battle->battleEstimateDamage(attackInfo);
				const auto exposure = calculateExposure(
					stack,
					hex,
					{.currentRoundOnly = true, .logDetails = false, .attackedEnemy = enemy, .expectedKillsTwice = attack.kills.min + attack.kills.max}
				);
				bestAttackExposure = std::min(bestAttackExposure, exposure.expectedDamage);
			}
		}

		const bool retreatIsViable = retreatPlan.destination.isValid() && waiting.remainingHealth > 0
								  && retreatPlan.exposedEnemyValue * RETREAT_EXPOSURE_DENOMINATOR <= waiting.remainingHealth * RETREAT_EXPOSURE_NUMERATOR;
		if(retreatIsViable && retreatPlan.exposedEnemyValue < waiting.expectedDamage && retreatPlan.exposedEnemyValue < bestAttackExposure)
		{
			logAi->info(
				"HARBot [%s]: %s makes a standalone retreat to hex %d before waiting (retreatExposure=%lld, attackExposure=%lld, waitingExposure=%lld)",
				colorName,
				stack->getDescription(),
				retreatPlan.destination.toInt(),
				retreatPlan.exposedEnemyValue,
				bestAttackExposure,
				waiting.expectedDamage
			);
			consecutiveRetreats[stack] = 1;
			cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, retreatPlan.destination));
			return true;
		}

		logAi->info(
			"HARBot [%s]: %s is exposed to enemies still acting this round; searching for an immediate attack that does not increase exposure "
			"(bestRetreatExposure=%lld, bestAttackExposure=%lld)",
			colorName,
			stack->getDescription(),
			retreatPlan.exposedEnemyValue,
			bestAttackExposure
		);
		if(attackAndMarkForRetreat(battleID, stack, {.force = true, .requireNoExposureIncrease = true, .currentRoundExposureOnly = true}))
			return true;

		logAi->info(
			"HARBot [%s]: no immediate attack avoids increasing current-round exposure for %s; preserving the normal wait", colorName, stack->getDescription()
		);
	}

	logAi->info("HARBot [%s]: %s waits so it can attack late in the round", colorName, stack->getDescription());
	cb->battleMakeUnitAction(battleID, BattleAction::makeWait(stack));
	return true;
}

void HARBot::attackImmediatelyOrDelegate(const BattleID & battleID, const CStack * stack, const std::string & reason)
{
	consecutiveRetreats.erase(stack);
	logAi->info("HARBot [%s]: %s; %s will attack immediately", colorName, reason, stack->getDescription());
	if(!attackAndMarkForRetreat(battleID, stack, {.force = true}))
		delegate(battleID, stack, "immediate attack requested, but no legal melee attack was found");
}

HARBot::RetreatPlan HARBot::findBestRetreatFrom(const CStack * stack, const BattleHex & assumedPosition, const ExposureOptions & exposureOptions) const
{
	assert(fastbfs);
	const auto distances = fastbfs->run(
		stack->getPosition(),
		assumedPosition,
		stack->unitSide(),
		stack->hasBonusOfType(BonusType::FLYING),
		stack->doubleWide(),
		static_cast<int>(stack->getMovementRange())
	);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	const auto accessibility = battle->getAccessibility();
	RetreatPlan best;

	for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
	{
		const BattleHex destination(static_cast<si16>(i));
		const auto movementDistance = static_cast<int>(distances.at(i));
		if(movementDistance == FastBFS::INFINITE_DIST || movementDistance > stack->getMovementRange() || destination == assumedPosition)
			continue;

		int minimumEnemyDistance = std::numeric_limits<int>::max();
		int totalEnemyDistance = 0;
		for(const auto * enemy : enemies)
		{
			if(!enemy->alive() || IsWarMachine(enemy) || enemy->isShooter() || !enemy->getPosition().isValid())
				continue;

			const auto distance = static_cast<int>(BattleHex::getDistance(destination, enemy->getPosition()));
			minimumEnemyDistance = std::min(minimumEnemyDistance, distance);
			totalEnemyDistance += distance;
		}

		if(minimumEnemyDistance == std::numeric_limits<int>::max())
			continue;

		int surroundingHexCount = 0;
		for(const auto & neighbour : destination.getNeighbouringTiles())
		{
			const auto neighbourAccessibility = accessibility.at(neighbour.toInt());
			if(neighbourAccessibility == EAccessibility::ACCESSIBLE || neighbourAccessibility == EAccessibility::ALIVE_STACK)
				++surroundingHexCount;
		}

		int64_t exposedEnemyValue;
		if(exposureOptions.currentRoundOnly || exposureOptions.attackedEnemy)
		{
			auto options = exposureOptions;
			options.logDetails = false;
			exposedEnemyValue = calculateExposure(stack, destination, options).expectedDamage;
		}
		else
		{
			auto exposureIt = retreatExposureCache.find(i);
			if(exposureIt == retreatExposureCache.end())
			{
				const auto exposure = calculateExposure(stack, destination, {.logDetails = false});
				exposureIt = retreatExposureCache.emplace(i, exposure.expectedDamage).first;
			}
			exposedEnemyValue = exposureIt->second;
		}
		const CStack * attackTarget = nullptr;
		int64_t expectedAttackValue = -1;
		int64_t expectedRetaliationValue = std::numeric_limits<int64_t>::max();
		for(const auto * enemy : enemies)
		{
			if(!enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible() || !enemy->getPosition().isValid()
			   || !CStack::isMeleeAttackPossible(stack, enemy, destination))
				continue;

			BattleAttackInfo attackInfo(stack, enemy, movementDistance, false);
			attackInfo.attackerPos = destination;
			attackInfo.defenderPos = enemy->getPosition();
			DamageEstimation retaliation;
			const auto attack = battle->battleEstimateDamage(attackInfo, &retaliation);
			const auto attackValue = ExpectedDamageValue(attack.damage, enemy);
			const auto retaliationValue = ExpectedDamageValue(retaliation.damage, stack);
			if(!attackTarget || attackValue > expectedAttackValue || (attackValue == expectedAttackValue && retaliationValue < expectedRetaliationValue))
			{
				attackTarget = enemy;
				expectedAttackValue = attackValue;
				expectedRetaliationValue = retaliationValue;
			}
		}

		const auto tieBreakScore =
			std::tuple(expectedAttackValue, -expectedRetaliationValue, -surroundingHexCount, minimumEnemyDistance, totalEnemyDistance, -movementDistance);
		const auto bestTieBreakScore = std::tuple(
			best.expectedAttackValue,
			-best.expectedRetaliationValue,
			-best.surroundingHexCount,
			best.minimumEnemyDistance,
			best.totalEnemyDistance,
			-best.movementDistance
		);
		if(!best.destination.isValid() || exposedEnemyValue < best.exposedEnemyValue
		   || (exposedEnemyValue == best.exposedEnemyValue && tieBreakScore > bestTieBreakScore))
		{
			best.destination = destination;
			best.attackTarget = attackTarget;
			best.expectedAttackValue = expectedAttackValue;
			best.expectedRetaliationValue = expectedRetaliationValue;
			best.exposedEnemyValue = exposedEnemyValue;
			best.surroundingHexCount = surroundingHexCount;
			best.minimumEnemyDistance = minimumEnemyDistance;
			best.totalEnemyDistance = totalEnemyDistance;
			best.movementDistance = movementDistance;
		}
	}

	return best;
}

bool HARBot::isImmediatelyThreatenedAt(const CStack * stack, const BattleHex & destination) const
{
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	const auto totalEnemyValue = TotalEnemyValue(enemies);

	for(const auto * enemy : enemies)
	{
		if(!enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible() || enemy->isShooter() || !enemy->getPosition().isValid())
			continue;

		const auto enemyValue = StackValue(enemy);
		if(totalEnemyValue <= 0 || enemyValue * SIGNIFICANT_ENEMY_VALUE_DENOMINATOR < totalEnemyValue)
			continue;

		const auto enemyAvailableHexes = battle->battleGetAvailableHexes(enemy, false);
		bool canAttack = battle->battleCanAttackHex(enemyAvailableHexes, enemy, destination);
		const auto occupiedHex = stack->occupiedHex(destination);
		if(stack->doubleWide() && occupiedHex.isValid())
			canAttack |= battle->battleCanAttackHex(enemyAvailableHexes, enemy, occupiedHex);

		logAi->debug(
			"HARBot [%s]: immediate retreat threat enemy=%s value=%lld/%lld (%.1f%%) canAttack=%d destination=%d",
			colorName,
			enemy->getDescription(),
			enemyValue,
			totalEnemyValue,
			100.0 * static_cast<double>(enemyValue) / static_cast<double>(totalEnemyValue),
			canAttack,
			destination.toInt()
		);
		if(canAttack)
			return true;
	}

	return false;
}

HARBot::ExposureEstimate HARBot::calculateExposure(const CStack * stack, const BattleHex & destination, const ExposureOptions & options) const
{
	const auto currentRoundOnly = options.currentRoundOnly;
	const auto logDetails = options.logDetails;
	const auto * attackedEnemy = options.attackedEnemy;
	const auto expectedKillsTwice = options.expectedKillsTwice;
	const auto turnScope = GetExposureTurnScope(battle, stack);
	const auto isRelevant = [&](const CStack * enemy)
	{
		return enemy->alive() && !IsWarMachine(enemy) && !enemy->isInvincible() && enemy->getPosition().isValid()
			&& (!currentRoundOnly || turnScope.thisRound.contains(enemy));
	};

	int64_t rangedDamage = 0;
	const int64_t totalHealth = stack->getAvailableHealth();
	int rangedAttackerCount = 0;
	std::vector<MeleeThreat> meleeThreats;
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	BattleHexArray vacatedHexes;
	for(const auto & hex : stack->getHexes())
		vacatedHexes.checkAndPush(hex);
	for(const auto * enemy : enemies)
	{
		if(!turnScope.beforeNextTurn.contains(enemy))
			continue;
		for(const auto & hex : enemy->getHexes())
			vacatedHexes.checkAndPush(hex);
	}
	for(const auto * enemy : enemies)
	{
		if(!isRelevant(enemy))
			continue;

		const int64_t survivingCountTwice = enemy == attackedEnemy ? std::max<int64_t>(0, (static_cast<int64_t>(enemy->getCount()) * 2) - expectedKillsTwice)
																   : static_cast<int64_t>(enemy->getCount()) * 2;
		if(survivingCountTwice == 0)
			continue;
		const auto adjustForCasualties = [enemy, survivingCountTwice](int64_t damage)
		{
			return damage * survivingCountTwice / (static_cast<int64_t>(enemy->getCount()) * 2);
		};

		const bool canShoot = enemy->isShooter() && battle->battleCanShoot(enemy, destination);
		if(canShoot)
		{
			BattleAttackInfo attackInfo(enemy, stack, 0, true);
			attackInfo.attackerPos = enemy->getPosition();
			attackInfo.defenderPos = destination;
			const auto expectedDamage = adjustForCasualties(ExpectedDamage(battle->battleEstimateDamage(attackInfo).damage, stack));
			rangedDamage += expectedDamage;
			++rangedAttackerCount;
			if(logDetails)
			{
				logAi->debug(
					"HARBot [%s]: %s exposure enemy=%s mode=ranged expectedDamage=%lld (%.1f%% of HAR health) destination=%d",
					colorName,
					currentRoundOnly ? "current-round" : "all-enemy",
					enemy->getDescription(),
					expectedDamage,
					totalHealth > 0 ? 100.0 * static_cast<double>(expectedDamage) / static_cast<double>(totalHealth) : 0.0,
					destination.toInt()
				);
			}
			continue;
		}

		auto attackOptions = GetMeleeAttackOptions(battle, enemy, stack, destination, vacatedHexes, survivingCountTwice);

		if(logDetails)
		{
			logAi->debug(
				"HARBot [%s]: %s exposure enemy=%s mode=melee candidateFootprints=%d doubleWide=%d actsBeforeNextTurn=%d destination=%d",
				colorName,
				currentRoundOnly ? "current-round" : "all-enemy",
				enemy->getDescription(),
				static_cast<int>(attackOptions.size()),
				enemy->doubleWide(),
				turnScope.beforeNextTurn.contains(enemy),
				destination.toInt()
			);
		}

		if(!attackOptions.empty())
			meleeThreats.push_back({enemy, std::move(attackOptions)});
	}

	const auto meleeSelection = SelectMeleeThreats(meleeThreats);

	if(logDetails)
	{
		for(size_t index = 0; index < meleeThreats.size(); ++index)
		{
			logAi->debug(
				"HARBot [%s]: %s exposure enemy=%s mode=melee selected=%d expectedDamage=%lld destination=%d",
				colorName,
				currentRoundOnly ? "current-round" : "all-enemy",
				meleeThreats.at(index).enemy->getDescription(),
				meleeSelection.damageByThreat.at(index) > 0,
				meleeSelection.damageByThreat.at(index),
				destination.toInt()
			);
		}
		logAi->debug(
			"HARBot [%s]: %s exposure destination=%d rangedAttackers=%d meleeCandidates=%d selectedMeleeAttackers=%d expectedDamage=%lld/%lld HAR health",
			colorName,
			currentRoundOnly ? "current-round" : "all-enemy",
			destination.toInt(),
			rangedAttackerCount,
			static_cast<int>(meleeThreats.size()),
			meleeSelection.attackerCount,
			std::min(rangedDamage + meleeSelection.expectedDamage, totalHealth),
			totalHealth
		);
	}

	return {.expectedDamage = std::min(rangedDamage + meleeSelection.expectedDamage, totalHealth), .remainingHealth = totalHealth};
}

bool HARBot::canEnemyThreatenThisRound(const CStack * stack) const
{
	assert(fastbfs);
	std::vector<battle::Units> turnOrder;
	battle->battleGetTurnOrder(turnOrder, std::numeric_limits<size_t>::max(), 1);

	for(const auto & turn : turnOrder)
	{
		for(const auto * enemy : turn)
		{
			if(enemy == stack || enemy->unitSide() == stack->unitSide() || !enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible() || enemy->isShooter()
			   || !enemy->getPosition().isValid() || enemy->getMovementRange() == 0)
				continue;

			const auto distances = fastbfs->run(
				enemy->getPosition(),
				enemy->getPosition(),
				enemy->unitSide(),
				enemy->hasBonusOfType(BonusType::FLYING),
				enemy->doubleWide(),
				static_cast<int>(enemy->getMovementRange())
			);
			auto attackDistance = FastBFS::INFINITE_DIST;
			for(const auto & attackHex : stack->getAttackableHexes(enemy))
				if(attackHex.isValid())
					attackDistance = std::min(attackDistance, distances.at(attackHex.toInt()));

			logAi->debug(
				"HARBot [%s]: current-round threat check enemy=%s attackDistance=%u speed=%u target=%s",
				colorName,
				enemy->getDescription(),
				static_cast<unsigned>(attackDistance),
				enemy->getMovementRange(),
				stack->getDescription()
			);
			if(attackDistance <= enemy->getMovementRange())
			{
				logAi->info("HARBot [%s]: %s can threaten %s later this round", colorName, enemy->getDescription(), stack->getDescription());
				return true;
			}
		}
	}

	return false;
}

bool HARBot::canEnemyReachNextTurn(const CStack * stack) const
{
	assert(fastbfs);
	for(const auto * enemy : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY))
	{
		if(!enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible() || !enemy->getPosition().isValid() || enemy->getMovementRange() == 0)
			continue;

		if(enemy->isShooter())
		{
			logAi->debug(
				"HARBot [%s]: ignoring shooter %s when checking whether %s should retreat again", colorName, enemy->getDescription(), stack->getDescription()
			);
			continue;
		}

		const auto distances = fastbfs->run(
			enemy->getPosition(),
			enemy->getPosition(),
			enemy->unitSide(),
			enemy->hasBonusOfType(BonusType::FLYING),
			enemy->doubleWide(),
			static_cast<int>(enemy->getMovementRange())
		);

		auto attackDistance = FastBFS::INFINITE_DIST;
		for(const auto & attackHex : stack->getAttackableHexes(enemy))
		{
			if(attackHex.isValid())
				attackDistance = std::min(attackDistance, distances.at(attackHex.toInt()));
		}

		logAi->debug(
			"HARBot [%s]: chase check enemy=%s attackDistance=%u speed=%u target=%s",
			colorName,
			enemy->getDescription(),
			static_cast<unsigned>(attackDistance),
			enemy->getMovementRange(),
			stack->getDescription()
		);
		if(attackDistance <= enemy->getMovementRange())
		{
			logAi->info("HARBot [%s]: %s can reach %s next turn", colorName, enemy->getDescription(), stack->getDescription());
			return true;
		}
	}

	return false;
}

bool HARBot::attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack, const AttackOptions & options)
{
	const auto forceAttack = options.force;
	const auto requireNoExposureIncrease = options.requireNoExposureIncrease;
	const auto currentRoundExposureOnly = options.currentRoundExposureOnly;
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto distances = battle->battleGetDistances(stack, stack->getPosition());
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	const auto totalEnemyValue = TotalEnemyValue(enemies);
	logAi->debug(
		"HARBot [%s]: evaluating attacks for %s from %d available hexes against %d enemies",
		colorName,
		stack->getDescription(),
		static_cast<int>(availableHexes.size()),
		static_cast<int>(enemies.size())
	);
	const bool shouldRetreat = std::ranges::any_of(
		enemies,
		[](const CStack * enemy)
		{
			return enemy->alive() && !IsWarMachine(enemy) && !enemy->isShooter() && enemy->getPosition().isValid();
		}
	);
	const bool shouldPlanRetreat = shouldRetreat && !forceAttack;
	const bool preferLowerExposure = forceAttack || requireNoExposureIncrease;
	const auto waitingExposure =
		requireNoExposureIncrease ? calculateExposure(stack, stack->getPosition(), {.currentRoundOnly = currentRoundExposureOnly}).expectedDamage : int64_t{-1};
	AttackCandidate best;

	for(const auto * enemy : enemies)
	{
		if(!enemy->alive())
		{
			logAi->debug("HARBot [%s]: skipping dead target %s", colorName, enemy->getDescription());
			continue;
		}

		if(enemy->isInvincible())
		{
			logAi->debug("HARBot [%s]: skipping invincible target %s", colorName, enemy->getDescription());
			continue;
		}

		if(IsWarMachine(enemy))
		{
			logAi->debug("HARBot [%s]: skipping war machine target %s", colorName, enemy->getDescription());
			continue;
		}

		int attackHexCount = 0;
		for(const auto & hex : availableHexes)
		{
			if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
				continue;

			++attackHexCount;
			const auto candidate = evaluateAttackCandidate(
				stack, enemy, hex, static_cast<int>(distances.at(hex.toInt())), preferLowerExposure, shouldPlanRetreat, currentRoundExposureOnly
			);
			const auto & retreatPlan = candidate.retreat;
			logAi->debug(
				"HARBot [%s]: attack candidate target=%s shooter=%d baseValue=%lld (%.1f%%) priorityValue=%lld fromHex=%d forceAttack=%d "
				"preferLowerExposure=%d requireNoExposureIncrease=%d expectedKills=%.1f attackExposure=%lld waitingExposure=%lld shouldPlanRetreat=%d "
				"retreatSafe=%d retreatHex=%d retreatExposure=%lld retreatAttackTarget=%s surroundingHexes=%d nearestDistance=%d totalDistance=%d "
				"retreatMoveDistance=%d",
				colorName,
				enemy->getDescription(),
				enemy->isShooter(),
				candidate.targetBaseValue,
				totalEnemyValue > 0 ? 100.0 * static_cast<double>(candidate.targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
				candidate.targetValue,
				hex.toInt(),
				forceAttack,
				preferLowerExposure,
				requireNoExposureIncrease,
				static_cast<double>(candidate.expectedKillsTwice) / 2.0,
				candidate.attackExposure,
				waitingExposure,
				shouldPlanRetreat,
				candidate.retreatIsSafe,
				retreatPlan.destination.toInt(),
				retreatPlan.exposedEnemyValue,
				retreatPlan.attackTarget ? retreatPlan.attackTarget->getDescription() : "none",
				retreatPlan.surroundingHexCount,
				retreatPlan.minimumEnemyDistance,
				retreatPlan.totalEnemyDistance,
				retreatPlan.movementDistance
			);

			if(shouldPlanRetreat && !retreatPlan.destination.isValid())
				continue;

			if(isBetterAttackCandidate(candidate, best, preferLowerExposure, shouldPlanRetreat))
			{
				logAi->debug("HARBot [%s]: candidate becomes the current best attack/retreat plan", colorName);
				best = candidate;
			}
		}

		if(attackHexCount == 0)
			logAi->debug("HARBot [%s]: target %s is not reachable in melee", colorName, enemy->getDescription());
	}

	if(!best.target)
	{
		logAi->debug("HARBot [%s]: no legal melee attack found for %s", colorName, stack->getDescription());
		return false;
	}

	if(requireNoExposureIncrease && best.attackExposure > waitingExposure)
	{
		logAi->debug(
			"HARBot [%s]: best immediate attack target=%s fromHex=%d is rejected because %s exposure=%lld is above waiting exposure=%lld",
			colorName,
			best.target->getDescription(),
			best.attackHex.toInt(),
			currentRoundExposureOnly ? "current-round" : "all-enemy",
			best.attackExposure,
			waitingExposure
		);
		return false;
	}

	if(shouldPlanRetreat)
	{
		logAi->info(
			"HARBot [%s]: %s attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld), planning to retreat toward hex %d (exposure=%lld, "
			"retreatAttackTarget=%s, immediatelySafe=%d, nearest non-shooter distance=%d, moveDistance=%d)",
			colorName,
			stack->getDescription(),
			best.target->getDescription(),
			best.attackHex.toInt(),
			best.targetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			best.targetValue,
			best.retreat.destination.toInt(),
			best.retreat.exposedEnemyValue,
			best.retreat.attackTarget ? best.retreat.attackTarget->getDescription() : "none",
			best.retreatIsSafe,
			best.retreat.minimumEnemyDistance,
			best.retreat.movementDistance
		);
		mustRetreat.insert(stack);
	}
	else if(forceAttack)
	{
		logAi->info(
			"HARBot [%s]: %s immediately attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld, exposure=%lld vs waiting=%lld) without requiring "
			"a viable retreat plan",
			colorName,
			stack->getDescription(),
			best.target->getDescription(),
			best.attackHex.toInt(),
			best.targetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			best.targetValue,
			best.attackExposure,
			waitingExposure
		);
		if(shouldRetreat)
			mustRetreat.insert(stack);
		else
			consecutiveRetreats.erase(stack);
	}
	else
	{
		logAi->info(
			"HARBot [%s]: %s attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld) without retreating because no living non-shooter enemies "
			"remain",
			colorName,
			stack->getDescription(),
			best.target->getDescription(),
			best.attackHex.toInt(),
			best.targetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			best.targetValue
		);
		consecutiveRetreats.erase(stack);
	}
	cb->battleMakeUnitAction(battleID, BattleAction::makeMeleeAttack(stack, best.target, best.attackHex));
	return true;
}

HARBot::AttackCandidate HARBot::evaluateAttackCandidate(
	const CStack * stack,
	const CStack * enemy,
	const BattleHex & attackHex,
	int movementDistance,
	bool preferLowerExposure,
	bool shouldPlanRetreat,
	bool currentRoundExposureOnly
) const
{
	BattleAttackInfo attackInfo(stack, enemy, movementDistance, false);
	attackInfo.attackerPos = attackHex;
	attackInfo.defenderPos = enemy->getPosition();
	const auto attack = battle->battleEstimateDamage(attackInfo);
	const auto expectedKillsTwice = attack.kills.min + attack.kills.max;
	const auto attackExposure =
		preferLowerExposure
			? calculateExposure(
				  stack,
				  attackHex,
				  {.currentRoundOnly = currentRoundExposureOnly, .logDetails = false, .attackedEnemy = enemy, .expectedKillsTwice = expectedKillsTwice}
			  )
				  .expectedDamage
			: int64_t{-1};
	const auto retreatPlan =
		shouldPlanRetreat ? findBestRetreatFrom(stack, attackHex, {.attackedEnemy = enemy, .expectedKillsTwice = expectedKillsTwice}) : RetreatPlan{};
	const auto targetBaseValue = StackValue(enemy);
	return {
		.target = enemy,
		.attackHex = attackHex,
		.retreat = retreatPlan,
		.retreatIsSafe = shouldPlanRetreat && retreatPlan.destination.isValid() && !isImmediatelyThreatenedAt(stack, retreatPlan.destination),
		.targetBaseValue = targetBaseValue,
		.targetValue = enemy->isShooter() ? targetBaseValue * SHOOTER_PRIORITY_MULTIPLIER : targetBaseValue,
		.attackExposure = attackExposure,
		.expectedKillsTwice = expectedKillsTwice
	};
}

bool HARBot::isBetterAttackCandidate(const AttackCandidate & candidate, const AttackCandidate & best, bool preferLowerExposure, bool shouldPlanRetreat) const
{
	if(!best.target)
		return true;
	if(preferLowerExposure && candidate.attackExposure != best.attackExposure)
		return candidate.attackExposure < best.attackExposure;
	if(shouldPlanRetreat && candidate.retreat.exposedEnemyValue != best.retreat.exposedEnemyValue)
		return candidate.retreat.exposedEnemyValue < best.retreat.exposedEnemyValue;
	if(shouldPlanRetreat && candidate.retreat.expectedAttackValue != best.retreat.expectedAttackValue)
		return candidate.retreat.expectedAttackValue > best.retreat.expectedAttackValue;
	if(shouldPlanRetreat && candidate.retreat.expectedRetaliationValue != best.retreat.expectedRetaliationValue)
		return candidate.retreat.expectedRetaliationValue < best.retreat.expectedRetaliationValue;
	if(!shouldPlanRetreat)
		return candidate.targetValue > best.targetValue;
	if(candidate.retreatIsSafe != best.retreatIsSafe)
		return candidate.retreatIsSafe;

	if(candidate.retreatIsSafe)
	{
		const auto score = std::tuple(
			candidate.targetValue, -candidate.retreat.surroundingHexCount, candidate.retreat.minimumEnemyDistance, candidate.retreat.totalEnemyDistance
		);
		const auto bestScore =
			std::tuple(best.targetValue, -best.retreat.surroundingHexCount, best.retreat.minimumEnemyDistance, best.retreat.totalEnemyDistance);
		return score > bestScore || (score == bestScore && candidate.retreat.movementDistance < best.retreat.movementDistance);
	}

	const auto score =
		std::tuple(-candidate.retreat.surroundingHexCount, candidate.retreat.minimumEnemyDistance, candidate.retreat.totalEnemyDistance, candidate.targetValue);
	const auto bestScore = std::tuple(-best.retreat.surroundingHexCount, best.retreat.minimumEnemyDistance, best.retreat.totalEnemyDistance, best.targetValue);
	return score > bestScore || (score == bestScore && candidate.retreat.movementDistance < best.retreat.movementDistance);
}

bool HARBot::advanceTowardsEnemy(const BattleID & battleID, const CStack * stack)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	const auto totalEnemyValue = TotalEnemyValue(enemies);

	logAi->debug(
		"HARBot [%s]: evaluating %d staging hexes for %s against %d enemies (totalEnemyValue=%lld)",
		colorName,
		static_cast<int>(availableHexes.size()),
		stack->getDescription(),
		static_cast<int>(enemies.size()),
		totalEnemyValue
	);

	StagingCandidate best;
	for(const auto & destination : availableHexes)
	{
		if(stack->coversPos(destination))
			continue;

		const auto threats = assessStagingThreats(stack, destination, enemies, totalEnemyValue);
		if(threats.unsafe)
			continue;

		const auto distances = getDistancesFromStaging(stack, destination);
		BattleHexArray nextTurnAvailableHexes;
		for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
			if(distances.at(i) <= stack->getMovementRange())
				nextTurnAvailableHexes.insert(BattleHex(static_cast<si16>(i)));

		const auto reachable = findReachableEnemies(stack, enemies, distances);
		if(!reachable.target)
		{
			logAi->debug("HARBot [%s]: staging hex %d cannot produce a melee attack next turn", colorName, destination.toInt());
			continue;
		}

		const StagingCandidate candidate{
			.destination = destination,
			.target = reachable.target,
			.threatenedRetreatHexes = countThreatenedRetreatHexes(stack, destination, enemies, nextTurnAvailableHexes),
			.reachableValue = reachable.totalValue,
			.toleratedThreatValue = threats.toleratedValue,
			.minimumEnemyDistance = threats.minimumEnemyDistance,
			.nextAttackDistance = reachable.attackDistance,
			.targetValue = reachable.targetValue
		};
		logAi->debug(
			"HARBot [%s]: staging candidate hex=%d threatenedRetreatHexes=%d reachableValue=%lld (%.1f%%) toleratedThreatValue=%lld (%.1f%%) "
			"nearestEnemyDistance=%d representativeTarget=%s targetValue=%lld (%.1f%%) attackDistance=%u",
			colorName,
			candidate.destination.toInt(),
			candidate.threatenedRetreatHexes,
			candidate.reachableValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(candidate.reachableValue) / static_cast<double>(totalEnemyValue) : 0.0,
			candidate.toleratedThreatValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(candidate.toleratedThreatValue) / static_cast<double>(totalEnemyValue) : 0.0,
			candidate.minimumEnemyDistance,
			candidate.target->getDescription(),
			candidate.targetValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(candidate.targetValue) / static_cast<double>(totalEnemyValue) : 0.0,
			candidate.nextAttackDistance
		);
		if(isBetterStagingCandidate(candidate, best))
			best = candidate;
	}

	if(!best.destination.isValid())
	{
		logAi->info("HARBot [%s]: no safe staging hex enables a melee attack next turn for %s", colorName, stack->getDescription());
		return false;
	}

	logAi->info(
		"HARBot [%s]: %s stages at hex %d threatening %d enemy retreat positions while nearestEnemyDistance=%d; reachableEnemyValue=%lld (%.1f%%), "
		"toleratedThreatValue=%lld (%.1f%%); representative target=%s (value=%lld, %.1f%%, attackDistance=%u)",
		colorName,
		stack->getDescription(),
		best.destination.toInt(),
		best.threatenedRetreatHexes,
		best.minimumEnemyDistance,
		best.reachableValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.reachableValue) / static_cast<double>(totalEnemyValue) : 0.0,
		best.toleratedThreatValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.toleratedThreatValue) / static_cast<double>(totalEnemyValue) : 0.0,
		best.target->getDescription(),
		best.targetValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(best.targetValue) / static_cast<double>(totalEnemyValue) : 0.0,
		best.nextAttackDistance
	);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, best.destination));
	return true;
}

HARBot::StagingThreats HARBot::assessStagingThreats(const CStack * stack, const BattleHex & destination, const TStacks & enemies, int64_t totalEnemyValue) const
{
	StagingThreats result;
	for(const auto * enemy : enemies)
	{
		if(!IsEligibleEnemy(enemy))
			continue;

		result.minimumEnemyDistance = std::min(result.minimumEnemyDistance, static_cast<int>(BattleHex::getDistance(destination, enemy->getPosition())));
		const auto enemyAvailableHexes = battle->battleGetAvailableHexes(enemy, false);
		bool canAttackDestination = battle->battleCanAttackHex(enemyAvailableHexes, enemy, destination);
		const auto occupiedHex = stack->occupiedHex(destination);
		if(stack->doubleWide() && occupiedHex.isValid())
			canAttackDestination |= battle->battleCanAttackHex(enemyAvailableHexes, enemy, occupiedHex);
		if(!canAttackDestination)
			continue;

		const auto value = StackValue(enemy);
		if(totalEnemyValue > 0 && value * SIGNIFICANT_ENEMY_VALUE_DENOMINATOR >= totalEnemyValue)
		{
			logAi->debug(
				"HARBot [%s]: rejecting staging hex %d: %s can reach it and has value %lld/%lld (%.1f%%, at least 10%%)",
				colorName,
				destination.toInt(),
				enemy->getDescription(),
				value,
				totalEnemyValue,
				100.0 * static_cast<double>(value) / static_cast<double>(totalEnemyValue)
			);
			result.unsafe = true;
			return result;
		}
		result.toleratedValue += value;
	}
	return result;
}

FastBFS::Distances HARBot::getDistancesFromStaging(const CStack * stack, const BattleHex & destination) const
{
	assert(fastbfs);
	return fastbfs->run(
		stack->getPosition(),
		destination,
		stack->unitSide(),
		stack->hasBonusOfType(BonusType::FLYING),
		stack->doubleWide(),
		static_cast<int>(stack->getMovementRange())
	);
}

int HARBot::countThreatenedRetreatHexes(
	const CStack * stack,
	const BattleHex & destination,
	const TStacks & enemies,
	const BattleHexArray & availableHexes
) const
{
	int total = 0;
	for(const auto * enemy : enemies)
	{
		if(!IsEligibleEnemy(enemy))
			continue;

		int threatenedForEnemy = 0;
		for(const auto & retreatHex : battle->battleGetAvailableHexes(enemy, false))
		{
			bool threatened = battle->battleCanAttackHex(availableHexes, stack, retreatHex);
			const auto occupiedHex = enemy->occupiedHex(retreatHex);
			if(enemy->doubleWide() && occupiedHex.isValid())
				threatened |= battle->battleCanAttackHex(availableHexes, stack, occupiedHex);
			if(threatened)
			{
				++threatenedForEnemy;
				++total;
			}
		}
		logAi->debug(
			"HARBot [%s]: staging hex %d threatens %d possible positions of %s", colorName, destination.toInt(), threatenedForEnemy, enemy->getDescription()
		);
	}
	return total;
}

HARBot::ReachableEnemies HARBot::findReachableEnemies(const CStack * stack, const TStacks & enemies, const FastBFS::Distances & distances) const
{
	ReachableEnemies result;
	for(const auto * enemy : enemies)
	{
		if(!IsEligibleEnemy(enemy))
			continue;

		auto distanceToAttack = FastBFS::INFINITE_DIST;
		for(const auto & attackHex : enemy->getAttackableHexes(stack))
			if(attackHex.isValid())
				distanceToAttack = std::min(distanceToAttack, distances.at(attackHex.toInt()));
		if(distanceToAttack > stack->getMovementRange())
			continue;

		const auto value = StackValue(enemy);
		result.totalValue += value;
		if(!result.target || value > result.targetValue || (value == result.targetValue && distanceToAttack > result.attackDistance))
		{
			result.target = enemy;
			result.attackDistance = distanceToAttack;
			result.targetValue = value;
		}
	}
	return result;
}

bool HARBot::isBetterStagingCandidate(const StagingCandidate & candidate, const StagingCandidate & best) const
{
	if(!best.destination.isValid())
		return true;
	if(candidate.threatenedRetreatHexes != best.threatenedRetreatHexes)
		return candidate.threatenedRetreatHexes > best.threatenedRetreatHexes;
	if(candidate.minimumEnemyDistance != best.minimumEnemyDistance)
		return candidate.minimumEnemyDistance > best.minimumEnemyDistance;
	if(candidate.toleratedThreatValue != best.toleratedThreatValue)
		return candidate.toleratedThreatValue < best.toleratedThreatValue;
	return std::tie(candidate.reachableValue, candidate.nextAttackDistance, candidate.targetValue)
		 > std::tie(best.reachableValue, best.nextAttackDistance, best.targetValue);
}

HARBot::RetreatResult HARBot::retreat(const BattleID & battleID, const CStack * stack)
{
	logAi->debug("HARBot [%s]: evaluating retreat for %s from hex %d", colorName, stack->getDescription(), stack->getPosition().toInt());
	const auto plan = findBestRetreatFrom(stack, stack->getPosition(), {});

	if(!plan.destination.isValid())
	{
		logAi->info("HARBot [%s]: no legal retreat destination found for %s", colorName, stack->getDescription());
		return RetreatResult::UNAVAILABLE;
	}

	const auto exposure = calculateExposure(stack, plan.destination, {});
	const bool exceedsExposureLimit =
		exposure.remainingHealth > 0 && exposure.expectedDamage * RETREAT_EXPOSURE_DENOMINATOR > exposure.remainingHealth * RETREAT_EXPOSURE_NUMERATOR;
	logAi->info(
		"HARBot [%s]: retreat exposure at hex %d is %lld/%lld expected HP damage (%.1f%% of remaining health, limit=30%%)",
		colorName,
		plan.destination.toInt(),
		exposure.expectedDamage,
		exposure.remainingHealth,
		exposure.remainingHealth > 0 ? 100.0 * static_cast<double>(exposure.expectedDamage) / static_cast<double>(exposure.remainingHealth) : 0.0
	);
	if(exceedsExposureLimit)
	{
		logAi->info(
			"HARBot [%s]: retreat to hex %d is not viable because expected incoming damage to %s is %d%% of its remaining health",
			colorName,
			plan.destination.toInt(),
			stack->getDescription(),
			100 * exposure.expectedDamage / exposure.remainingHealth
		);
		return RetreatResult::NOT_VIABLE;
	}

	if(plan.attackTarget)
	{
		const auto targetBaseValue = StackValue(plan.attackTarget);
		logAi->info(
			"HARBot [%s]: %s retreats by attacking %s from hex %d (exposure=%lld, targetBaseValue=%lld; expectedAttackValue=%lld, "
			"expectedRetaliationValue=%lld; surroundingHexes=%d, nearestDistance=%d, totalDistance=%d, moveDistance=%d)",
			colorName,
			stack->getDescription(),
			plan.attackTarget->getDescription(),
			plan.destination.toInt(),
			plan.exposedEnemyValue,
			targetBaseValue,
			plan.expectedAttackValue,
			plan.expectedRetaliationValue,
			plan.surroundingHexCount,
			plan.minimumEnemyDistance,
			plan.totalEnemyDistance,
			plan.movementDistance
		);
		cb->battleMakeUnitAction(battleID, BattleAction::makeMeleeAttack(stack, plan.attackTarget, plan.destination));
		return RetreatResult::ATTACKED;
	}

	logAi->info(
		"HARBot [%s]: %s retreats to hex %d without attacking (exposure=%lld, surroundingHexes=%d, nearestDistance=%d, totalDistance=%d, moveDistance=%d)",
		colorName,
		stack->getDescription(),
		plan.destination.toInt(),
		plan.exposedEnemyValue,
		plan.surroundingHexCount,
		plan.minimumEnemyDistance,
		plan.totalEnemyDistance,
		plan.movementDistance
	);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, plan.destination));
	return RetreatResult::MOVED;
}

void HARBot::delegate(const BattleID & battleID, const CStack * stack, const std::string & reason)
{
	logAi->info("HARBot [%s]: delegating %s to %s: %s", colorName, stack->getDescription(), fallback, reason);
	fallbackBot->activeStack(battleID, stack);
}

}
