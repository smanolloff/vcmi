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
	primaryStack = nullptr;
	int64_t primaryValue = -1;
	for(const auto * stack : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE))
	{
		if(!stack->alive() || stack->unitType()->getGrowth() <= 0)
			continue;

		const auto value = static_cast<int64_t>(stack->getCount()) * stack->unitType()->getAIValue();
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

	fastbfs = std::make_unique<const FastBFS>(*battle, battle->getAccessibility());

	if(stack != primaryStack)
	{
		delegate(
			battleID,
			stack,
			primaryStack
				? "stack is not HARBot's primary army stack"
				: "HARBot has no regular primary army stack"
		);
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

	const auto attackImmediately = [&](const std::string & reason)
	{
		consecutiveRetreats.erase(stack);
		logAi->info("HARBot [%s]: %s; %s will attack immediately", colorName, reason, stack->getDescription());
		if(!attackAndMarkForRetreat(battleID, stack, true))
			delegate(battleID, stack, "immediate attack requested, but no legal melee attack was found");
	};

	bool actAsAlreadyRetreated = false;
	if(mustRetreat.erase(stack) > 0)
	{
		logAi->info("HARBot [%s]: %s is due to retreat after its previous attack", colorName, stack->getDescription());
		if(!canEnemyThreatenThisRound(stack))
		{
			logAi->info(
				"HARBot [%s]: no non-shooter enemy acting later this round can threaten %s; skipping retreat",
				colorName,
				stack->getDescription()
			);
			consecutiveRetreats[stack] = 1;
			actAsAlreadyRetreated = true;
		}
		else
		{
			const auto result = retreat(battleID, stack);
			if(result == RetreatResult::MOVED)
			{
				consecutiveRetreats[stack] = 1;
				return;
			}
			attackImmediately(
				result == RetreatResult::NOT_VIABLE
					? "retreat is no longer viable"
					: "no legal retreat destination was found"
			);
			return;
		}
	}

	const auto retreatIt = consecutiveRetreats.find(stack);
	const auto retreatCount = retreatIt == consecutiveRetreats.end() ? 0 : retreatIt->second;
	if(!actAsAlreadyRetreated && retreatCount > 0 && retreatCount < 2 && canEnemyReachNextTurn(stack))
	{
		logAi->info(
			"HARBot [%s]: %s is reachable after %d consecutive retreat(s) and will retreat again",
			colorName,
			stack->getDescription(),
			retreatCount
		);
		const auto result = retreat(battleID, stack);
		if(result == RetreatResult::MOVED)
		{
			consecutiveRetreats[stack] = retreatCount + 1;
			return;
		}
		attackImmediately(
			result == RetreatResult::NOT_VIABLE
				? "follow-up retreat is no longer viable"
				: "no legal follow-up retreat destination was found"
		);
		return;
	}
	else if(retreatCount >= 2)
	{
		logAi->info(
			"HARBot [%s]: %s reached the limit of %d consecutive retreats",
			colorName,
			stack->getDescription(),
			retreatCount
		);
		attackImmediately("consecutive retreat limit reached");
		return;
	}
	else if(retreatCount > 0)
	{
		logAi->info("HARBot [%s]: %s is no longer reachable next turn and resumes its attack cycle", colorName, stack->getDescription());
	}

	if(!stack->waitedThisTurn)
	{
		consecutiveRetreats.erase(stack);
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
			if(!enemy->alive() || enemy->isShooter() || !enemy->getPosition().isValid())
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

bool HARBot::isImmediatelyThreatenedAt(const CStack * stack, const BattleHex & destination) const
{
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	int64_t totalEnemyValue = 0;
	for(const auto * enemy : enemies)
		if(enemy->alive() && !enemy->isInvincible() && enemy->getPosition().isValid())
			totalEnemyValue += static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();

	for(const auto * enemy : enemies)
	{
		if(!enemy->alive() || enemy->isInvincible() || enemy->isShooter() || !enemy->getPosition().isValid())
			continue;

		const auto enemyValue = static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
		if(totalEnemyValue <= 0 || enemyValue * 10 < totalEnemyValue)
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

std::pair<int64_t, int64_t> HARBot::calculateExposedEnemyValue(const CStack * stack, const BattleHex & destination) const
{
	int64_t exposedValue = 0;
	int64_t totalValue = 0;
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	for(const auto * enemy : enemies)
		if(enemy->alive() && !enemy->isInvincible() && enemy->getPosition().isValid())
			totalValue += static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();

	for(const auto * enemy : enemies)
	{
		if(!enemy->alive() || enemy->isInvincible() || !enemy->getPosition().isValid())
			continue;

		const auto enemyValue = static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();

		const bool canShoot = enemy->isShooter() && battle->battleCanShoot(enemy, destination);
		const auto enemyAvailableHexes = battle->battleGetAvailableHexes(enemy, false);
		bool canMelee = battle->battleCanAttackHex(enemyAvailableHexes, enemy, destination);
		if(stack->doubleWide())
			canMelee |= battle->battleCanAttackHex(enemyAvailableHexes, enemy, stack->occupiedHex(destination));

		const bool hasUnpenalizedMelee = canMelee && enemy->hasBonusOfType(BonusType::NO_MELEE_PENALTY);
		const bool hasShootingDistancePenalty = canShoot
			&& !hasUnpenalizedMelee
			&& battle->battleHasDistancePenalty(enemy, enemy->getPosition(), destination);
		const auto exposedContribution = hasShootingDistancePenalty ? enemyValue / 2 : enemyValue;
		logAi->debug(
			"HARBot [%s]: retreat exposure enemy=%s shooter=%d canShoot=%d canMelee=%d noMeleePenalty=%d distancePenalty=%d aiValue=%lld (%.1f%%) exposedContribution=%lld (%.1f%%) destination=%d",
			colorName,
			enemy->getDescription(),
			enemy->isShooter(),
			canShoot,
			canMelee,
			hasUnpenalizedMelee,
			hasShootingDistancePenalty,
			enemyValue,
			totalValue > 0 ? 100.0 * static_cast<double>(enemyValue) / static_cast<double>(totalValue) : 0.0,
			exposedContribution,
			totalValue > 0 ? 100.0 * static_cast<double>(exposedContribution) / static_cast<double>(totalValue) : 0.0,
			destination.toInt()
		);

		if(canShoot || canMelee)
			exposedValue += exposedContribution;
	}

	return {exposedValue, totalValue};
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
			if(enemy == stack || enemy->unitSide() == stack->unitSide() || !enemy->alive() || enemy->isInvincible()
				|| enemy->isShooter() || !enemy->getPosition().isValid() || enemy->getMovementRange() == 0)
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
				attackDistance,
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
		if(!enemy->alive() || enemy->isInvincible() || !enemy->getPosition().isValid() || enemy->getMovementRange() == 0)
			continue;

		if(enemy->isShooter())
		{
			logAi->debug(
				"HARBot [%s]: ignoring shooter %s when checking whether %s should retreat again",
				colorName,
				enemy->getDescription(),
				stack->getDescription()
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
			attackDistance,
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

bool HARBot::attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack, bool forceAttack)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	int64_t totalEnemyValue = 0;
	for(const auto * enemy : enemies)
		if(enemy->alive() && !enemy->isInvincible() && enemy->getPosition().isValid())
			totalEnemyValue += static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
	logAi->debug(
		"HARBot [%s]: evaluating attacks for %s from %d available hexes against %d enemies",
		colorName,
		stack->getDescription(),
		static_cast<int>(availableHexes.size()),
		static_cast<int>(enemies.size())
	);
	const bool shouldRetreat = std::ranges::any_of(enemies, [](const CStack * enemy)
	{
		return enemy->alive() && !enemy->isShooter() && enemy->getPosition().isValid();
	});
	const bool shouldPlanRetreat = shouldRetreat && !forceAttack;
	const CStack * bestTarget = nullptr;
	BattleHex bestAttackHex;
	RetreatPlan bestRetreat;
	bool bestRetreatIsSafe = false;
	int64_t bestTargetBaseValue = std::numeric_limits<int64_t>::min();
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

		const auto targetBaseValue = static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
		const auto targetValue = enemy->isShooter() ? targetBaseValue * 4 : targetBaseValue;
		int attackHexCount = 0;
		for(const auto & hex : availableHexes)
		{
			if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
				continue;

			++attackHexCount;
			const auto retreatPlan = shouldPlanRetreat ? findBestRetreatFrom(stack, hex) : RetreatPlan{};
			const bool retreatIsSafe = shouldPlanRetreat
				&& retreatPlan.destination.isValid()
				&& !isImmediatelyThreatenedAt(stack, retreatPlan.destination);
			logAi->debug(
				"HARBot [%s]: attack candidate target=%s shooter=%d baseValue=%lld (%.1f%%) priorityValue=%lld fromHex=%d forceAttack=%d shouldPlanRetreat=%d retreatSafe=%d retreatHex=%d nearestDistance=%d totalDistance=%d homewardProgress=%d retreatMoveDistance=%d",
				colorName,
				enemy->getDescription(),
				enemy->isShooter(),
				targetBaseValue,
				totalEnemyValue > 0 ? 100.0 * static_cast<double>(targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
				targetValue,
				hex.toInt(),
				forceAttack,
				shouldPlanRetreat,
				retreatIsSafe,
				retreatPlan.destination.toInt(),
				retreatPlan.minimumEnemyDistance,
				retreatPlan.totalEnemyDistance,
				retreatPlan.homewardProgress,
				retreatPlan.movementDistance
			);

			if(shouldPlanRetreat && !retreatPlan.destination.isValid())
				continue;

			bool betterCandidate = !bestTarget;
			if(bestTarget && !shouldPlanRetreat)
				betterCandidate = targetValue > bestTargetValue;
			else if(bestTarget && retreatIsSafe != bestRetreatIsSafe)
				betterCandidate = retreatIsSafe;
			else if(bestTarget && retreatIsSafe)
			{
				const auto score = std::tie(targetValue, retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance, retreatPlan.homewardProgress);
				const auto bestScore = std::tie(bestTargetValue, bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance, bestRetreat.homewardProgress);
				betterCandidate = score > bestScore || (score == bestScore && retreatPlan.movementDistance < bestRetreat.movementDistance);
			}
			else if(bestTarget)
			{
				const auto score = std::tie(retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance, retreatPlan.homewardProgress, targetValue);
				const auto bestScore = std::tie(bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance, bestRetreat.homewardProgress, bestTargetValue);
				betterCandidate = score > bestScore || (score == bestScore && retreatPlan.movementDistance < bestRetreat.movementDistance);
			}

			if(betterCandidate)
			{
				logAi->debug("HARBot [%s]: candidate becomes the current best attack/retreat plan", colorName);
				bestTarget = enemy;
				bestAttackHex = hex;
				bestRetreat = retreatPlan;
				bestRetreatIsSafe = retreatIsSafe;
				bestTargetBaseValue = targetBaseValue;
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

	if(shouldPlanRetreat)
	{
		logAi->info(
			"HARBot [%s]: %s attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld), planning to retreat toward hex %d (immediatelySafe=%d, nearest non-shooter distance=%d, moveDistance=%d)",
			colorName,
			stack->getDescription(),
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			bestTargetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			bestTargetValue,
			bestRetreat.destination.toInt(),
			bestRetreatIsSafe,
			bestRetreat.minimumEnemyDistance,
			bestRetreat.movementDistance
		);
		mustRetreat.insert(stack);
	}
	else if(forceAttack)
	{
		logAi->info(
			"HARBot [%s]: %s immediately attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld) without requiring a viable retreat plan",
			colorName,
			stack->getDescription(),
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			bestTargetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			bestTargetValue
		);
		if(shouldRetreat)
			mustRetreat.insert(stack);
		else
			consecutiveRetreats.erase(stack);
	}
	else
	{
		logAi->info(
			"HARBot [%s]: %s attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld) without retreating because no living non-shooter enemies remain",
			colorName,
			stack->getDescription(),
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			bestTargetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			bestTargetValue
		);
		consecutiveRetreats.erase(stack);
	}
	cb->battleMakeUnitAction(battleID, BattleAction::makeMeleeAttack(stack, bestTarget, bestAttackHex));
	return true;
}

bool HARBot::advanceTowardsEnemy(const BattleID & battleID, const CStack * stack)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	const auto enemyValue = [](const CStack * enemy)
	{
		return static_cast<int64_t>(enemy->getCount()) * enemy->unitType()->getAIValue();
	};
	const auto eligible = [](const CStack * enemy)
	{
		return enemy->alive() && !enemy->isInvincible() && enemy->getPosition().isValid();
	};
	int64_t totalEnemyValue = 0;
	for(const auto * enemy : enemies)
		if(eligible(enemy))
			totalEnemyValue += enemyValue(enemy);

	BattleHex bestDestination;
	const CStack * bestTarget = nullptr;
	int bestThreatenedRetreatHexes = -1;
	int64_t bestReachableValue = -1;
	int64_t bestToleratedThreatValue = std::numeric_limits<int64_t>::max();
	int bestMinimumEnemyDistance = -1;
	uint32_t bestNextAttackDistance = 0;
	int64_t bestTargetValue = -1;

	logAi->debug(
		"HARBot [%s]: evaluating %d staging hexes for %s against %d enemies (totalEnemyValue=%lld)",
		colorName,
		static_cast<int>(availableHexes.size()),
		stack->getDescription(),
		static_cast<int>(enemies.size()),
		totalEnemyValue
	);

	assert(fastbfs);
	for(const auto & destination : availableHexes)
	{
		if(stack->coversPos(destination))
			continue;

		int64_t toleratedThreatValue = 0;
		int minimumEnemyDistance = std::numeric_limits<int>::max();
		bool unsafe = false;
		for(const auto * enemy : enemies)
		{
			if(!eligible(enemy))
				continue;

			minimumEnemyDistance = std::min(
				minimumEnemyDistance,
				static_cast<int>(BattleHex::getDistance(destination, enemy->getPosition()))
			);
			const auto enemyAvailableHexes = battle->battleGetAvailableHexes(enemy, false);
			bool canAttackDestination = battle->battleCanAttackHex(enemyAvailableHexes, enemy, destination);
			const auto occupiedHex = stack->occupiedHex(destination);
			if(stack->doubleWide() && occupiedHex.isValid())
				canAttackDestination |= battle->battleCanAttackHex(enemyAvailableHexes, enemy, occupiedHex);
			if(!canAttackDestination)
				continue;

			const auto value = enemyValue(enemy);
			if(totalEnemyValue > 0 && value * 10 >= totalEnemyValue)
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
				unsafe = true;
				break;
			}
			toleratedThreatValue += value;
		}
		if(unsafe)
			continue;

		const auto distancesFromDestination = fastbfs->run(
			stack->getPosition(),
			destination,
			stack->unitSide(),
			stack->hasBonusOfType(BonusType::FLYING),
			stack->doubleWide(),
			static_cast<int>(stack->getMovementRange()));
		BattleHexArray nextTurnAvailableHexes;
		for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
			if(distancesFromDestination.at(i) <= stack->getMovementRange())
				nextTurnAvailableHexes.insert(BattleHex(static_cast<si16>(i)));

		int threatenedRetreatHexes = 0;
		for(const auto * enemy : enemies)
		{
			if(!eligible(enemy))
				continue;

			int threatenedForEnemy = 0;
			for(const auto & retreatHex : battle->battleGetAvailableHexes(enemy, false))
			{
				bool threatened = battle->battleCanAttackHex(nextTurnAvailableHexes, stack, retreatHex);
				const auto occupiedHex = enemy->occupiedHex(retreatHex);
				if(enemy->doubleWide() && occupiedHex.isValid())
					threatened |= battle->battleCanAttackHex(nextTurnAvailableHexes, stack, occupiedHex);
				if(threatened)
				{
					++threatenedForEnemy;
					++threatenedRetreatHexes;
				}
			}
			logAi->debug(
				"HARBot [%s]: staging hex %d threatens %d possible positions of %s",
				colorName,
				destination.toInt(),
				threatenedForEnemy,
				enemy->getDescription()
			);
		}

		const CStack * target = nullptr;
		uint32_t nextAttackDistance = 0;
		int64_t targetValue = -1;
		int64_t reachableValue = 0;

		for(const auto * enemy : enemies)
		{
			if(!eligible(enemy))
				continue;

			auto distanceToAttack = FastBFS::INFINITE_DIST;
			for(const auto & attackHex : enemy->getAttackableHexes(stack))
				if(attackHex.isValid())
					distanceToAttack = std::min(distanceToAttack, distancesFromDestination.at(attackHex.toInt()));
			if(distanceToAttack > stack->getMovementRange())
				continue;

			const auto value = enemyValue(enemy);
			reachableValue += value;
			if(!target || value > targetValue || (value == targetValue && distanceToAttack > nextAttackDistance))
			{
				target = enemy;
				nextAttackDistance = distanceToAttack;
				targetValue = value;
			}
		}
		if(!target)
		{
			logAi->debug("HARBot [%s]: staging hex %d cannot produce a melee attack next turn", colorName, destination.toInt());
			continue;
		}

		logAi->debug(
			"HARBot [%s]: staging candidate hex=%d threatenedRetreatHexes=%d reachableValue=%lld (%.1f%%) toleratedThreatValue=%lld (%.1f%%) nearestEnemyDistance=%d representativeTarget=%s targetValue=%lld (%.1f%%) attackDistance=%u",
			colorName,
			destination.toInt(),
			threatenedRetreatHexes,
			reachableValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(reachableValue) / static_cast<double>(totalEnemyValue) : 0.0,
			toleratedThreatValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(toleratedThreatValue) / static_cast<double>(totalEnemyValue) : 0.0,
			minimumEnemyDistance,
			target->getDescription(),
			targetValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(targetValue) / static_cast<double>(totalEnemyValue) : 0.0,
			nextAttackDistance
		);
		if(!bestDestination.isValid()
			|| threatenedRetreatHexes > bestThreatenedRetreatHexes
			|| (threatenedRetreatHexes == bestThreatenedRetreatHexes && minimumEnemyDistance > bestMinimumEnemyDistance)
			|| (threatenedRetreatHexes == bestThreatenedRetreatHexes && minimumEnemyDistance == bestMinimumEnemyDistance
				&& toleratedThreatValue < bestToleratedThreatValue)
			|| (threatenedRetreatHexes == bestThreatenedRetreatHexes && minimumEnemyDistance == bestMinimumEnemyDistance
				&& toleratedThreatValue == bestToleratedThreatValue
				&& std::tie(reachableValue, nextAttackDistance, targetValue)
					> std::tie(bestReachableValue, bestNextAttackDistance, bestTargetValue)))
		{
			bestDestination = destination;
			bestTarget = target;
			bestThreatenedRetreatHexes = threatenedRetreatHexes;
			bestReachableValue = reachableValue;
			bestToleratedThreatValue = toleratedThreatValue;
			bestMinimumEnemyDistance = minimumEnemyDistance;
			bestNextAttackDistance = nextAttackDistance;
			bestTargetValue = targetValue;
		}
	}

	if(!bestDestination.isValid())
	{
		logAi->info("HARBot [%s]: no safe staging hex enables a melee attack next turn for %s", colorName, stack->getDescription());
		return false;
	}

	logAi->info(
		"HARBot [%s]: %s stages at hex %d threatening %d enemy retreat positions while nearestEnemyDistance=%d; reachableEnemyValue=%lld (%.1f%%), toleratedThreatValue=%lld (%.1f%%); representative target=%s (value=%lld, %.1f%%, attackDistance=%u)",
		colorName,
		stack->getDescription(),
		bestDestination.toInt(),
		bestThreatenedRetreatHexes,
		bestMinimumEnemyDistance,
		bestReachableValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestReachableValue) / static_cast<double>(totalEnemyValue) : 0.0,
		bestToleratedThreatValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestToleratedThreatValue) / static_cast<double>(totalEnemyValue) : 0.0,
		bestTarget->getDescription(),
		bestTargetValue,
		totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetValue) / static_cast<double>(totalEnemyValue) : 0.0,
		bestNextAttackDistance
	);
	cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, bestDestination));
	return true;
}

HARBot::RetreatResult HARBot::retreat(const BattleID & battleID, const CStack * stack)
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
		return RetreatResult::UNAVAILABLE;
	}

	const auto [exposedValue, totalValue] = calculateExposedEnemyValue(stack, plan.destination);
	const bool exceedsExposureLimit = totalValue > 0 && exposedValue * 10 > totalValue * 3;
	logAi->info(
		"HARBot [%s]: retreat exposure at hex %d is %lld/%lld AI value (%.1f%%, limit=30%%)",
		colorName,
		plan.destination.toInt(),
		exposedValue,
		totalValue,
		totalValue > 0 ? 100.0 * static_cast<double>(exposedValue) / static_cast<double>(totalValue) : 0.0
	);
	if(exceedsExposureLimit)
	{
		logAi->info(
			"HARBot [%s]: retreat to hex %d is not viable because it exposes %s to %d%% of enemy army AI value",
			colorName,
			plan.destination.toInt(),
			stack->getDescription(),
			100 * exposedValue / totalValue
		);
		return RetreatResult::NOT_VIABLE;
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
	return RetreatResult::MOVED;
}

void HARBot::delegate(const BattleID & battleID, const CStack * stack, const std::string & reason)
{
	logAi->info("HARBot [%s]: delegating %s to %s: %s", colorName, stack->getDescription(), fallback, reason);
	fallbackBot->activeStack(battleID, stack);
}

}
