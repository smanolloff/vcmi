#include "action_result.h"
#include "../common.h"
#include "mytypes.h"

namespace MMAI {
  void ActionResult::setAction(BattleAction ba) {
    if (errmask > 0) return;
    ASSERT(!battleAction, "battleAction already set: " + std::to_string(EI(ba.actionType)));
    battleAction = std::make_shared<BattleAction>(ba);
  }

  void ActionResult::addError(MMAIExport::ErrType errtype) {
    auto& [flag, name, msg] = MMAIExport::ERRORS.at(errtype);
    errmask |= flag;
    errmsgs.emplace_back("(" + name + ") " + msg);
    battleAction = nullptr;
  }
}
