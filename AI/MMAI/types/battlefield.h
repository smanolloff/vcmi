#pragma once

#include "CCallback.h"
#include "common.h"
#include "export.h"
#include "hex.h"
#include "stack.h"

namespace MMAI {
    using Hexes = std::array<Hex, BF_SIZE>;

    constexpr int QSIZE = 15;
    using Queue = std::vector<uint32_t>;

    /**
     * A container for all 165 Hex tiles.
     */
    struct Battlefield {
        static void initHex(
            Hex &hex,
            BattleHex bh,
            const CStack* astack,
            const AccessibilityInfo &ainfo,
            const ReachabilityInfo &rinfo
        );

        static void updateMaskForSpecialMoveCase(Hex &hex, const CStack* astack, const AccessibilityInfo &ainfo);
        static void updateMaskForSpecialDefendCase(Hex &hex, const CStack* astack);
        static void updateMaskForAttackCases(const CStack* astack, const Hexes &hexes, Hex &ehex);

        static Hexes initHexes(CBattleCallback* cb, const CStack* astack);
        static Stack initStack(CBattleCallback* cb, const Queue &q, const CStack* cstack);
        static Queue getQueue(CBattleCallback* cb);

        Battlefield(CBattleCallback* cb, const CStack* astack_) :
            astack(astack_),
            hexes(initHexes(cb, astack_))
            {};

        const CStack* const astack;
        Hexes hexes;
        const Export::State exportState();
        const Export::ActMask exportActMask();

        // needed only for getting stack by slot
        std::array<const CStack*, 7> enemystacks;

        void offTurnUpdate(CBattleCallback* cb);
    };


    // Sync check hard-coded values in Export
    static_assert(Export::N_HEX_ATTRS == 1 + std::tuple_size<StackAttrs>::value);

    static_assert(std::tuple_size<Export::State>::value ==
        std::tuple_size<Hexes>::value * Export::N_HEX_ATTRS
    );

    static_assert(std::tuple_size<Export::ActMask>::value == N_ACTIONS);
}
