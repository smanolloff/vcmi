#include "GameConstants.h"
#include "battle/BattleHex.h"
#include "battle/CBattleInfoEssentials.h"
#include "export.h"
#include "gameState/CGameState.h"
#include "AAI.h"

#include <boost/thread.hpp>

namespace MMAI {
    AAI::AAI() {
        info("*** (constructor) ***");
    }

    AAI::~AAI() {
        info("--- (destructor) ---");
    }

    void AAI::error(const std::string &text) const { logAi->error("AAI [%s] %s", colorprint, text); }
    void AAI::warn(const std::string &text) const { logAi->warn("AAI [%s] %s", colorprint, text); }
    void AAI::info(const std::string &text) const { logAi->info("AAI [%s] %s", colorprint, text); }
    void AAI::debug(const std::string &text) const { logAi->debug("AAI [%s] %s", colorprint, text); }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
        error("*** initGameInterface -- BUT NO BAGGAGE ***");
        initGameInterface(env, CB, std::any{});
    }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage_) {
        info("*** initGameInterface ***");

        colorstr = CB->getMyColor()->getStr();
        colorprint = (colorstr == "red")
            ? "\033[0m\033[97m\033[41mRED\033[0m"  // white on red
            : "\033[0m\033[97m\033[44mBLUE\033[0m"; // white on blue
        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Export::Baggage*), "baggage of unexpected type");

        baggage = std::any_cast<Export::Baggage*>(baggage_);

        // Attackers are red, defenders are blue
        if (colorstr == "red") {
            battleAiName = baggage->AttackerBattleAIName;
            getActionOrig = baggage->f_getActionAttacker;
        } else {
            battleAiName = baggage->DefenderBattleAIName;
            getActionOrig = baggage->f_getActionDefender;
        }

        // A wrapper around baggage->idGetAction
        // It ensures special handling for non-game actions (eg. render, reset)
        getActionWrapper = [this](const Export::Result* result) {
            info("getActionWrapper called with result type: " + std::to_string(result->type));
            auto action = getNonRenderAction(result);
            info("getActionWrapper received action: " + std::to_string(action));

            if (action == Export::ACTION_RESET) {
                // AAI::getAction is called only by BAI, only during battle
                // FIXME: retreat may be impossible, a _real_ reset should be implemented
                info("Will retreat in order to reset battle");
                ASSERT(!bai->result->ended, "expected active battle");
                action = Export::ACTION_RETREAT;
            }

            return action;
        };

        cb = CB;
        cbc = CB;
        cb->waitTillRealize = true;
        cb->unlockGsWhenWaiting = true;
    };

    Export::Action AAI::getNonRenderAction(const Export::Result* result) {
        info("getNonRenderAciton called with result type: " + std::to_string(result->type));
        auto action = getActionOrig(result);
        while (action == Export::ACTION_RENDER_ANSI) {
            auto res = Export::Result(bai->renderANSI());
            info("getNonRenderAciton (loop) called with result type: " + std::to_string(res.type));
            action = getActionOrig(&res);
        }

        return action;
    }

    void AAI::yourTurn() {
        info("*** yourTurn ***");

        std::make_unique<boost::thread>([this]() {
            boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

            auto heroes = cb->getHeroesInfo();
            assert(!heroes.empty());
            auto h = heroes[0];

            cb->moveHero(h, int3{2,1,0}, false);
        });
    }

    void AAI::battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) {
        info("*** battleStart ***");

        if (colorstr == "red") {
            ASSERT(side == BattleSide::ATTACKER, "Red is not attacker");
        } else {
            ASSERT(side == BattleSide::DEFENDER, "Non-red is not defender");
        }

        // just copied code from CAdventureAI::battleStart
        // only difference is argument to initBattleInterface()
        assert(!battleAI);
        assert(cbc);

        auto ainame = getBattleAIName();
        battleAI = CDynLibHandler::getNewBattleAI(ainame);

        ASSERT(!bai, "expected a nullptr bai");

        if (ainame == "MMAI") {
            bai = std::static_pointer_cast<BAI>(battleAI);
            bai->myInitBattleInterface(env, cbc, getActionWrapper);
        } else {
            battleAI->initBattleInterface(env, cbc);
        }

        battleAI->battleStart(army1, army2, tile, hero1, hero2, side, replayAllowed);
    }

    void AAI::battleEnd(const BattleResult * br, QueryID queryID) {
        info("*** battleEnd (QueryID: " + std::to_string(static_cast<int>(queryID)) + ") ***");

        // Null action means the battle ended without us receiving a turn
        // (can only happen if we are DEFENDER)

        if (!bai) {
            ASSERT(getBattleAIName() != "MMAI", "no bai, but battleAIName is MMAI");
            // We are not MMAI, nothing to do
        } else if (!bai->action) {
            ASSERT(bai->side == BattleSide::DEFENDER, "no action at battle end, but we are ATTACKER");
        } else if (bai->action->action != Export::ACTION_RETREAT) {
            // TODO: must add check that *we* have retreated and not the enemy
            // battleEnd after MOVE
            // => call f_getAction (expecting RESET)

            // first call bai->battleEnd to update result
            // (in case there are render actions)
            bai->battleEnd(br, queryID);

            info("<BATTLE_END> Will ASK for an action");
            auto action = getNonRenderAction(bai->result.get());
            info("<BATTLE_END> Got action: " + std::to_string(action));

            // Any non-render action *must* be a reset (battle has ended)
            ASSERT(action == Export::ACTION_RESET, "expected RESET, got: " + std::to_string(action));
        }

        if (cb->battleGetMySide() == BattlePerspective::LEFT_SIDE) {
            ASSERT(queryID != -1, "QueryID is -1, but we are ATTACKER");
            info("Answering query " + std::to_string(queryID) + " to re-play battle");

            std::make_unique<boost::thread>([this, queryID]() {
                boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);
                cb->selectionMade(1, queryID);
            });
        } else {
            // My patch in CGameHandler::endBattle allows replay even
            // both sides are non-neutrals. Could not figure out how to
            // send the query only to the attacker.
            // ASSERT(queryID == -1, "QueryID is not -1, but we are DEFENDER");

            // The defender should not answer "replay" queries
            info("Ignoring query " + std::to_string(queryID));
        }

        battleAI.reset();
        bai = nullptr;  // XXX: call this last, it changes cb->waitTillRealize
    }

    std::string AAI::getBattleAIName() const {
        debug("*** getBattleAIName ***");

        // Attackers are red, defenders are blue
        return (colorstr == "red")
            ? baggage->AttackerBattleAIName
            : baggage->DefenderBattleAIName;
    }
}
