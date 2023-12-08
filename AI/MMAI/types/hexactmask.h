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
