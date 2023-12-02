#pragma once
#include "../common.h"

// There is a cyclic dependency if those are placed in action.h:
// action.h -> battlefield.h -> hex.h -> actmask.h -> action.h
namespace MMAI {
    enum class NonHexAction : int {
        RETREAT,
        DEFEND,
        WAIT,
        count
    };

    enum class HexAction : int {
        MOVE_AND_ATTACK_0,
        MOVE_AND_ATTACK_1,
        MOVE_AND_ATTACK_2,
        MOVE_AND_ATTACK_3,
        MOVE_AND_ATTACK_4,
        MOVE_AND_ATTACK_5,
        MOVE_AND_ATTACK_6,
        MOVE,
        count
    };

    static_assert(EI(HexAction::MOVE_AND_ATTACK_0) == 0, "HexAction must index must correspond to stack");

    constexpr int N_ACTIONS = EI(NonHexAction::count) + EI(HexAction::count)*BF_SIZE;

}
