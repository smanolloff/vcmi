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

#include "BAI/delegatee.h"
#include "BAI/v1/battlefield.h"
#include "BAI/v1/attack_log.h"
#include "BAI/v1/action.h"
#include "BAI/v1/state.h"

namespace MMAI::BAI::V1 {
    class BAI : public Delegatee {
    public:
        using Delegatee::Delegatee;

        void activeStack(const BattleID &bid, const CStack * stack) override;
        void yourTacticPhase(const BattleID &bid, int distance) override;

        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override;
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right

        // Just for logging
        void actionFinished(const BattleID &bid, const BattleAction &action) override; //occurs AFTER every action taken by any stack or by the hero
        void actionStarted(const BattleID &bid, const BattleAction &action) override; //occurs BEFORE every action taken by any stack or by the hero
        void battleAttack(const BattleID &bid, const BattleAttack *ba) override; //called when stack is performing attack
        void battleNewRoundFirst(const BattleID &bid) override; //called at the beginning of each turn before changes are applied;
        void battleNewRound(const BattleID &bid) override; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
        void battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) override;//called when a specific effect is set to stacks
        void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & units) override;
        void battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) override; //called when catapult makes an attack

        Schema::Action getNonRenderAction() override;

    private:
        std::unique_ptr<Battlefield> battlefield;
        std::unique_ptr<State> state;
        std::shared_ptr<CPlayerBattleCallback> battle;
        bool resetting = false;
        std::vector<Schema::Action> allactions; // DEBUG ONLY

        std::string renderANSI();
        std::string debugInfo(Action *action, const CStack *astack, BattleHex *nbh); // DEBUG ONLY
        std::shared_ptr<BattleAction> buildBattleAction();
    };
}
