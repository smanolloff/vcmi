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
	bool IsWarMachine(const battle::Unit * unit)
	{
		return unit->hasBonusOfType(BonusType::SIEGE_WEAPON);
	}

	int64_t UnitValue(const battle::Unit * unit)
	{
		return UnitNode::GetValue(
			unit->unitType(),
			unit->isClone(),
			unit->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER
		);
	}

	int64_t StackValue(const CStack * stack)
	{
		return static_cast<int64_t>(stack->getCount()) * UnitValue(stack);
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
}

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
	retreatExposureCache.clear();

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
			if(result == RetreatResult::ATTACKED)
			{
				consecutiveRetreats.erase(stack);
				logAi->info("HARBot [%s]: attacking during retreat resets the consecutive retreat counter for %s", colorName, stack->getDescription());
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
		if(result == RetreatResult::ATTACKED)
		{
			consecutiveRetreats.erase(stack);
			logAi->info("HARBot [%s]: attacking during follow-up retreat resets the consecutive retreat counter for %s", colorName, stack->getDescription());
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
		consecutiveRetreats.erase(stack);
		if(attackAndMarkForRetreat(battleID, stack, true, true))
			return;

		logAi->info(
			"HARBot [%s]: no immediate attack avoids increasing exposure for %s; waiting instead",
			colorName,
			stack->getDescription()
		);
		if(!stack->waitedThisTurn)
			cb->battleMakeUnitAction(battleID, BattleAction::makeWait(stack));
		else
			cb->battleMakeUnitAction(battleID, BattleAction::makeDefend(stack));
		return;
	}
	else if(retreatCount > 0)
	{
		logAi->info("HARBot [%s]: %s is no longer threatened next turn and resumes its attack cycle", colorName, stack->getDescription());
	}

	if(!stack->waitedThisTurn)
	{
		consecutiveRetreats.erase(stack);
		const auto [waitingExposure, remainingHealth] = calculateExposedEnemyValue(stack, stack->getPosition(), true);
		logAi->info(
			"HARBot [%s]: current-round pre-wait exposure for %s is %lld/%lld expected HP damage (%.1f%% of remaining health)",
			colorName,
			stack->getDescription(),
			waitingExposure,
			remainingHealth,
			remainingHealth > 0 ? 100.0 * static_cast<double>(waitingExposure) / static_cast<double>(remainingHealth) : 0.0
		);
		if(waitingExposure > 0)
		{
			const auto retreatPlan = findBestRetreatFrom(stack, stack->getPosition(), true);
			int64_t bestAttackExposure = std::numeric_limits<int64_t>::max();
			const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
			const auto distances = battle->battleGetDistances(stack, stack->getPosition());
			const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
			for(const auto * enemy : enemies)
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
					const auto expectedKillsTwice = attack.kills.min + attack.kills.max;
					bestAttackExposure = std::min(
						bestAttackExposure,
						calculateExposedEnemyValue(stack, hex, true, false, enemy, expectedKillsTwice).first
					);
				}
			}

			const bool retreatIsViable = retreatPlan.destination.isValid()
				&& remainingHealth > 0
				&& retreatPlan.exposedEnemyValue * 10 <= remainingHealth * 3;
			if(retreatIsViable
				&& retreatPlan.exposedEnemyValue < waitingExposure
				&& retreatPlan.exposedEnemyValue < bestAttackExposure)
			{
				logAi->info(
					"HARBot [%s]: %s makes a standalone retreat to hex %d before waiting (retreatExposure=%lld, attackExposure=%lld, waitingExposure=%lld)",
					colorName,
					stack->getDescription(),
					retreatPlan.destination.toInt(),
					retreatPlan.exposedEnemyValue,
					bestAttackExposure,
					waitingExposure
				);
				consecutiveRetreats[stack] = 1;
				cb->battleMakeUnitAction(battleID, BattleAction::makeMove(stack, retreatPlan.destination));
				return;
			}

			logAi->info(
				"HARBot [%s]: %s is exposed to enemies still acting this round; searching for an immediate attack that does not increase exposure (bestRetreatExposure=%lld, bestAttackExposure=%lld)",
				colorName,
				stack->getDescription(),
				retreatPlan.exposedEnemyValue,
				bestAttackExposure
			);
			if(attackAndMarkForRetreat(battleID, stack, true, true, true))
				return;

			logAi->info(
				"HARBot [%s]: no immediate attack avoids increasing current-round exposure for %s; preserving the normal wait",
				colorName,
				stack->getDescription()
			);
		}

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

