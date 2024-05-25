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

#include "build_action_result.h"
#include "../common.h"
#include "export.h"

namespace MMAI {
    void BuildActionResult::setAction(BattleAction ba) {
        if (errmask > 0) return;
        ASSERT(!battleAction, "battleAction already set: " + std::to_string(EI(ba.actionType)));
        battleAction = std::make_shared<BattleAction>(ba);
    }

    void BuildActionResult::addError(Export::ErrType errtype) {
        auto& [flag, name, msg] = Export::ERRORS.at(errtype);
        errmask |= flag;
        errmsgs.emplace_back("(" + name + ") " + msg);
        battleAction = nullptr;
    }
}
