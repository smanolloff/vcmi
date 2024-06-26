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

#include "battle/BattleHex.h"
#include "battle/CBattleInfoEssentials.h"
#include "battle/CPlayerBattleCallback.h"
#include "schema/base.h"
#include "schema/v3/constants.h"
#include "schema/v3/types.h"
#include "./supplementary_data.h"
#include "./battlefield.h"
#include "./attack_log.h"
#include "./action.h"

namespace MMAI::BAI::V3 {
    using BS = Schema::BattlefieldState;

    class State : public Schema::IState {
    public:
        // IState impl
        const Schema::ActionMask& getActionMask() const override { return actmask; };
        const Schema::AttentionMask& getAttentionMask() const override { return attnmask; }
        const Schema::BattlefieldState& getBattlefieldState() const override { return bfstate; }
        const std::any getSupplementaryData() const override {
            return static_cast<const MMAI::Schema::V3::ISupplementaryData*>(supdata.get());
        }
        int version() const override { return version_; }

        State() = delete;
        State(const int version_, const std::string colorname, const CPlayerBattleCallback* battle_);

        void onActiveStack(const CStack* astack);
        void onBattleStacksAttacked(const std::vector<BattleStackAttacked> &bsa);
        void onBattleTriggerEffect(const BattleTriggerEffect &bte);
        void onBattleEnd(const BattleResult *br);

        // Subsequent versions may override this if they only change
        // the data type of encoded values (i.e. have their own HEX_ENCODING)
        virtual void encodeHex(Hex* hex);
        virtual void encodeStack(Stack* stack);
        virtual void verify();

        const int version_;
        Schema::BattlefieldState bfstate = {};
        Schema::ActionMask actmask = {};
        Schema::AttentionMask attnmask = {};
        std::unique_ptr<SupplementaryData> supdata;
        std::vector<std::shared_ptr<AttackLog>> attackLogs = {};
        std::shared_ptr<const Battlefield> battlefield = nullptr;
        std::unique_ptr<Action> action = nullptr;
        const std::string colorname;
        const CPlayerBattleCallback* battle; // survives discard()
        const BattlePerspective::BattlePerspective side;
        const std::pair<int, int> initialArmyValues;
        bool isMorale = false;

        static std::vector<float> InitNullStack();
        const std::vector<float> nullstack;
    };
}
