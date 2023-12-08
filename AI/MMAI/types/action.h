#pragma once

#include "export.h"
#include "types/hexaction.h"
#include "types/battlefield.h"

namespace MMAI {
    /**
     * Wrapper around Export::Action
     */
    struct Action {
        static std::unique_ptr<Hex> initHex(const Export::Action &a, const Battlefield * bf);
        static HexAction initHexAction(const Export::Action &a, const Battlefield * bf);

        Action(const Export::Action action_, const Battlefield * bf);

        const Export::Action action;
        const std::unique_ptr<Hex> hex;
        const HexAction hexaction; // XXX: must come after action

        std::string name() const;
    };
}
