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
#include "CStack.h"
#include "battle/BattleAction.h"
#include "battle/BattleHex.h"
#include "callback/CBattleCallback.h"
#include "lib/callback/AIFactory.h"

#include "HARBot.h"

#include <limits>
#include <tuple>

namespace MMAI::BAI
{

using FastBFS = MMAI::BAI::V15::FastBFS;

HARBot::HARBot(const std::string & fallback)
	: fallback(fallback), fallbackBot(AIFactory::createBattleAI(fallback))
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

void HARBot::battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed)
{
	battle = cb->getBattle(battleID);
	fastbfs = std::make_unique<const FastBFS>(*battle, battle->getAccessibility());
	mustRetreat.clear();
	logAi->info(
		"HARBot [%s]: battle started as side=%d with %d friendly and %d enemy stacks",
		colorName,
		static_cast<int>(side),
		static_cast<int>(battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE).size()),
		static_cast<int>(battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY).size())
	);
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
		"HARBot [%s]: starting round %d (%d stacks marked to retreat)",
		colorName,
		battle ? battle->battleGetRound() : -1,
		static_cast<int>(mustRetreat.size())
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

	if(!stack->willMove())
	{
		delegate(battleID, stack, "stack cannot perform normal movement");
		return;
	}

	if(stack->hasBonusOfType(BonusType::SIEGE_WEAPON))
	{
		delegate(battleID, stack, "stack is a siege weapon");
		return;
	}

	if(stack->getMovementRange() == 0)
	{
		delegate(battleID, stack, "stack has zero movement range");
		return;
	}

	if(mustRetreat.erase(stack) > 0)
	{
		logAi->info("HARBot [%s]: %s is due to retreat after its previous attack", colorName, stack->getDescription());
		if(retreat(battleID, stack))
			return;

		delegate(battleID, stack, "no legal retreat destination was found");
		return;
	}

	if(!stack->waitedThisTurn)
	{
		logAi->info("HARBot [%s]: %s waits so it can attack late in the round", colorName, stack->getDescription());
		cb->battleMakeUnitAction(battleID, BattleAction::makeWait(stack));
		return;
	}

	logAi->debug("HARBot [%s]: %s has already waited; searching for a melee attack", colorName, stack->getDescription());
	if(attackAndMarkForRetreat(battleID, stack))
		return;

	logAi->info("HARBot [%s]: %s has no reachable melee target and will advance", colorName, stack->getDescription());
	if(advanceTowardsEnemy(battleID, stack))
		return;

	delegate(battleID, stack, "no legal advance toward an enemy was found");
}

HARBot::RetreatPlan HARBot::findBestRetreatFrom(const CStack * stack, const BattleHex & assumedPosition) const
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
			if(!enemy->alive() || !enemy->getPosition().isValid())
				continue;

			const auto distance = static_cast<int>(BattleHex::getDistance(destination, enemy->getPosition()));
			minimumEnemyDistance = std::min(minimumEnemyDistance, distance);
			totalEnemyDistance += distance;
		}

		if(minimumEnemyDistance == std::numeric_limits<int>::max())
			continue;

		const auto destinationX = static_cast<int>(destination.getX());
		const auto homewardProgress = stack->unitSide() == BattleSide::ATTACKER ? -destinationX : destinationX;
		if(!best.destination.isValid()
			|| std::tie(minimumEnemyDistance, totalEnemyDistance, homewardProgress)
				> std::tie(best.minimumEnemyDistance, best.totalEnemyDistance, best.homewardProgress)
			|| (std::tie(minimumEnemyDistance, totalEnemyDistance, homewardProgress)
				== std::tie(best.minimumEnemyDistance, best.totalEnemyDistance, best.homewardProgress)
				&& movementDistance < best.movementDistance))
		{
			best.destination = destination;
			best.minimumEnemyDistance = minimumEnemyDistance;
			best.totalEnemyDistance = totalEnemyDistance;
			best.homewardProgress = homewardProgress;
			best.movementDistance = movementDistance;
		}
	}

	return best;
}

