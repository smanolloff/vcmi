// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
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
#include "lib/AI_Base.h"
#include "base.h"

namespace MMAI::BAI {
    class Router : public CBattleGameInterface {
    public:
        Router();
        ~Router() override;

        std::unique_ptr<Base> bai;  // calls will be delegated to this object

        /*
         * Handled locally (not delegated)
         */

        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage, std::string colorstr) override;

        /*
         * Delegated to BAI
         */

        void actionFinished(const BattleID &bid, const BattleAction &action) override;
        void actionStarted(const BattleID &bid, const BattleAction &action) override;
        void activeStack(const BattleID &bid, const CStack * stack) override; //called when it's turn of that stack
        void battleAttack(const BattleID &bid, const BattleAttack *ba) override;
        void battleCatapultAttacked(const BattleID &bid, const CatapultAttack &ca) override;
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        void battleGateStateChanged(const BattleID &bid, const EGateState state) override;
        void battleLogMessage(const BattleID &bid, const std::vector<MetaString> &lines) override;
        void battleNewRound(const BattleID &bid) override;
        void battleNewRoundFirst(const BattleID &bid) override;
        void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> &obstacles) override;
        void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        void battleStackMoved(const BattleID &bid, const CStack *stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) override;
        void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect &sse) override;
        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override;
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect &bte) override;
        void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> &changes) override;
        void yourTacticPhase(const BattleID &bid, int distance) override;

    private:
        std::string addrstr = "?";
        std::string colorname = "?";

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;
        void trace(const std::string &text) const;
        void log(ELogLevel::ELogLevel level, const std::string &text) const;
    };
}
