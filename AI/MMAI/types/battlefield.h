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
#include "export.h"
#include "hex.h"

namespace MMAI {
    using Hexes = std::array<std::array<Hex, BF_XMAX>, BF_YMAX>;

    constexpr int QSIZE = 15;
    using Queue = std::vector<uint32_t>;
    using ReachabilityInfos = std::map<const CStack*, std::shared_ptr<ReachabilityInfo>>;
    using ShooterInfos = std::map<const CStack*, bool>;
    using XY = std::pair<int, int>;

    /**
     * A container for all 165 Hex tiles.
     */
    struct Battlefield {
        static BattleHex AMoveTarget(BattleHex &bh, HexAction &action);

        static bool IsReachable(
            const BattleHex &bh,
            const CStack* cstack,
            const ReachabilityInfos &rinfos
        );

        static HexAction AttackAction(
            const BattleHex &nbh,
            const BattleHex &bh,
            const BattleHex &obh
        );

        static void ProcessNeighbouringHexes(
            Hex &hex,
            const CStack* astack,
            const std::vector<const CStack*> &allstacks,
            const AccessibilityInfo &ainfo, // accessibility info for active stack
            const ReachabilityInfos &rinfos
        );

        static Hex InitHex(
            const int id,
            const std::vector<const CStack*> &allstacks,
            const CStack* astack,
            const Queue &queue,
            const AccessibilityInfo &ainfo,
            const ReachabilityInfos &rinfos,
            const ShooterInfos &sinfos
        );

        static Hexes InitHexes(CBattleCallback* cb, const CStack* astack);
        static Queue GetQueue(CBattleCallback* cb);
        static void AddToExportState(Export::State &state, std::array<const CStack*, 7> &stacks);


        Battlefield(CBattleCallback* cb, const CStack* astack_) :
            astack(astack_),
            hexes(InitHexes(cb, astack_))
            {};

        Hexes hexes; // not const due to offTurnUpdate
        const CStack* const astack;
        const Export::State exportState();
        const Export::ActMask exportActMask();

        // needed only for getting stack by slot
        std::array<const CStack*, 7> enemystacks;

        void offTurnUpdate(CBattleCallback* cb);
    };
}
