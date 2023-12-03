#include "battle/BattleAction.h"
#include "mytypes.h"

namespace MMAI {
  struct BuildActionResult {
    std::shared_ptr<BattleAction> battleAction; // can be nullptr!
    MMAIExport::ErrMask errmask;
    std::vector<std::string> errmsgs;

    void setAction(BattleAction ba);
    void addError(MMAIExport::ErrType errtype);
  };
}
