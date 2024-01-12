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

#include "hexaction.h"

namespace MMAI {
    /**
     * A list of booleans for a single hex (see HexAction)
     */
    using HexActMask = std::array<bool, EI(HexAction::count)>;

    struct ActMask {
        bool retreat = false;
        bool wait = false;

        /**
         * A list of HexActMask objects
         *
         * [0] HexActMask for hex 0
         * [1] HexActMask for hex 1
         * ...
         * [164] HexActMask for hex 164
         */
        std::array<HexActMask, BF_SIZE> hexactmasks = {};
    };
    static_assert(BF_SIZE == 165, "doc assumes BF_SIZE=165");
}
