#pragma once

#include "battle/BattleAction.h"
#include "export.h"

namespace MMAI {
    struct BuildActionResult {
        std::shared_ptr<BattleAction> battleAction; // can be nullptr!
        Export::ErrMask errmask;
        std::vector<std::string> errmsgs;

        void setAction(BattleAction ba);
        void addError(Export::ErrType errtype);
    };
}
