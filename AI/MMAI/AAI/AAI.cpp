// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include "GameConstants.h"
#include "battle/BattleHex.h"
#include "battle/CBattleInfoEssentials.h"
#include "export.h"
#include "gameState/CGameState.h"
#include "AAI.h"
#include "mapObjects/CArmedInstance.h"
#include "mapObjects/CGHeroInstance.h"

#include <cstdio>
#include <boost/thread.hpp>

namespace MMAI {
    AAI::AAI() {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++");
    }

    AAI::~AAI() {
        info("--- (destructor) ---");
    }

    void AAI::error(const std::string &text) const { logAi->error("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::warn(const std::string &text) const { logAi->warn("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::info(const std::string &text) const { logAi->info("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::debug(const std::string &text) const { logAi->debug("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
        error("*** initGameInterface -- BUT NO BAGGAGE ***");
        initGameInterface(env, CB, std::any{});
    }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage_) {
        info("*** initGameInterface ***");

        color = CB->getPlayerID()->toString();

        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Export::Baggage*), "baggage of unexpected type");

        baggage = std::any_cast<Export::Baggage*>(baggage_);

        if (color == "red") {
            // ansicolor = "\033[41m";  // red background
            battleAiName = baggage->battleAINameRed;
            getActionOrig = baggage->f_getActionRed;
            getValue = baggage->f_getValueRed;
            debug("(initGameInterface) using f_getActionRed");
        } else if (color == "blue") {
            // ansicolor = "\033[44m";  // blue background
            battleAiName = baggage->battleAINameBlue;
            getActionOrig = baggage->f_getActionBlue;
            getValue = baggage->f_getValueBlue;
            debug("(initGameInterface) using f_getActionBlue");
        } else {
            // Maps and everything basically assumes red human player attacking blue human player
            // Swapping armies and sides still uses only RED and BLUE as players
            // All other players should never be asked to lead a battle
            getActionOrig = [](const Export::Result* result) {
                throw std::runtime_error("Tried to call getAction on a non-RED non-BLUE player");
                return 0;
            };
            getValue = getActionOrig;
            battleAiName = "BUG_IF_REQUESTED";
            debug("(initGameInterface) n/a getAction");
        }


        debug("(init) battleAiName: " + battleAiName);

        // A wrapper around baggage->getAction
        // It ensures special handling for non-game actions (eg. render, reset)
        getActionWrapper = [this](const Export::Result* result) {
            debug("getActionWrapper called with result type: " + std::to_string(result->type));
            auto action = getNonRenderAction(result);
            debug("getActionWrapper received action: " + std::to_string(action));

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
        // info("getNonRenderAciton called with result type: " + std::to_string(result->type));
        auto action = getActionOrig(result);
        while (action == Export::ACTION_RENDER_ANSI) {
            auto res = Export::Result(bai->renderANSI(), Export::Side(bai->battle->battleGetMySide()));
            // info("getNonRenderAciton (loop) called with result type: " + std::to_string(res.type));
            action = getActionOrig(&res);
        }

        return action;
    }

    void AAI::yourTurn(QueryID queryID) {
        info("*** yourTurn *** (" + std::to_string(queryID.getNum()) + ")");

        std::make_unique<boost::thread>([this, queryID]() {
            boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

            info("Answering query " + std::to_string(queryID) + " to start turn");
            cb->selectionMade(0, queryID);

            auto heroes = cb->getHeroesInfo();
            assert(!heroes.empty());
            auto h = heroes.at(0);

            // Move 1 tile to the right
            cb->moveHero(h, h->pos + int3{1,0,0}, false);
        });
    }

    void AAI::battleStart(const BattleID &bid, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side_, bool replayAllowed) {
        info("*** battleStart ***");

        side = side_;

        const CGHeroInstance* hero;

        // Battles are ALWAYS between a RED hero and a BLUE hero
        // If --random-heroes is provided, side_, hero1 and hero2 will be different
        // Regardless hero1 and hero2's real owner, RED and BLUE AAIs will
        // receive them as battleStart arguments as if they were the owners.
        // (hero1->tempOwner is set to 0 (RED) or 1 (BLUE) for that purpose)

        // XXX: fix wrong color if --swap-sides option is used
        if (side == BattleSide::ATTACKER) {
            info("Will play ATTACKER in this battle");
            hero = dynamic_cast<const CGHeroInstance*>(army1);
            debug("(battleStart) side=ATTACKER => hero=(army1)");
        } else {
            info("Will play DEFENDER in this battle");
            hero = dynamic_cast<const CGHeroInstance*>(army2);
            debug("(battleStart) side=DEFENDER => hero=(army2)");
        }

        // XXX: VCMI's hero IDs do cannot be inferred by the map's JSON
        //      The gym maps use the hero's experience as a unique ref

        debug("(battleStart) hero1->tempOwner: " + std::to_string(hero1->tempOwner));
        debug("(battleStart) hero2->tempOwner: " + std::to_string(hero2->tempOwner));
        debug("(battleStart) hero(army)->tempOwner: " + std::to_string(hero->tempOwner));
        debug("(battleStart) hero1->getOwner(): " + std::to_string(hero1->getOwner()));
        debug("(battleStart) hero2->getOwner(): " + std::to_string(hero2->getOwner()));
        debug("(battleStart) hero(army)->getOwner(): " + std::to_string(hero->getOwner()));

        // armyID = int(hero->exp);
        // debug("(battleStart) armyID: " + std::to_string(armyID));
        // debug("Our hero: " + std::to_string(armyID));

        // just copied code from CAdventureAI::battleStart
        // only difference is argument to initBattleInterface()
        assert(!battleAI);
        assert(cbc);

        auto ainame = getBattleAIName();
        battleAI = CDynLibHandler::getNewBattleAI(ainame);

        ASSERT(!bai, "expected a nullptr bai");

        if (ainame == "MMAI") {
            bai = std::static_pointer_cast<BAI>(battleAI);
            bai->color = color;
            bai->ansicolor = ansicolor;
            bai->myInitBattleInterface(env, cbc, getActionWrapper, getValue);
        } else {
            battleAI->initBattleInterface(env, cbc);
        }

        battleAI->battleStart(bid, army1, army2, tile, hero1, hero2, side_, replayAllowed);
    }

    void AAI::battleEnd(const BattleID &bid, const BattleResult * br, QueryID queryID) {
        info("*** battleEnd (QueryID: " + std::to_string(static_cast<int>(queryID)) + ") ***");

        if (!bai) {
            ASSERT(getBattleAIName() != "MMAI", "no bai, but battleAIName is MMAI");
            // We are not MMAI, nothing to do
        } else if (!bai->action) {
            // Null action means the battle ended without us receiving a turn
            // Happens if the enemy immediately retreats (we won)
            // or if the enemy one-shots us (we lost)
            // Nothing to do
        } else if (bai->action->action != Export::ACTION_RETREAT) {
            // TODO: must add check that *we* have retreated and not the enemy
            // XXX: ^^^ still relevant?

            // battleEnd after MOVE
            // => call f_getAction (expecting RESET)

            // first call bai->battleEnd to update result
            // (in case there are render actions)
            bai->battleEnd(bid, br, queryID);

            debug("<BATTLE_END> Will request a non-render action");
            auto action = getNonRenderAction(bai->result.get());
            info("<BATTLE_END> Got action: " + std::to_string(action));

            // Any non-render action *must* be a reset (battle has ended)
            ASSERT(action == Export::ACTION_RESET, "expected RESET, got: " + std::to_string(action));
        }

        if (cb->getBattle(bid)->battleGetMySide() == BattlePerspective::LEFT_SIDE) {
            ASSERT(queryID != -1, "QueryID is -1, but we are ATTACKER");
            info("Answering query " + std::to_string(queryID) + " to re-play battle");

            std::make_unique<boost::thread>([this, queryID]() {
                boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);
                cb->selectionMade(1, queryID);
            });
        } else {
            // My patch in CGameHandler::endBattle allows replay even when
            // both sides are non-neutrals. Could not figure out how to
            // send the query only to the attacker.
            // ASSERT(queryID == -1, "QueryID is not -1, but we are DEFENDER");

            // The defender should not answer replay battle queries
            info("Ignoring query " + std::to_string(queryID));
        }

        battleAI.reset();
        bai = nullptr;  // XXX: call this last, it changes cb->waitTillRealize
    }

    std::string AAI::getBattleAIName() const {
        debug("*** getBattleAIName ***");

        // Attackers are red, defenders are blue
        auto ainame = (color == "red")
            ? baggage->battleAINameRed
            : baggage->battleAINameBlue;

        debug("(getBattleAIName) battleAiName: " + ainame);
        return ainame;
    }
}
