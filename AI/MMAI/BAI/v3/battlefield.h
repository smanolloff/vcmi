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

#include "BAI/v3/stack.h"
#include "CCallback.h"
#include "battle/CObstacleInstance.h"
#include "battle/ReachabilityInfo.h"
#include "common.h"

#include "./hex.h"
#include "./stackinfo.h"
#include "./general_info.h"

#include "schema/base.h"
#include "schema/v3/constants.h"

namespace MMAI::BAI::V3 {
    using Stacks = std::array<std::array<std::shared_ptr<Stack>, MAX_STACKS_PER_SIDE>, 2>;
    using Hexes = std::array<std::array<std::shared_ptr<Hex>, BF_XMAX>, BF_YMAX>;
    using HexStacks = std::map<BattleHex, const std::shared_ptr<Stack>>;
    using HexObstacles = std::map<BattleHex, std::shared_ptr<const CObstacleInstance>>;
    using XY = std::pair<int, int>;
    using DirHex = std::vector<std::pair<BattleHex::EDir, BattleHex>>;
    using HexActionHex = std::array<BattleHex, 12>;

    constexpr int QSIZE = 15;

    class Battlefield {
    public:
        static std::shared_ptr<const Battlefield> Create(
            const CPlayerBattleCallback* battle,
            const CStack* astack_,
            std::pair<int, int> initialArmyValues,
            bool isMorale
        );

        Battlefield(
            std::shared_ptr<GeneralInfo> info,
            std::shared_ptr<Hexes> hexes,
            std::shared_ptr<Stacks> stacks,
            const CStack* astack
        );

        const std::shared_ptr<GeneralInfo> info;
        const std::shared_ptr<Hexes> hexes;
        const std::shared_ptr<Stacks> stacks; // XXX: may or may not hold hold the active stack
        const CStack* astack;
    private:

        static bool IsReachable(
            const BattleHex &bh,
            const StackInfo *stackinfo
        );

        static HexAction HexActionFromHexes(
            const BattleHex &nbh,
            const BattleHex &bh,
            const BattleHex &obh
        );

        static void ProcessNeighbouringHexes(
            Hex &hex,
            const CStack* astack,
            const StackInfo &astackinfo,
            const HexStacks &hexstacks
        );

        static HexActionHex NearbyHexes(BattleHex &bh);
        static std::pair<DirHex, BattleHex> NearbyHexesForHexAttackableBy(BattleHex &bh, const CStack* cstack);

        static std::shared_ptr<Hex> InitHex(
            const int id,
            const AccessibilityInfo &ainfo,
            const std::shared_ptr<StackInfo> astackinfo,
            const HexStacks &hexstacks,
            const HexObstacles &hexobstacles
        );

        static std::shared_ptr<Stacks> InitStacks(const CPlayerBattleCallback* battle, const CStack* astack, bool isMorale);
        static std::shared_ptr<Hexes> InitHexes(const CPlayerBattleCallback* battle, const std::shared_ptr<Stacks> stacks, bool ended);
        static Queue GetQueue(const CPlayerBattleCallback* battle, const CStack* astack, bool isMorale);
        static int BaseMeleeMod(const CPlayerBattleCallback* battle, const CStack* cstack);
        static int BaseRangeMod(const CPlayerBattleCallback* battle, const CStack* cstack);
    };
}