bool HARBot::attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	logAi->debug(
		"HARBot [%s]: evaluating attacks for %s from %d available hexes against %d enemies",
		colorName,
		stack->getDescription(),
		static_cast<int>(availableHexes.size()),
		static_cast<int>(enemies.size())
	);
	const CStack * bestTarget = nullptr;
	BattleHex bestAttackHex;
	RetreatPlan bestRetreat;
	int64_t bestTargetValue = std::numeric_limits<int64_t>::min();

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

		const auto targetValue = static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
		int attackHexCount = 0;
		for(const auto & hex : availableHexes)
		{
			if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
				continue;

			++attackHexCount;
			const auto retreatPlan = findBestRetreatFrom(stack, hex);
			logAi->debug(
				"HARBot [%s]: attack candidate target=%s targetValue=%lld fromHex=%d retreatHex=%d nearestDistance=%d totalDistance=%d homewardProgress=%d retreatMoveDistance=%d",
				colorName,
				enemy->getDescription(),
				targetValue,
				hex.toInt(),
				retreatPlan.destination.toInt(),
				retreatPlan.minimumEnemyDistance,
				retreatPlan.totalEnemyDistance,
				retreatPlan.homewardProgress,
				retreatPlan.movementDistance
			);

			if(!retreatPlan.destination.isValid())
				continue;

			if(!bestTarget
				|| std::tie(retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance, retreatPlan.homewardProgress, targetValue)
					> std::tie(bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance, bestRetreat.homewardProgress, bestTargetValue)
				|| (std::tie(retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance, retreatPlan.homewardProgress, targetValue)
					== std::tie(bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance, bestRetreat.homewardProgress, bestTargetValue)
					&& retreatPlan.movementDistance < bestRetreat.movementDistance))
			{
				logAi->debug("HARBot [%s]: candidate becomes the current best attack/retreat plan", colorName);
				bestTarget = enemy;
				bestAttackHex = hex;
				bestRetreat = retreatPlan;
				bestTargetValue = targetValue;
			}
		}

		if(attackHexCount == 0)
			logAi->debug("HARBot [%s]: target %s is not reachable in melee", colorName, enemy->getDescription());
	}

	if(!bestTarget)
	{
		logAi->debug("HARBot [%s]: no legal melee attack found for %s", colorName, stack->getDescription());
		return false;
	}

	logAi->info(
		"HARBot [%s]: %s attacks %s from hex %d (targetValue=%lld), planning to retreat toward hex %d (nearestDistance=%d, moveDistance=%d)",
		colorName,
		stack->getDescription(),
		bestTarget->getDescription(),
		bestAttackHex.toInt(),
		bestTargetValue,
		bestRetreat.destination.toInt(),
		bestRetreat.minimumEnemyDistance,
		bestRetreat.movementDistance
	);
	mustRetreat.insert(stack);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMeleeAttack(stack, bestTarget, bestAttackHex));
	return true;
}

