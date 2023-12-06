#include <dlfcn.h>

#include "BAI.h"
#include "export.h"
#include "include/loader.h"

// Contains initialization-related code

namespace MMAI {
    BAI::BAI() : side(-1), wasWaitingForRealize(false), wasUnlockingGs(false) {
        info("+++ constructor +++");
    }

    BAI::~BAI() {
        info("--- destructor ---");

        if(cb) {
            // copied from BattleAI, not sure if needed
            cb->waitTillRealize = wasWaitingForRealize;
            cb->unlockGsWhenWaiting = wasUnlockingGs;
        }

        // if (shutdown)
        //     shutdown();
    }

    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB
    ) {
        info("*** initBattleInterface -- BUT NO F_GetAction - LOADING libconnload.dylib ***");

        auto libfile = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/connector/build/libloader.dylib";
        void* handle = dlopen(libfile, RTLD_LAZY);
        ASSERT(handle, "Error loading the library: " + std::string(dlerror()));

        // preemptive init done in myclient to avoid freezing at first click of "auto-combat"
        // init = reinterpret_cast<decltype(&ConnectorLoader_init)>(dlsym(handle, "ConnectorLoader_init"));
        // ASSERT(init, "Error getting init fn: " + std::string(dlerror()));

        getAction = reinterpret_cast<decltype(&ConnectorLoader_getAction)>(dlsym(handle, "ConnectorLoader_getAction"));
        ASSERT(getAction, "Error getting getAction fn: " + std::string(dlerror()));

        myInitBattleInterface(ENV, CB, getAction);
    }

    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        AutocombatPreferences autocombatPreferences
    ) {
        initBattleInterface(ENV, CB);
    }

    void BAI::myInitBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        Export::F_GetAction f_getAction
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
}
