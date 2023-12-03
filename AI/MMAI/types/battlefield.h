#pragma once

#include "CCallback.h"
#include "common.h"
#include "export.h"
#include "types/hex.h"

namespace MMAI {
    using Hexes = std::array<Hex, BF_SIZE>;

    enum class StackAttr : int {
        Quantity,
        Attack,
        Defense,
        Shots,
        DmgMin,
        DmgMax,
        HP,
        HPLeft,
        Speed,
        Waited,
        count
    };

    /**
     * A list attributes for a single stack
     *
     *  [0] qty,    [1] att,    [2] def, [3] shots,
     *  [4] mindmg, [5] maxdmg, [6] HP,  [7] HP left,
     *  [8] speed,  [9] waited
     */
    using StackAttrs = std::array<int, EI(StackAttr::count)>;

    struct Stack {
        Stack() {};
        Stack(const CStack * stack_, const StackAttrs attrs_)
        : stack(stack_), attrs(attrs_) {};

        const CStack * stack = nullptr;
        const StackAttrs attrs = StackAttrs();
    };

    /**
     * List of stacks by slot:
     *  [0..7] friendly stacks,
     *  [8..13] enemy stacks
     */
    using Stacks = std::array<std::unique_ptr<Stack>, 14>;

    /**
     * A container for all 165 Hex tiles.
     */
    struct Battlefield {
        static void initHex(
            Hex &hex,
            BattleHex bh,
            CBattleCallback* cb,
            const CStack* astack,
            const AccessibilityInfo &ainfo,
            const ReachabilityInfo &rinfo
        );

        static Hexes initHexes(CBattleCallback* cb, const CStack* astack);
        static Stacks initStacks(const CBattleCallback * cb);

        Battlefield(CBattleCallback* cb, const CStack* astack_) :
            astack(astack_),
            hexes(initHexes(cb, astack_)),
            stacks(initStacks(cb))
            {};

        const CStack* const astack;
        const Hexes hexes;
        const Stacks stacks;
        const Export::State exportState();
        const Export::ActMask exportActMask();
        const CStack * getEnemyStackBySlot(int slot);
    };

    // Sync check hard-coded values in Export
    static_assert(std::tuple_size<Export::State>::value ==
        std::tuple_size<Hexes>::value
        + std::tuple_size<Stacks>::value * std::tuple_size<StackAttrs>::value
        + 1 // cur stack
    );

    static_assert(std::tuple_size<Export::ActMask>::value == N_ACTIONS);

    // static std::string hexStateName(HexState hs) {
    //     switch(hs) {
    //     case HexState::FREE_REACHABLE:   return "FREE_REACHABLE";
    //     case HexState::FREE_UNREACHABLE: return "FREE_UNREACHABLE";
    //     case HexState::OBSTACLE:         return "OBSTACLE";
    //     case HexState::FRIENDLY_STACK_0: return "FRIENDLY_STACK_0";
    //     case HexState::FRIENDLY_STACK_1: return "FRIENDLY_STACK_1";
    //     case HexState::FRIENDLY_STACK_2: return "FRIENDLY_STACK_2";
    //     case HexState::FRIENDLY_STACK_3: return "FRIENDLY_STACK_3";
    //     case HexState::FRIENDLY_STACK_4: return "FRIENDLY_STACK_4";
    //     case HexState::FRIENDLY_STACK_5: return "FRIENDLY_STACK_5";
    //     case HexState::FRIENDLY_STACK_6: return "FRIENDLY_STACK_6";
    //     case HexState::ENEMY_STACK_0:    return "ENEMY_STACK_0";
    //     case HexState::ENEMY_STACK_1:    return "ENEMY_STACK_1";
    //     case HexState::ENEMY_STACK_2:    return "ENEMY_STACK_2";
    //     case HexState::ENEMY_STACK_3:    return "ENEMY_STACK_3";
    //     case HexState::ENEMY_STACK_4:    return "ENEMY_STACK_4";
    //     case HexState::ENEMY_STACK_5:    return "ENEMY_STACK_5";
    //     case HexState::ENEMY_STACK_6:    return "ENEMY_STACK_6";
    //     default:
    //         throw std::runtime_error("Unknown state: " + std::to_string(EI(hs)));
    //     }
    // };

    // NOTE: function is very inefficient and must not be called often
    // static HexState hexStateValue(std::string name) {
    //     // Invert the key-value pairs
    //     for (int i=0; i<EI(HexState::count); i++) {
    //         auto hs = HexState(i);
    //         if (hexStateName(hs) == name)
    //             return hs;
    //     }

    //     throw std::runtime_error("Unknown state name: " + name);
    // }
}
