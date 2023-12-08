#include "types/hex.h"
#include "types/action.h"
#include <stdexcept>

namespace MMAI {
    // static
    std::unique_ptr<Hex> Action::initHex(const Export::Action &a, const Battlefield * bf) {
        // Control actions (<0) should never reach here
        ASSERT(a >= 0 && a < N_ACTIONS, "Invalid action: " + std::to_string(a));

        auto res = BattleHex();
        auto i = a - EI(NonHexAction::count);

        return (i < 0) ? nullptr : std::make_unique<Hex>(bf->hexes[i / EI(HexAction::count)]);
    }

    // static
    HexAction Action::initHexAction(const Export::Action &a, const Battlefield * bf) {
        if(a < EI(NonHexAction::count)) return HexAction(-1); // a is about a hex
        return HexAction((a-EI(NonHexAction::count)) % EI(HexAction::count));
    }

    Action::Action(const Export::Action action_, const Battlefield * bf)
        : action(action_)
        , hex(initHex(action_, bf))
        , hexaction(initHexAction(action_, bf)) {};

    std::string Action::name() const {
        if (action == Export::ACTION_RETREAT)
            return "Retreat";
        else if (action == Export::ACTION_WAIT)
            return "Wait";

        ASSERT(hex, "hex is null");

        auto ha = (action - EI(NonHexAction::count)) % EI(HexAction::count);
        auto res = std::string{};

        auto slot = hex->stack->attrs[EI(StackAttr::Slot)];
        auto stackstr = hex->stack->cstack ? "\033[31m#" + std::to_string(slot+1) + "\033[0m" : "?";

        switch (HexAction(ha)) {
        break; case HexAction::MOVE:     res = "Move to " + hex->name();
        break; case HexAction::SHOOT:    res = "Attack " + stackstr + " " + hex->name() + " (ranged)";
        break; case HexAction::MELEE_TL: res = "Attack " + stackstr + " " + hex->name() + " from top-left";
        break; case HexAction::MELEE_TR: res = "Attack " + stackstr + " " + hex->name() + " from top-right";
        break; case HexAction::MELEE_R:  res = "Attack " + stackstr + " " + hex->name() + " from right";
        break; case HexAction::MELEE_BR: res = "Attack " + stackstr + " " + hex->name() + " from bottom-right";
        break; case HexAction::MELEE_BL: res = "Attack " + stackstr + " " + hex->name() + " from bottom-left";
        break; case HexAction::MELEE_L:  res = "Attack " + stackstr + " " + hex->name() + " from left";
        break; case HexAction::MELEE_T:  res = "Attack " + stackstr + " " + hex->name() + " from top";
        break; case HexAction::MELEE_B:  res = "Attack " + stackstr + " " + hex->name() + " from bottom";
        break; default:
            throw std::runtime_error("Unexpected hexaction: " + std::to_string(ha));
        }

        return res;
    }

}

