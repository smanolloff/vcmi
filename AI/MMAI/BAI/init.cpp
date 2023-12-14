#include <dlfcn.h>

#include "BAI.h"
#include "battle/BattleHex.h"
#include "battle/CBattleInfoEssentials.h"
#include "export.h"
#include "include/loader.h"

// Contains initialization-related code

namespace MMAI {
    BAI::BAI() : side(BattleSide::ATTACKER), wasWaitingForRealize(false), wasUnlockingGs(false) {
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

    // XXX:
    // This call is made when BAI is created for a Human (by CPlayerInterface)
    // If BAI is created for a computer (by MMAI), "myInitBattleInterface"
    //   is called instead with a wrapped f_getAction fn that never returns
    //   control actions (like RENDER or RESET).
    //
    // In this case, however, baggage is passed in directly - we must make
    // sure to wrap its f_getAction ourselves.
    //
    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        std::any baggage_
    ) {
        info("*** initBattleInterface -- WITH baggage ***");

        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Export::Baggage*), "baggage of unexpected type");

        auto baggage = std::any_cast<Export::Baggage*>(baggage_);

        getActionOrig = (CB->battleGetMySide() == BattlePerspective::LEFT_SIDE)
            ? baggage->f_getActionAttacker
            : baggage->f_getActionDefender;

        Export::F_GetAction f_getAction = [this](const Export::Result* result) {
            info("BAI6");
            auto act = getActionOrig(result);
            info("BAI7");
            while (act == Export::ACTION_RENDER_ANSI) {
                auto res = Export::Result(renderANSI());
                info("BAI8");
                act = getActionOrig(&res);
            }

            if (act == Export::ACTION_RESET) {
                info("Will retreat in order to reset battle");
                ASSERT(!result->ended, "expected active battle");
                act = Export::ACTION_RETREAT;
            }

            return act;
        };

        myInitBattleInterface(ENV, CB, f_getAction);
    }

    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB
    ) {
        info("*** initBattleInterface -- BUT NO BAGGAGE ***");
        myInitBattleInterface(ENV, CB, nullptr);
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
        sidestr = (CB->battleGetMySide() == BattlePerspective::LEFT_SIDE) ? "A" : "D";
        ASSERT(f_getAction, "f_getAction is null");
        getAction = f_getAction;
        env = ENV;
        cb = CB;
        wasWaitingForRealize = CB->waitTillRealize;
        wasUnlockingGs = CB->unlockGsWhenWaiting;
        CB->waitTillRealize = false;
        CB->unlockGsWhenWaiting = false;
    }
}
