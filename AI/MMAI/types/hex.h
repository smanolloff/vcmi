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
