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
