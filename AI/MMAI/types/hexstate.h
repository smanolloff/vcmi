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

#include "../common.h"

namespace MMAI {
    // XXX: there *can* be a stack on top of a FREE_REACHABLE hex only if:
    //      * it's the back hex of the active stack
    //      * AND the stack can move there
    // XXX: Can't encode the stack slot here -- a slot *must* be present in
    //      the stack, because in the case of a REACHABLE back hex of cur stack
    //      there is no way to indicate the slot.

    enum class HexState : int {
        INVALID,  // no hex
        OBSTACLE,
        OCCUPIED, // alive stack
        FREE_UNREACHABLE,
        FREE_REACHABLE,
        count
    };
}
