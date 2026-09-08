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

#pragma once

#include "AI/MMAI/BAI/v15/fastbfs.h"
#include "battle/CPlayerBattleCallback.h"
#include "callback/CBattleGameInterface.h"

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace MMAI::BAI
{

class HARBot : public CBattleGameInterface
{
public:
	explicit HARBot(const std::string & fallback);

	void initBattleInterface(std::shared_ptr<Environment> env, std::shared_ptr<CBattleCallback> cb, AICombatOptions aiCombatOptions) override;
	void battleStart(
		const BattleID & battleID,
		const CCreatureSet * army1,
		const CCreatureSet * army2,
		int3 tile,
		const CGHeroInstance * hero1,
		const CGHeroInstance * hero2,
		BattleSide side,
		bool replayAllowed
	) override;
	void yourTacticPhase(const BattleID & battleID, int distance) override;
	void activeStack(const BattleID & battleID, const CStack * stack) override;
	void battleNewRound(const BattleID & battleID) override;
	void actionStarted(const BattleID & battleID, const BattleAction & action) override;

private:
	const std::string fallback = "?";

	std::shared_ptr<CBattleCallback> cb;
	std::shared_ptr<CBattleGameInterface> fallbackBot;
	std::shared_ptr<CPlayerBattleCallback> battle;
	const CStack * primaryStack = nullptr;
	std::unordered_set<const CStack *> mustRetreat;
	std::unordered_map<const CStack *, int> consecutiveRetreats;
	mutable std::unordered_map<int, int64_t> retreatExposureCache;
	std::string colorName = "?";

	std::unique_ptr<const MMAI::BAI::V15::FastBFS> fastbfs;

	struct ExposureEstimate
	{
		int64_t expectedDamage = 0;
		int64_t remainingHealth = 0;
	};

	struct ExposureOptions
	{
		bool currentRoundOnly = false;
		bool logDetails = true;
		const CStack * attackedEnemy = nullptr;
		int64_t expectedKillsTwice = 0;
	};

	struct RetreatPlan
	{
		BattleHex destination;
		const CStack * attackTarget = nullptr;
		int64_t expectedAttackValue = -1;
		int64_t expectedRetaliationValue = std::numeric_limits<int64_t>::max();
		int64_t exposedEnemyValue = std::numeric_limits<int64_t>::max();
		int surroundingHexCount = std::numeric_limits<int>::max();
		int minimumEnemyDistance = -1;
		int totalEnemyDistance = -1;
		int movementDistance = std::numeric_limits<int>::max();
	};

	struct AttackOptions
	{
		bool force = false;
		bool requireNoExposureIncrease = false;
		bool currentRoundExposureOnly = false;
	};

	struct AttackCandidate
	{
		const CStack * target = nullptr;
		BattleHex attackHex;
		RetreatPlan retreat;
		bool retreatIsSafe = false;
		int64_t targetBaseValue = std::numeric_limits<int64_t>::min();
		int64_t targetValue = std::numeric_limits<int64_t>::min();
		int64_t attackExposure = -1;
		int64_t expectedKillsTwice = 0;
	};

	struct StagingCandidate
	{
		BattleHex destination;
		const CStack * target = nullptr;
		int threatenedRetreatHexes = -1;
		int64_t reachableValue = -1;
		int64_t toleratedThreatValue = std::numeric_limits<int64_t>::max();
		int minimumEnemyDistance = -1;
		uint32_t nextAttackDistance = 0;
		int64_t targetValue = -1;
	};

	struct StagingThreats
	{
		bool unsafe = false;
		int64_t toleratedValue = 0;
		int minimumEnemyDistance = std::numeric_limits<int>::max();
	};

	struct ReachableEnemies
	{
		const CStack * target = nullptr;
		uint32_t attackDistance = 0;
		int64_t targetValue = -1;
		int64_t totalValue = 0;
	};

	enum class RetreatResult : std::uint8_t
	{
		MOVED,
		ATTACKED,
		NOT_VIABLE,
		UNAVAILABLE
	};

	bool handlePendingRetreat(const BattleID & battleID, const CStack * stack, bool & actAsAlreadyRetreated);
	bool handleFollowUpRetreat(const BattleID & battleID, const CStack * stack, bool actAsAlreadyRetreated);
	bool handlePreWaitTurn(const BattleID & battleID, const CStack * stack);
	void attackImmediatelyOrDelegate(const BattleID & battleID, const CStack * stack, const std::string & reason);

	RetreatPlan findBestRetreatFrom(const CStack * stack, const BattleHex & assumedPosition, const ExposureOptions & exposure) const;
	ExposureEstimate calculateExposure(const CStack * stack, const BattleHex & destination, const ExposureOptions & options) const;
	bool isImmediatelyThreatenedAt(const CStack * stack, const BattleHex & destination) const;
	bool canEnemyThreatenThisRound(const CStack * stack) const;
	bool canEnemyReachNextTurn(const CStack * stack) const;
	bool attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack, const AttackOptions & options);
	AttackCandidate evaluateAttackCandidate(
		const CStack * stack,
		const CStack * enemy,
		const BattleHex & attackHex,
		int movementDistance,
		bool preferLowerExposure,
		bool shouldPlanRetreat,
		bool currentRoundExposureOnly
	) const;
	bool isBetterAttackCandidate(const AttackCandidate & candidate, const AttackCandidate & best, bool preferLowerExposure, bool shouldPlanRetreat) const;
	bool isBetterStagingCandidate(const StagingCandidate & candidate, const StagingCandidate & best) const;
	StagingThreats assessStagingThreats(const CStack * stack, const BattleHex & destination, const TStacks & enemies, int64_t totalEnemyValue) const;
	V15::FastBFS::Distances getDistancesFromStaging(const CStack * stack, const BattleHex & destination) const;
	int countThreatenedRetreatHexes(const CStack * stack, const BattleHex & destination, const TStacks & enemies, const BattleHexArray & availableHexes) const;
	ReachableEnemies findReachableEnemies(const CStack * stack, const TStacks & enemies, const V15::FastBFS::Distances & distances) const;
	bool advanceTowardsEnemy(const BattleID & battleID, const CStack * stack);
	RetreatResult retreat(const BattleID & battleID, const CStack * stack);
	void delegate(const BattleID & battleID, const CStack * stack, const std::string & reason);
};

}
