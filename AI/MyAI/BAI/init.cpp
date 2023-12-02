#include "BAI.h"
#include "battle/BattleHex.h"
#include "mytypes.h"
#include "vstd/CLoggerBase.h"
#include <memory>

// Contains initialization-related code

MMAI_NS_BEGIN

BAI::BAI() : side(-1), wasWaitingForRealize(false), wasUnlockingGs(false) {
    info("+++ constructor +++");

    // Prevent null pointer dereferencing
    action = std::make_unique<Action>(Action{ACTION_UNSET, nullptr});
    result = std::make_unique<MMAIExport::Result>();
}

BAI::~BAI() {
    info("--- destructor ---");

    if(cb) {
        // copied from BattleAI, not sure if needed
        cb->waitTillRealize = wasWaitingForRealize;
        cb->unlockGsWhenWaiting = wasUnlockingGs;
    }
}

void BAI::initBattleInterface(
    std::shared_ptr<Environment> ENV,
    std::shared_ptr<CBattleCallback> CB
) {
    error("*** initBattleInterface -- BUT NO F_GetAction ***");
    myInitBattleInterface(ENV, CB, nullptr);
}

void BAI::initBattleInterface(
    std::shared_ptr<Environment> ENV,
    std::shared_ptr<CBattleCallback> CB,
    AutocombatPreferences autocombatPreferences
) {
    error("*** initBattleInterface -- BUT NO F_GetAction ***");
    myInitBattleInterface(ENV, CB, nullptr);
}

void BAI::myInitBattleInterface(
    std::shared_ptr<Environment> ENV,
    std::shared_ptr<CBattleCallback> CB,
    MMAIExport::F_GetAction f_getAction
) {
    info("*** myInitBattleInterface ***");

    ASSERT(f_getAction, "f_getAction is null");

    env = ENV;
    cb = CB;
    getAction = f_getAction;

    wasWaitingForRealize = CB->waitTillRealize;
    wasUnlockingGs = CB->unlockGsWhenWaiting;
    CB->waitTillRealize = false;
    CB->unlockGsWhenWaiting = false;
}

MMAI_NS_END
