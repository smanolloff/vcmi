#pragma once

#include "export.h"
#include "types/action_enums.h"
#include "types/battlefield.h"

namespace MMAI {
    static_assert(EI(HexAction::MOVE_AND_ATTACK_0) == 0, "code assumes corresponding stack slot");

    // forward declaration needed to break cyclic dependency

    /**
     * Wrapper around Export::Action
     */
    struct Action {
        Action(const Export::Action action_, const Battlefield * bf) :
            action(action_),
            hex(initHex(bf)),
            hexaction(initHexAction())
            {};

        const Export::Action action;
        const Hex * hex; // XXX: must come after action
        const HexAction hexaction; // XXX: must come after action

        const Hex * initHex(const Battlefield * bf);
        HexAction initHexAction();
        std::string name() const;
    };
}
