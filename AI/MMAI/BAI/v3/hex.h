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

#include "CStack.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"

#include "battle/CBattleInfoEssentials.h"
#include "battle/CObstacleInstance.h"
#include "battle/ReachabilityInfo.h"
#include "constants/Enumerations.h"
#include "schema/v3/types.h"
#include "./hexactmask.h"
#include "./stack.h"
#include <memory>

namespace MMAI::BAI::V3 {
    using namespace Schema::V3;
    using HexActionMask = std::bitset<EI(HexAction::_count)>;
    using HexStateMask = std::bitset<EI(HexState::_count)>;
    using HexActionHex = std::array<BattleHex, 12>;

    struct ActiveStackInfo {
        const std::shared_ptr<Stack> stack;
        const bool canshoot;
        const std::shared_ptr<ReachabilityInfo> rinfo;

        ActiveStackInfo(
            const std::shared_ptr<Stack> stack_,
            const bool canshoot_,
            const std::shared_ptr<ReachabilityInfo> rinfo_
        ) : stack(stack_), canshoot(canshoot_), rinfo(rinfo_) {};
    };

    /**
     * A wrapper around BattleHex. Differences:
     *
     * x is 0..14     (instead of 0..16),
     * id is 0..164  (instead of 0..177)
     */
    class Hex : public Schema::V3::IHex {
    public:
        static int CalcId(const BattleHex &bh);
        static std::pair<int, int> CalcXY(const BattleHex &bh);

        Hex(
            const BattleHex &bh,
            const EAccessibility accessibility,
            const EGateState gatestate,
            const std::vector<std::shared_ptr<const CObstacleInstance>> &obstacles,
            const std::map<BattleHex, std::shared_ptr<Stack>> &hexstacks,
            const std::shared_ptr<ActiveStackInfo> &astackinfo
        );

        // IHex impl
        const HexAttrs& getAttrs() const override;
        int getAttr(HexAttribute a) const override;

        const BattleHex bhex;
        std::shared_ptr<const Stack> stack;
        HexAttrs attrs;
        HexActionMask actmask;   // for active stack only
        HexStateMask statemask;  //

        std::string name() const;
        int attr(HexAttribute a) const;
    private:
        void setattr(HexAttribute a, int value);
        void addAction(HexAction action);
        void addState(HexState state);
        void finalize();

        void setStateMask(
            const EAccessibility accessibility,
            const std::vector<std::shared_ptr<const CObstacleInstance>> &obstacles,
            bool side
        );

        void setActionMask(
            const std::shared_ptr<ActiveStackInfo> &astackinfo,
            const std::map<BattleHex, std::shared_ptr<Stack>> &hexstacks
        );

        static HexActionHex NearbyBattleHexes(const BattleHex &bh);
    };
}