HARBot::RetreatPlan HARBot::findBestRetreatFrom(const CStack * stack, const BattleHex & assumedPosition, bool currentRoundExposureOnly, const CStack * attackedEnemy, int64_t expectedKillsTwice) const
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
		if(currentRoundExposureOnly || attackedEnemy)
		{
			exposedEnemyValue = calculateExposedEnemyValue(
				stack,
				destination,
				currentRoundExposureOnly,
				false,
				attackedEnemy,
				expectedKillsTwice
			).first;
		}
		else
		{
			auto exposureIt = retreatExposureCache.find(i);
			if(exposureIt == retreatExposureCache.end())
			{
				const auto exposure = calculateExposedEnemyValue(stack, destination, false, false).first;
				exposureIt = retreatExposureCache.emplace(i, exposure).first;
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
			if(!attackTarget
				|| attackValue > expectedAttackValue
				|| (attackValue == expectedAttackValue && retaliationValue < expectedRetaliationValue))
			{
				attackTarget = enemy;
				expectedAttackValue = attackValue;
				expectedRetaliationValue = retaliationValue;
			}
		}

		const auto tieBreakScore = std::tuple(
			expectedAttackValue,
			-expectedRetaliationValue,
			-surroundingHexCount,
			minimumEnemyDistance,
			totalEnemyDistance,
			-movementDistance
		);
		const auto bestTieBreakScore = std::tuple(
			best.expectedAttackValue,
			-best.expectedRetaliationValue,
			-best.surroundingHexCount,
			best.minimumEnemyDistance,
			best.totalEnemyDistance,
			-best.movementDistance
		);
		if(!best.destination.isValid()
			|| exposedEnemyValue < best.exposedEnemyValue
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
	int64_t totalEnemyValue = 0;
	for(const auto * enemy : enemies)
		if(enemy->alive() && !IsWarMachine(enemy) && !enemy->isInvincible() && enemy->getPosition().isValid())
			totalEnemyValue += StackValue(enemy);

	for(const auto * enemy : enemies)
	{
		if(!enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible() || enemy->isShooter() || !enemy->getPosition().isValid())
			continue;

		const auto enemyValue = StackValue(enemy);
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

std::pair<int64_t, int64_t> HARBot::calculateExposedEnemyValue(const CStack * stack, const BattleHex & destination, bool currentRoundOnly, bool logDetails, const CStack * attackedEnemy, int64_t expectedKillsTwice) const
{
	std::unordered_set<const battle::Unit *> enemiesActingThisRound;
	std::unordered_set<const battle::Unit *> enemiesActingBeforeNextTurn;
	std::vector<battle::Units> turnOrder;
	battle->battleGetTurnOrder(turnOrder, std::numeric_limits<size_t>::max(), 2);
	if(!turnOrder.empty())
	{
		for(const auto * unit : turnOrder.front())
			if(unit != stack && unit->unitSide() != stack->unitSide())
				enemiesActingThisRound.insert(unit);
	}

	bool foundCurrentTurn = false;
	bool foundNextTurn = false;
	for(const auto & turn : turnOrder)
	{
		for(const auto * unit : turn)
		{
			if(unit == stack)
			{
				if(foundCurrentTurn)
				{
					foundNextTurn = true;
					break;
				}
				foundCurrentTurn = true;
				continue;
			}

			if(foundCurrentTurn && unit->unitSide() != stack->unitSide())
				enemiesActingBeforeNextTurn.insert(unit);
		}
		if(foundNextTurn)
			break;
	}

	const auto isRelevant = [&](const CStack * enemy)
	{
		return enemy->alive()
			&& !IsWarMachine(enemy)
			&& !enemy->isInvincible()
			&& enemy->getPosition().isValid()
			&& (!currentRoundOnly || enemiesActingThisRound.contains(enemy));
	};

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
		if(!enemiesActingBeforeNextTurn.contains(enemy))
			continue;
		for(const auto & hex : enemy->getHexes())
			vacatedHexes.checkAndPush(hex);
	}
	for(const auto * enemy : enemies)
	{
		if(!isRelevant(enemy))
			continue;

		const int64_t survivingCountTwice = enemy == attackedEnemy
			? std::max<int64_t>(0, (static_cast<int64_t>(enemy->getCount()) * 2) - expectedKillsTwice)
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

		BattleHexArray knownAccessible = vacatedHexes;
		for(const auto & hex : enemy->getHexes())
			knownAccessible.checkAndPush(hex);
		const auto reachabilityParams = ReachabilityInfo::Parameters(enemy->unitSide(), enemy, enemy->getPosition(), knownAccessible);
		const auto reachability = battle->getReachability(reachabilityParams);
		const auto availableHexes = battle->battleGetAvailableHexes(reachability, enemy, false);
		std::vector<AttackOption> attackOptions;
		BattleHexArray targetHexes;
		targetHexes.insert(destination);
		if(stack->doubleWide())
			targetHexes.insert(stack->occupiedHex(destination));

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

				const auto movementDistance = static_cast<int>(reachability.distances.at(attackFrom.toInt()));
				BattleAttackInfo attackInfo(enemy, stack, movementDistance, false);
				attackInfo.attackerPos = attackFrom;
				attackInfo.defenderPos = destination;
				const auto expectedDamage = adjustForCasualties(ExpectedDamage(battle->battleEstimateDamage(attackInfo).damage, stack));
				const auto existing = std::ranges::find_if(attackOptions, [&footprint](const auto & option)
				{
					return option.footprint == footprint;
				});
				if(existing == attackOptions.end())
					attackOptions.push_back({ footprint, expectedDamage });
				else
					existing->expectedDamage = std::max(existing->expectedDamage, expectedDamage);
			}
		}

		if(logDetails)
		{
			logAi->debug(
				"HARBot [%s]: %s exposure enemy=%s mode=melee candidateFootprints=%d doubleWide=%d actsBeforeNextTurn=%d destination=%d",
				colorName,
				currentRoundOnly ? "current-round" : "all-enemy",
				enemy->getDescription(),
				static_cast<int>(attackOptions.size()),
				enemy->doubleWide(),
				enemiesActingBeforeNextTurn.contains(enemy),
				destination.toInt()
			);
		}

		if(!attackOptions.empty())
			meleeThreats.push_back({ enemy, std::move(attackOptions) });
	}

	int bestMeleeAttackerCount = 0;
	int64_t bestMeleeDamage = -1;
	std::vector<int64_t> selectedMeleeDamage(meleeThreats.size(), 0);
	std::vector<int64_t> bestSelectedMeleeDamage(meleeThreats.size(), 0);
	std::function<void(size_t, std::bitset<GameConstants::BFIELD_SIZE>, int, int64_t)> selectMeleeAttackers;
	selectMeleeAttackers = [&](size_t index, std::bitset<GameConstants::BFIELD_SIZE> occupiedHexes, int attackerCount, int64_t meleeDamage)
	{
		if(index == meleeThreats.size())
		{
			if(meleeDamage > bestMeleeDamage
				|| (meleeDamage == bestMeleeDamage && attackerCount > bestMeleeAttackerCount))
			{
				bestMeleeDamage = meleeDamage;
				bestMeleeAttackerCount = attackerCount;
				bestSelectedMeleeDamage = selectedMeleeDamage;
			}
			return;
		}

		selectedMeleeDamage.at(index) = 0;
		selectMeleeAttackers(index + 1, occupiedHexes, attackerCount, meleeDamage);
		for(const auto & option : meleeThreats.at(index).attackOptions)
		{
			if((occupiedHexes & option.footprint).any())
				continue;

			selectedMeleeDamage.at(index) = option.expectedDamage;
			selectMeleeAttackers(
				index + 1,
				occupiedHexes | option.footprint,
				attackerCount + 1,
				meleeDamage + option.expectedDamage
			);
			selectedMeleeDamage.at(index) = 0;
		}
	};
	selectMeleeAttackers(0, {}, 0, 0);

	if(logDetails)
	{
		for(size_t index = 0; index < meleeThreats.size(); ++index)
		{
			logAi->debug(
				"HARBot [%s]: %s exposure enemy=%s mode=melee selected=%d expectedDamage=%lld destination=%d",
				colorName,
				currentRoundOnly ? "current-round" : "all-enemy",
				meleeThreats.at(index).enemy->getDescription(),
				bestSelectedMeleeDamage.at(index) > 0,
				bestSelectedMeleeDamage.at(index),
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
			bestMeleeAttackerCount,
			std::min(rangedDamage + bestMeleeDamage, totalHealth),
			totalHealth
		);
	}

	return {std::min(rangedDamage + bestMeleeDamage, totalHealth), totalHealth};
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
			if(enemy == stack || enemy->unitSide() == stack->unitSide() || !enemy->alive() || IsWarMachine(enemy) || enemy->isInvincible()
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

bool HARBot::attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack, bool forceAttack, bool requireNoExposureIncrease, bool currentRoundExposureOnly)
{
	const auto availableHexes = battle->battleGetAvailableHexes(stack, false);
	const auto distances = battle->battleGetDistances(stack, stack->getPosition());
	const auto enemies = battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_ENEMY);
	int64_t totalEnemyValue = 0;
	for(const auto * enemy : enemies)
		if(enemy->alive() && !IsWarMachine(enemy) && !enemy->isInvincible() && enemy->getPosition().isValid())
			totalEnemyValue += StackValue(enemy);
	logAi->debug(
		"HARBot [%s]: evaluating attacks for %s from %d available hexes against %d enemies",
		colorName,
		stack->getDescription(),
		static_cast<int>(availableHexes.size()),
		static_cast<int>(enemies.size())
	);
	const bool shouldRetreat = std::ranges::any_of(enemies, [](const CStack * enemy)
	{
		return enemy->alive() && !IsWarMachine(enemy) && !enemy->isShooter() && enemy->getPosition().isValid();
	});
	const bool shouldPlanRetreat = shouldRetreat && !forceAttack;
	const bool preferLowerExposure = forceAttack || requireNoExposureIncrease;
	const auto waitingExposure = requireNoExposureIncrease
		? calculateExposedEnemyValue(stack, stack->getPosition(), currentRoundExposureOnly).first
		: int64_t{-1};
	const CStack * bestTarget = nullptr;
	BattleHex bestAttackHex;
	RetreatPlan bestRetreat;
	bool bestRetreatIsSafe = false;
	int64_t bestTargetBaseValue = std::numeric_limits<int64_t>::min();
	int64_t bestTargetValue = std::numeric_limits<int64_t>::min();
	int64_t bestAttackExposure = -1;

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

		const auto targetBaseValue = StackValue(enemy);
		const auto targetValue = enemy->isShooter() ? targetBaseValue * 4 : targetBaseValue;
		int attackHexCount = 0;
		for(const auto & hex : availableHexes)
		{
			if(!CStack::isMeleeAttackPossible(stack, enemy, hex))
				continue;

			++attackHexCount;
			BattleAttackInfo attackInfo(stack, enemy, static_cast<int>(distances.at(hex.toInt())), false);
			attackInfo.attackerPos = hex;
			attackInfo.defenderPos = enemy->getPosition();
			const auto attack = battle->battleEstimateDamage(attackInfo);
			const auto expectedKillsTwice = attack.kills.min + attack.kills.max;
			const auto attackExposure = preferLowerExposure
				? calculateExposedEnemyValue(stack, hex, currentRoundExposureOnly, false, enemy, expectedKillsTwice).first
				: int64_t{-1};

			const auto retreatPlan = shouldPlanRetreat
				? findBestRetreatFrom(stack, hex, false, enemy, expectedKillsTwice)
				: RetreatPlan{};
			const bool retreatIsSafe = shouldPlanRetreat
				&& retreatPlan.destination.isValid()
				&& !isImmediatelyThreatenedAt(stack, retreatPlan.destination);
			logAi->debug(
				"HARBot [%s]: attack candidate target=%s shooter=%d baseValue=%lld (%.1f%%) priorityValue=%lld fromHex=%d forceAttack=%d preferLowerExposure=%d requireNoExposureIncrease=%d expectedKills=%.1f attackExposure=%lld waitingExposure=%lld shouldPlanRetreat=%d retreatSafe=%d retreatHex=%d retreatExposure=%lld retreatAttackTarget=%s surroundingHexes=%d nearestDistance=%d totalDistance=%d retreatMoveDistance=%d",
				colorName,
				enemy->getDescription(),
				enemy->isShooter(),
				targetBaseValue,
				totalEnemyValue > 0 ? 100.0 * static_cast<double>(targetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
				targetValue,
				hex.toInt(),
				forceAttack,
				preferLowerExposure,
				requireNoExposureIncrease,
				static_cast<double>(expectedKillsTwice) / 2.0,
				attackExposure,
				waitingExposure,
				shouldPlanRetreat,
				retreatIsSafe,
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

			bool betterCandidate = !bestTarget;
			if(bestTarget && preferLowerExposure && attackExposure != bestAttackExposure)
				betterCandidate = attackExposure < bestAttackExposure;
			else if(bestTarget && shouldPlanRetreat && retreatPlan.exposedEnemyValue != bestRetreat.exposedEnemyValue)
				betterCandidate = retreatPlan.exposedEnemyValue < bestRetreat.exposedEnemyValue;
			else if(bestTarget && shouldPlanRetreat && retreatPlan.expectedAttackValue != bestRetreat.expectedAttackValue)
				betterCandidate = retreatPlan.expectedAttackValue > bestRetreat.expectedAttackValue;
			else if(bestTarget && shouldPlanRetreat && retreatPlan.expectedRetaliationValue != bestRetreat.expectedRetaliationValue)
				betterCandidate = retreatPlan.expectedRetaliationValue < bestRetreat.expectedRetaliationValue;
			else if(bestTarget && !shouldPlanRetreat)
				betterCandidate = targetValue > bestTargetValue;
			else if(bestTarget && retreatIsSafe != bestRetreatIsSafe)
				betterCandidate = retreatIsSafe;
			else if(bestTarget && retreatIsSafe)
			{
				const auto score = std::tuple(targetValue, -retreatPlan.surroundingHexCount, retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance);
				const auto bestScore = std::tuple(bestTargetValue, -bestRetreat.surroundingHexCount, bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance);
				betterCandidate = score > bestScore || (score == bestScore && retreatPlan.movementDistance < bestRetreat.movementDistance);
			}
			else if(bestTarget)
			{
				const auto score = std::tuple(-retreatPlan.surroundingHexCount, retreatPlan.minimumEnemyDistance, retreatPlan.totalEnemyDistance, targetValue);
				const auto bestScore = std::tuple(-bestRetreat.surroundingHexCount, bestRetreat.minimumEnemyDistance, bestRetreat.totalEnemyDistance, bestTargetValue);
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
				bestAttackExposure = attackExposure;
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

	if(requireNoExposureIncrease && bestAttackExposure > waitingExposure)
	{
		logAi->debug(
			"HARBot [%s]: best immediate attack target=%s fromHex=%d is rejected because %s exposure=%lld is above waiting exposure=%lld",
			colorName,
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			currentRoundExposureOnly ? "current-round" : "all-enemy",
			bestAttackExposure,
			waitingExposure
		);
		return false;
	}

	if(shouldPlanRetreat)
	{
		logAi->info(
			"HARBot [%s]: %s attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld), planning to retreat toward hex %d (exposure=%lld, retreatAttackTarget=%s, immediatelySafe=%d, nearest non-shooter distance=%d, moveDistance=%d)",
			colorName,
			stack->getDescription(),
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			bestTargetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			bestTargetValue,
			bestRetreat.destination.toInt(),
			bestRetreat.exposedEnemyValue,
			bestRetreat.attackTarget ? bestRetreat.attackTarget->getDescription() : "none",
			bestRetreatIsSafe,
			bestRetreat.minimumEnemyDistance,
			bestRetreat.movementDistance
		);
		mustRetreat.insert(stack);
	}
	else if(forceAttack)
	{
		logAi->info(
			"HARBot [%s]: %s immediately attacks %s from hex %d (baseValue=%lld, %.1f%%; priorityValue=%lld, exposure=%lld vs waiting=%lld) without requiring a viable retreat plan",
			colorName,
			stack->getDescription(),
			bestTarget->getDescription(),
			bestAttackHex.toInt(),
			bestTargetBaseValue,
			totalEnemyValue > 0 ? 100.0 * static_cast<double>(bestTargetBaseValue) / static_cast<double>(totalEnemyValue) : 0.0,
			bestTargetValue,
			bestAttackExposure,
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
		return StackValue(enemy);
	};
	const auto eligible = [](const CStack * enemy)
	{
		return enemy->alive() && !IsWarMachine(enemy) && !enemy->isInvincible() && enemy->getPosition().isValid();
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

	const auto [exposedValue, remainingHealth] = calculateExposedEnemyValue(stack, plan.destination);
	const bool exceedsExposureLimit = remainingHealth > 0 && exposedValue * 10 > remainingHealth * 3;
	logAi->info(
		"HARBot [%s]: retreat exposure at hex %d is %lld/%lld expected HP damage (%.1f%% of remaining health, limit=30%%)",
		colorName,
		plan.destination.toInt(),
		exposedValue,
		remainingHealth,
		remainingHealth > 0 ? 100.0 * static_cast<double>(exposedValue) / static_cast<double>(remainingHealth) : 0.0
	);
	if(exceedsExposureLimit)
	{
		logAi->info(
			"HARBot [%s]: retreat to hex %d is not viable because expected incoming damage to %s is %d%% of its remaining health",
			colorName,
			plan.destination.toInt(),
			stack->getDescription(),
			100 * exposedValue / remainingHealth
		);
		return RetreatResult::NOT_VIABLE;
	}

	if(plan.attackTarget)
	{
		const auto targetBaseValue = StackValue(plan.attackTarget);
		logAi->info(
			"HARBot [%s]: %s retreats by attacking %s from hex %d (exposure=%lld, targetBaseValue=%lld; expectedAttackValue=%lld, expectedRetaliationValue=%lld; surroundingHexes=%d, nearestDistance=%d, totalDistance=%d, moveDistance=%d)",
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
