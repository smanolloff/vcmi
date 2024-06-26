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
#include "CStack.h"
#include "battle/ReachabilityInfo.h"
#include "schema/v3/types.h"

namespace MMAI::BAI::V3 {

    struct StackInfo {
        const std::shared_ptr<const Stack> stack;
        const int speed;
        const bool canshoot;
        const std::unique_ptr<ReachabilityInfo> rinfo;

        StackInfo(
            const std::shared_ptr<const Stack> stack_,
            const bool canshoot_,
            const ReachabilityInfo rinfo_
        ) : stack(stack_),
            speed(stack_->cstack->getMovementRange()),
            canshoot(canshoot_),
            rinfo(std::make_unique<ReachabilityInfo>(rinfo_))
            {};
    };
}
