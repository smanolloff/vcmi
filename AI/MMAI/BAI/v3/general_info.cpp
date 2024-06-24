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

#include "CStack.h"
#include "./general_info.h"

namespace MMAI::BAI::V3 {
    // static
    ArmyValues GeneralInfo::CalcTotalArmyValues(const CPlayerBattleCallback* battle) {
        int res0 = 0;
        int res1 = 0;
        for (auto &stack : battle->battleGetStacks()) {
            stack->unitSide() == 0
                ? res0 += stack->getCount() * stack->unitType()->getAIValue()
                : res1 += stack->getCount() * stack->unitType()->getAIValue();
        }
        return {res0, res1};
    }

    GeneralInfo::GeneralInfo(
        const CPlayerBattleCallback* battle,
        ArmyValues initialArmyValues_
    ) : initialArmyValues(initialArmyValues_),
        currentArmyValues(CalcTotalArmyValues(battle))
        {};
}
