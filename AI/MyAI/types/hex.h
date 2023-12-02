#pragma once
#include "CCallback.h"
#include "battle/BattleHex.h"
#include "actmask.h"
#include "hexstate.h"

namespace MMAI {
    /**
     * A wrapper around BattleHex. Differences:
     *
     * x is 0..14     (instead of 0..16),
     * id is 0..164  (instead of 0..177)
     */
    struct Hex {
        static int calcId(const BattleHex &bh) {
            ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.hex));
            return bh.getX()-1 + bh.getY()*BF_XMAX;
        }

        Hex();
        Hex(
            const CBattleCallback * cb,
            const CStack * astack,
            const AccessibilityInfo &ainfo,
            const ReachabilityInfo &rinfo,
            const BattleHex bh
        );

        // VCMI battle hex
        BattleHex bhex;

        int id;
        HexState state;
        HexActMask hexactmask;

        std::string name() const;
    };
}
