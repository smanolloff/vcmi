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

#include "battle/CPlayerBattleCallback.h"
#include "callback/CBattleGameInterface.h"
#include "AI/MMAI/BAI/v15/fastbfs.h"

#include <limits>
#include <unordered_set>

namespace MMAI::BAI
{

class HARBot : public CBattleGameInterface
{
public:
	explicit HARBot(const std::string & fallback);

	void initBattleInterface(std::shared_ptr<Environment> env, std::shared_ptr<CBattleCallback> cb, AICombatOptions aiCombatOptions) override;
	void battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed) override;
	void yourTacticPhase(const BattleID & battleID, int distance) override;
	void activeStack(const BattleID & battleID, const CStack * stack) override;
	void battleNewRound(const BattleID & battleID) override;
	void actionStarted(const BattleID & battleID, const BattleAction & action) override;

private:
    const std::string fallback = "?";

	std::shared_ptr<CBattleCallback> cb;
	std::shared_ptr<CBattleGameInterface> fallbackBot;
	std::shared_ptr<CPlayerBattleCallback> battle;
	std::unordered_set<const CStack *> mustRetreat;
	std::string colorName = "?";

	std::unique_ptr<const MMAI::BAI::V15::FastBFS> fastbfs;

	struct RetreatPlan
	{
		BattleHex destination;
		int minimumEnemyDistance = -1;
		int totalEnemyDistance = -1;
		int homewardProgress = -1;
		int movementDistance = std::numeric_limits<int>::max();
	};

	RetreatPlan findBestRetreatFrom(const CStack * stack, const BattleHex & assumedPosition) const;
	bool attackAndMarkForRetreat(const BattleID & battleID, const CStack * stack);
	bool advanceTowardsEnemy(const BattleID & battleID, const CStack * stack);
	bool retreat(const BattleID & battleID, const CStack * stack);
	void delegate(const BattleID & battleID, const CStack * stack, const std::string & reason);
};

}
