#include "types/hex.h"
#include "types/action.h"

namespace MMAI {
    const Hex * Action::initHex(const Battlefield * bf) {
        // Control actions (<0) should never reach here
        ASSERT(action >= 0 && action < N_ACTIONS, "Invalid action: " + std::to_string(action));

        auto res = BattleHex();
        auto i = action - EI(NonHexAction::count);

        return (i < 0) ? nullptr : &bf->hexes[i / EI(HexAction::count)];
    }

    HexAction Action::initHexAction() {
        if(!hex) return HexAction(-1);

        return HexAction((action - EI(NonHexAction::count)) % EI(HexAction::count));
    }

    std::string Action::name() const {
        static_assert(EI(NonHexAction::count) == 3, "3 non-hex actions are assumed");

        if (action == MMAIExport::ACTION_RETREAT)
            return "Retreat";
        else if (action == MMAIExport::ACTION_DEFEND)
            return "Defend";
        else if (action == MMAIExport::ACTION_WAIT)
            return "Wait";

        ASSERT(hex, "hex is null");

        auto ha = (action - 3) % EI(HexAction::count);
        auto res = "Move to " + hex->name();

        if (ha < 7)
            res += " and attack \033[31m#" + std::to_string(ha+1) + "\033[0m";

        return res;
    }

}

