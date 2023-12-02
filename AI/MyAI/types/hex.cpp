#include "CCallback.h"
#include "CStack.h"
#include "battle/BattleHex.h"
#include "hex.h"

namespace MMAI {
    Hex::Hex(
        const CBattleCallback * cb,
        const CStack * astack,
        const AccessibilityInfo &ainfo,
        const ReachabilityInfo &rinfo,
        const BattleHex bh
    ) {
        battleHex = bh;
        id = Hex::calcId(bh);

        const CStack * tmpstack;

        switch(ainfo[bh.hex]) {
        case LIB_CLIENT::EAccessibility::ACCESSIBLE:
            if (rinfo.distances[bh] <= astack->speed()) {
                state = HexState::FREE_REACHABLE;
                hexactmask[EI(HexAction::MOVE)] = true;
            } else {
                state = HexState::FREE_UNREACHABLE;
            }

            break;
        case LIB_CLIENT::EAccessibility::OBSTACLE:
            state = HexState::OBSTACLE;
            break;
        case LIB_CLIENT::EAccessibility::ALIVE_STACK:
            tmpstack = cb->battleGetStackByPos(bh, true);
            state = (tmpstack->unitSide() == astack->unitSide())
                ? HexState(EI(HexState::FRIENDLY_STACK_0) + tmpstack->unitSlot())
                : HexState(EI(HexState::ENEMY_STACK_0) + tmpstack->unitSlot());

            // Handle moving to the "back" hex of two-hex active stacks:
            // (xxooxx -> xooxx)
            //
            // If this is the active stack
            // AND this hex is the "occupied" (ie. the "back") hex of this (2-hex) stack
            // AND it is is accessible by a 2-hex stack
            // => can move to this hex.
            if (
              astack->unitId() == tmpstack->unitId() &&
              astack->occupiedHex() == bh &&
              ainfo.accessible(bh, true, astack->unitSide())
            ) {
              hexactmask[EI(HexAction::MOVE)] = true;
            }

            break;

        // XXX: unhandled hex states
        // case LIB_CLIENT::EAccessibility::DESTRUCTIBLE_WALL:
        // case LIB_CLIENT::EAccessibility::GATE:
        // case LIB_CLIENT::EAccessibility::UNAVAILABLE:
        // case LIB_CLIENT::EAccessibility::SIDE_COLUMN:
        default:
          throw std::runtime_error(
            "Unexpected hex accessibility for hex "+ std::to_string(bh.hex) + ": "
              + std::to_string(static_cast<int>(ainfo[bh.hex]))
          );
        }
    };

    std::string Hex::name() const {
        return "(" + std::to_string(1 + id % BF_XMAX) + "," + std::to_string(1 + id % BF_YMAX) + ")";
    }
}
