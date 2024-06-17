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

#include "CGameInterface.h"
#include "battle/CPlayerBattleCallback.h"

#include "schema/base.h"

namespace MMAI::BAI {
    class Delegatee : public CBattleGameInterface {
    public:
        // Factory method for versioned sub-delegatees (BAI::V1, BAI::V2, etc.)
        static std::unique_ptr<Delegatee> Create(
            const std::string colorname,
            const Schema::Baggage* baggage,
            const std::shared_ptr<Environment> env,
            const std::shared_ptr<CBattleCallback> cb
        );

        Delegatee() = delete;
        Delegatee(
            const int version_,
            const std::string colorname_,
            const Schema::Baggage* baggage_,
            const std::shared_ptr<Environment> env_,
            const std::shared_ptr<CBattleCallback> cb_
        );

        /*
         * These methods MUST be overridden by sub-delegatees (e.g. BAI::V1)
         */
        virtual void activeStack(const BattleID &bid, const CStack * stack) override = 0;
        virtual void yourTacticPhase(const BattleID &bid, int distance) override = 0;
        virtual Schema::Action getNonRenderAction() = 0;

        /*
         * These methods MAY be overriden by sub-delegatees (e.g. BAI::V1)
         * Their implementation here is a no-op.
         */
        virtual void init() {};  // called just after the constructor
        virtual void actionFinished(const BattleID &bid, const BattleAction &action) override {};
        virtual void actionStarted(const BattleID &bid, const BattleAction &action) override {};
        virtual void battleAttack(const BattleID &bid, const BattleAttack *ba) override {};
        virtual void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> & bsa, bool ranged) override {};
        virtual void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override {};
        virtual void battleNewRoundFirst(const BattleID &bid) override {};
        virtual void battleNewRound(const BattleID &bid) override {};
        virtual void battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override {};
        virtual void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override {};
        virtual void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) override {};
        virtual void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override {};
        //virtual void battleStartBefore(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override {};
        virtual void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override {};
        virtual void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & units) override {};
        //virtual void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> & obstacles) override {};
        virtual void battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) override {};
        //virtual void battleGateStateChanged(const BattleID &bid, const EGateState state) override {};


        /*
         * These methods MUST NOT be called.
         * Their implementation here throws a runtime error.
         */
        virtual void initBattleInterface(std::shared_ptr<Environment> _1, std::shared_ptr<CBattleCallback> _2) override { throw std::runtime_error("Delegatee received initBattleInterface/2"); }
        virtual void initBattleInterface(std::shared_ptr<Environment> _1, std::shared_ptr<CBattleCallback> _2, AutocombatPreferences _3) override { throw std::runtime_error("Delegatee received initBattleInterface/3"); }
        virtual void initBattleInterface(std::shared_ptr<Environment> _1, std::shared_ptr<CBattleCallback> _2, std::any _3, std::string _4) override { throw std::runtime_error("Delegatee received initBattleInterface/4"); }

    protected:
        const int version;
        const std::string colorname;
        const std::shared_ptr<Environment> env;
        const std::shared_ptr<CBattleCallback> cb;
        const Schema::Baggage* baggage;

        std::string addrstr = "?";
        Schema::F_GetAction f_getAction;
        Schema::F_GetValue f_getValue;

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;
    private:
        void _error(const std::string &text) const;
        void _warn(const std::string &text) const;
        void _info(const std::string &text) const;
        void _debug(const std::string &text) const;
    };
}
