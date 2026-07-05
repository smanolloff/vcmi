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

#include "battle/AccessibilityInfo.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/ReachabilityInfo.h"

namespace MMAI::BAI::V14
{

class Cache
{
public:
    explicit Cache(const CPlayerBattleCallback * battle);

    AccessibilityInfo getAccessibility();
    ReachabilityInfo getReachability(const CStack * cstack);
    bool isRUFR(const CStack * cstack, const BattleHex & bh);

    Cache(const Cache &) = delete;
    Cache & operator=(const Cache &) = delete;
    Cache(Cache &&) = delete;
    Cache & operator=(Cache &&) = delete;

private:
    const CPlayerBattleCallback * battle;
    std::unordered_map<const CStack *, ReachabilityInfo> rcache;
    std::unordered_map<const CStack *, std::array<bool, GameConstants::BFIELD_SIZE>> rufrHexes;
    std::unique_ptr<AccessibilityInfo> acache;
};
}
