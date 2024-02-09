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
#include "common.h"
#include "export.h"

namespace MMAI {
    constexpr int ATTR_NA = -1;

    enum class StackAttr : int {
        Quantity,
        Attack,
        Defense,
        Shots,
        DmgMin,
        DmgMax,
        HP,
        HPLeft,
        Speed,
        Waited,
        QueuePos,  // 0=active stack
        RetaliationsLeft,
        Side,  // 0=attacker, 1=defender
        Slot,  // 0..6
        CreatureType,  //0..144
        AIValue,
        IsActive, // 1=true
        count
    };

    static_assert(EI(StackAttr::count) == Export::N_STACK_ATTRS);

    /**
     * A list attributes for a single stack
     *
     *  [0] qty,    [1] att,    [2] def, [3] shots,
     *  [4] mindmg, [5] maxdmg, [6] HP,  [7] HP left,
     *  [8] speed,  [9] waited, ...
     */
    using StackAttrs = std::array<int, EI(StackAttr::count)>;

    struct Stack {
        Stack() = delete;
        Stack(bool donotuse);
        Stack(const CStack * cstack_, const StackAttrs attrs_);

        static StackAttrs NAAttrs();

        const CStack * cstack;
        const StackAttrs attrs = StackAttrs();
    };

    static const auto INVALID_STACK = std::make_shared<Stack>(true);
}
