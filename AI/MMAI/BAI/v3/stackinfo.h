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

#include "battle/ReachabilityInfo.h"
#include "schema/v3/types.h"

namespace MMAI::BAI::V3 {
    using DmgMod = Schema::V3::DmgMod;

    struct StackInfo {
        int speed;
        bool canshoot;
        DmgMod meleemod;
        bool noDistancePenalty;
        std::shared_ptr<ReachabilityInfo> rinfo;

        StackInfo(
            int speed_,
            bool canshoot_,
            DmgMod meleemod_,
            bool noDistancePenalty_,
            std::shared_ptr<ReachabilityInfo> rinfo_
        ) : speed(speed_),
            canshoot(canshoot_),
            meleemod(meleemod_),
            noDistancePenalty(noDistancePenalty_),
            rinfo(rinfo_) {};
    };
}