bool HARBot::advanceTowardsEnemy(const BattleID & battleID, const CStack * stack)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	BattleHex bestDestination;
	const CStack * bestTarget = nullptr;
	int bestThreatCount = std::numeric_limits<int>::max();
	int bestMinimumEnemyDistance = -1;
	uint32_t bestNextAttackDistance = 0;
	int64_t bestTargetValue = std::numeric_limits<int64_t>::min();

	logAi->debug(
		"HARBot [%s]: evaluating %d staging hexes for %s against %d enemies",
		colorName,
		static_cast<int>(availableHexes.size()),
		stack->getDescription(),
		static_cast<int>(enemies.size())
	);

	assert(fastbfs);

	for(const auto & destination : availableHexes)
	{
		if(stack->coversPos(destination))
			continue;

		const auto distancesFromDestination = fastbfs->run(
			stack->getPosition(),
			destination,
			stack->unitSide(),
			stack->hasBonusOfType(BonusType::FLYING),
			stack->doubleWide(),
			static_cast<int>(stack->getMovementRange()));

		const CStack * target = nullptr;
		uint32_t nextAttackDistance = 0;
		int64_t targetValue = std::numeric_limits<int64_t>::min();

		for(const auto * enemy : enemies)
		{
			if(!enemy->alive() || enemy->isInvincible())
				continue;

			auto distanceToAttack = FastBFS::INFINITE_DIST;
			for(const auto & attackHex : enemy->getAttackableHexes(stack))
			{
				if(attackHex.isValid())
					distanceToAttack = std::min(distanceToAttack, distancesFromDestination.at(attackHex.toInt()));
			}

			if(distanceToAttack > stack->getMovementRange())
				continue;

			const auto enemyValue = static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
			if(!target || distanceToAttack > nextAttackDistance || (distanceToAttack == nextAttackDistance && enemyValue > targetValue))
			{
				target = enemy;
				nextAttackDistance = distanceToAttack;
				targetValue = enemyValue;
			}
		}

		if(!target)
		{
			logAi->debug("HARBot [%s]: staging hex %d cannot produce a melee attack next turn", colorName, destination.toInt());
			continue;
		}

		int threatCount = 0;
		int minimumEnemyDistance = std::numeric_limits<int>::max();
		for(const auto * enemy : enemies)
		{
			if(!enemy->alive() || enemy->isInvincible())
				continue;

			minimumEnemyDistance = std::min(
				minimumEnemyDistance,
				static_cast<int>(BattleHex::getDistance(destination, enemy->getPosition()))
			);

			const auto enemyAvailableHexes = battle->battleGetAvailableHexes(enemy, false);
			bool canAttackDestination = battle->battleCanAttackHex(enemyAvailableHexes, enemy, destination);
			if(stack->doubleWide())
				canAttackDestination |= battle->battleCanAttackHex(enemyAvailableHexes, enemy, stack->occupiedHex(destination));

			if(canAttackDestination)
				++threatCount;
		}

		logAi->debug(
			"HARBot [%s]: staging candidate hex=%d target=%s nextAttackDistance=%u threats=%d nearestEnemyDistance=%d targetValue=%lld",
			colorName,
			destination.toInt(),
			target->getDescription(),
			nextAttackDistance,
			threatCount,
			minimumEnemyDistance,
			targetValue
		);

		if(!bestDestination.isValid()
			|| threatCount < bestThreatCount
			|| (threatCount == bestThreatCount && std::tie(minimumEnemyDistance, nextAttackDistance, targetValue) > std::tie(bestMinimumEnemyDistance, bestNextAttackDistance, bestTargetValue)))
		{
			bestDestination = destination;
			bestTarget = target;
			bestThreatCount = threatCount;
			bestMinimumEnemyDistance = minimumEnemyDistance;
			bestNextAttackDistance = nextAttackDistance;
			bestTargetValue = targetValue;
		}
	}

	if(!bestDestination.isValid())
	{
		logAi->info("HARBot [%s]: no staging hex enables a melee attack next turn for %s", colorName, stack->getDescription());
		return false;
	}

	logAi->info(
		"HARBot [%s]: %s stages at hex %d for a next-turn attack on %s (attackDistance=%u, threats=%d, nearestEnemyDistance=%d)",
		colorName,
		stack->getDescription(),
		bestDestination.toInt(),
		bestTarget->getDescription(),
		bestNextAttackDistance,
		bestThreatCount,
		bestMinimumEnemyDistance
	);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, bestDestination));
	return true;
}

bool HARBot::retreat(const BattleID & battleID, const CStack * stack)
{
	logAi->debug(
		"HARBot [%s]: evaluating retreat for %s from hex %d",
		colorName,
		stack->getDescription(),
		stack->getPosition().toInt()
	);
	const auto plan = findBestRetreatFrom(stack, stack->getPosition());

	if(!plan.destination.isValid())
	{
		logAi->info("HARBot [%s]: no legal retreat destination found for %s", colorName, stack->getDescription());
		return false;
	}

	logAi->info(
		"HARBot [%s]: %s retreats to hex %d (nearestDistance=%d, totalDistance=%d, homewardProgress=%d, moveDistance=%d)",
		colorName,
		stack->getDescription(),
		plan.destination.toInt(),
		plan.minimumEnemyDistance,
		plan.totalEnemyDistance,
		plan.homewardProgress,
		plan.movementDistance
	);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, plan.destination));
	return true;
}

void HARBot::delegate(const BattleID & battleID, const CStack * stack, const std::string & reason)
{
	logAi->info("HARBot [%s]: delegating %s to %s: %s", colorName, stack->getDescription(), fallback, reason);
	fallbackBot->activeStack(battleID, stack);
}

}
