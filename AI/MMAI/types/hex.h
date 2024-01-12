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
#include "battle/BattleHex.h"
#include "hexactmask.h"
#include "hexstate.h"
#include "stack.h"

namespace MMAI {

    /**
     * A wrapper around BattleHex. Differences:
     *
     * x is 0..14     (instead of 0..16),
     * id is 0..164  (instead of 0..177)
     */
    struct Hex {
        static int calcId(const BattleHex &bh);

        Hex() {};

        BattleHex bhex;
        int id;
        std::shared_ptr<Stack> stack = INVALID_STACK; // stack occupying this hex

        HexState state;
        HexActMask hexactmask;

        std::string name() const;
    };
}
