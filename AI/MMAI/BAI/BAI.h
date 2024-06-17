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
#include "delegatee.h"

namespace MMAI::BAI {
    class BAI : public CBattleGameInterface {
    public:
        BAI();
        ~BAI() override;

        /*
         * Implemented locally (not delegated)
         */

        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage, std::string colorstr) override;

        /*
         * MUST be implemented by delegatees (BAI::V1, BAI::V2, etc.)
         */

        void activeStack(const BattleID &bid, const CStack * stack) override; //called when it's turn of that stack
        void yourTacticPhase(const BattleID &bid, int distance) override;

        /*
         * MAY be implemented by delegatees (BAI::V1, BAI::V2, etc.)
         */

        void actionFinished(const BattleID &bid, const BattleAction &action) override;//occurs AFTER every action taken by any stack or by the hero
        void actionStarted(const BattleID &bid, const BattleAction &action) override;//occurs BEFORE every action taken by any stack or by the hero

        void battleAttack(const BattleID &bid, const BattleAttack *ba) override; //called when stack is performing attack
        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        void battleNewRoundFirst(const BattleID &bid) override; //called at the beginning of each turn before changes are applied;
        void battleNewRound(const BattleID &bid) override; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
        // void battleLogMessage(const BattleID &bid, const std::vector<MetaString> & lines) override;
        void battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) override;//called when a specific effect is set to stacks
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override;
        //void battleStartBefore(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override; //called just before battle start
        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right
        void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & units) override;
        //void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> & obstacles) override;
        void battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) override; //called when catapult makes an attack
        //void battleGateStateChanged(const BattleID &bid, const EGateState state) override;

    private:
        friend class AAI;

        std::string addrstr = "?";
        std::string colorname = "?";

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;

        // Delegate all calls to this object
        // (which is a specific BAI version of BAI)
        std::unique_ptr<CBattleGameInterface> delegatee;
    };
}
