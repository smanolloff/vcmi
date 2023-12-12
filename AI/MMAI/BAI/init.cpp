#include <dlfcn.h>

#include "BAI.h"
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
    //   is called instead with a wrapped f_idGetAction fn that never returns
    //   control actions (like RENDER or RESET).
    //
    // In this case, however, baggage is passed in directly - we must make
    // sure to wrap its f_idGetAction ourselves.
    //
    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        std::any baggage_
    ) {
        info("*** initBattleInterface -- WITH baggage ***");

        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Export::Baggage*), "baggage of unexpected type");

        // XXX: this may need to be stored?
        auto baggage = std::any_cast<Export::Baggage*>(baggage_);

        Export::F_IDGetAction f_idGetAction = [this, baggage](Export::Side side, const Export::Result* result) {
            auto action = baggage->f_idGetAction(side, result);
            while (action == Export::ACTION_RENDER_ANSI) {
                auto res = Export::Result(renderANSI());
                action = baggage->f_idGetAction(side, result);
            }

            if (action == Export::ACTION_RESET) {
                info("Will retreat in order to reset battle");
                ASSERT(!result->ended, "expected active battle");
                action = Export::ACTION_RETREAT;
            }

            return action;
        };

        myInitBattleInterface(ENV, CB, f_idGetAction);
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
        Export::F_IDGetAction f_idGetAction
    ) {
        info("*** myInitBattleInterface ***");
        ASSERT(f_idGetAction, "f_idGetAction is null");
        idGetAction = f_idGetAction;
        env = ENV;
        cb = CB;
        wasWaitingForRealize = CB->waitTillRealize;
        wasUnlockingGs = CB->unlockGsWhenWaiting;
        CB->waitTillRealize = false;
        CB->unlockGsWhenWaiting = false;
    }
}
