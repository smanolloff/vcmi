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

#include "lib/AI_Base.h"

#include "BAI/base.h"
#include "./battlefield.h"
#include "./attack_log.h"
#include "./action.h"
#include "./state.h"

namespace MMAI::BAI::V3 {
    class BAI : public Base {
    public:
        using Base::Base;

        // Bring thes template functions into the derived class's scope
        using Base::error;
        using Base::warn;
        using Base::info;
        using Base::debug;
        using Base::trace;
        using Base::log;
        using Base::_log;

        void activeStack(const BattleID &bid, const CStack * stack) override;
        void yourTacticPhase(const BattleID &bid, int distance) override;

        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override;
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right

        Schema::Action getNonRenderAction() override;
    protected:
        // Subsequent versions may override this with subclasses of State
        virtual std::unique_ptr<State> initState(const CPlayerBattleCallback* battle);
        std::unique_ptr<State> state;

        bool resetting = false;
        std::vector<Schema::Action> allactions; // DEBUG ONLY
        std::shared_ptr<CPlayerBattleCallback> battle;

        std::string renderANSI();
        std::string debugInfo(Action *action, const CStack *astack, BattleHex *nbh); // DEBUG ONLY
        std::shared_ptr<BattleAction> buildBattleAction();
        std::shared_ptr<BattleAction> maybeBuildAutoAction(const CStack * stack);
    };
}
