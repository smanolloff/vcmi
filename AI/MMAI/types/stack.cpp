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

#include "stack.h"
#include <algorithm>

namespace MMAI {
    // static
    StackAttrs Stack::NAAttrs() {
        auto res = StackAttrs{};
        res.fill(ATTR_NA);
        return res;
    }

    // Require an explicit argument:
    // this is to prevent invoking the "default" constructor by mistake:
    // (there should be only 1 invalid stack - the global INVALID_STACK)
    Stack::Stack(bool donotuse)
    : cstack(nullptr), attrs(NAAttrs()) {};

    Stack::Stack(const CStack * cstack_, const StackAttrs attrs_)
    : cstack(cstack_), attrs(attrs_) {
        ASSERT(cstack, "null cstack");
    };
}

