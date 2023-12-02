#pragma once
#include "../common.h"
#include "../mytypes.h"
#include "action_enums.h"

namespace MMAI {
    /**
     * A list of booleans for a single hex:
     *
     * [0] can move to Hex.
     * [1] can move to Hex + attack stack 1.
     * ...
     * [7] can move to Hex + attack stack 7.
     */
    using HexActMask = std::array<bool, EI(HexAction::count)>;
    static_assert(EI(HexAction::count) == 8, "doc assumes 8 hex actions");

    struct ActMask {
        bool retreat = false;
        bool defend = false;
        bool wait = false;

        /**
         * A list of masks for each hex
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
