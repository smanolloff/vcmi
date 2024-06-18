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

#include "CCallback.h"
#include "battle/ReachabilityInfo.h"
#include "common.h"

#include "BAI/v1/hex.h"
#include "BAI/v1/stackinfo.h"

#include "schema/base.h"

namespace MMAI::BAI::V1 {
    using Hexes = std::array<std::array<std::unique_ptr<Hex>, BF_XMAX>, BF_YMAX>;
    using Queue = std::vector<uint32_t>;
    using HexStacks = std::map<BattleHex, const CStack*>;
    using XY = std::pair<int, int>;
    using DirHex = std::vector<std::pair<BattleHex::EDir, BattleHex>>;
    using HexActionHex = std::array<BattleHex, 12>;
    using StackInfos = std::map<const CStack*, StackInfo>;

    constexpr int QSIZE = 15;

    class Battlefield {
    public:
        static BattleHex AMoveTarget(BattleHex &bh, HexAction &action);

        Battlefield(const CPlayerBattleCallback* battle, const CStack* astack_, int percentValue, bool isMorale);

        const CStack* const astack;
        const Hexes hexes;

    private:
        static bool IsReachable(
            const BattleHex &bh,
            const StackInfo &stackinfo
        );

        static HexAction HexActionFromHexes(
            const BattleHex &nbh,
            const BattleHex &bh,
            const BattleHex &obh
        );

        static void ProcessNeighbouringHexes(
            Hex &hex,
            const CStack* astack,
            const StackInfos &stackinfos,
            const HexStacks &hexstacks
        );

        static HexActionHex NearbyHexes(BattleHex &bh);
        static std::pair<DirHex, BattleHex> NearbyHexesForHexAttackableBy(BattleHex &bh, const CStack* cstack);

        static std::unique_ptr<Hex> InitHex(
            const int id,
            const CStack* astack,
            const int percentValue,
            const Queue &queue,
            const AccessibilityInfo &ainfo,
            const StackInfos &stackinfos,
            const HexStacks &hexstacks
        );

        static Hexes InitHexes(const CPlayerBattleCallback* battle, const CStack* astack, int percentValue, bool isMorale);
        static Queue GetQueue(const CPlayerBattleCallback* battle, const CStack* astack, bool isMorale);
    };
}
