#pragma once

#include "../common.h"

namespace MMAI {
    enum class HexState : int {
        FREE_REACHABLE,     // 0
        FREE_UNREACHABLE,   // 1
        OBSTACLE,           // 2
        FRIENDLY_STACK_0,   // 3
        FRIENDLY_STACK_1,   // 4
        FRIENDLY_STACK_2,   // 5
        FRIENDLY_STACK_3,   // 6
        FRIENDLY_STACK_4,   // 7
        FRIENDLY_STACK_5,   // 8
        FRIENDLY_STACK_6,   // 9
        ENEMY_STACK_0,      // 10
        ENEMY_STACK_1,      // 11
        ENEMY_STACK_2,      // 12
        ENEMY_STACK_3,      // 13
        ENEMY_STACK_4,      // 14
        ENEMY_STACK_5,      // 15
        ENEMY_STACK_6,      // 16
        count
    };

    static_assert(EI(HexState::count) - EI(HexState::FRIENDLY_STACK_0) == 14, "Code assumes 14 stack states");
}
