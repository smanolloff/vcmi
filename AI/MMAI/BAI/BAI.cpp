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

#include <stdexcept>
#include <filesystem>
#include "BAI.h"
#include "BAI/delegatee.h"
#include "common.h"

namespace MMAI::BAI {
    void BAI::error(const std::string &text) const { logAi->error("BAI-%s [%s] %s", addrstr, colorname, text); }
    void BAI::warn(const std::string &text) const { logAi->warn("BAI-%s [%s] %s", addrstr, colorname, text); }
    void BAI::info(const std::string &text) const { logAi->info("BAI-%s [%s] %s", addrstr, colorname, text); }
    void BAI::debug(const std::string &text) const { logAi->debug("BAI-%s [%s] %s", addrstr, colorname, text); }

    BAI::BAI() {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++"); // log after addrstr is set
    }

    BAI::~BAI() {
        info("--- destructor ---");
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
        std::any baggage_,
        std::string color_
    ) {
        info("*** initBattleInterface -- WITH baggage ***");
        colorname = color_;

        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Schema::Baggage*), "baggage of unexpected type");
        auto baggage = std::any_cast<Schema::Baggage*>(baggage_);
        delegatee = Delegatee::Create(colorname, baggage, ENV, CB);
    }

    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB
    ) {
        info("*** initBattleInterface -- BUT NO BAGGAGE ***");
        throw std::runtime_error("Baggage is required for all calls to BAI::initBattleInterface");
    }

    void BAI::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        AutocombatPreferences autocombatPreferences
    ) {
        initBattleInterface(ENV, CB);
    }

    /*
     * Delegated methods
     */
    void BAI::activeStack(const BattleID &bid, const CStack * astack) {
        delegatee->activeStack(bid, astack);
    };

    void BAI::yourTacticPhase(const BattleID &bid, int distance) {
        delegatee->yourTacticPhase(bid, distance);
    }

    void BAI::battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) {
        delegatee->battleStacksAttacked(bid, bsa, ranged);
    }

    void BAI::actionFinished(const BattleID &bid, const BattleAction &action) {
        debug("*** actionFinished ***");
        delegatee->actionFinished(bid, action);
    }

    void BAI::actionStarted(const BattleID &bid, const BattleAction &action) {
        debug("*** actionStarted ***");
        delegatee->actionStarted(bid, action);
    }

    void BAI::battleAttack(const BattleID &bid, const BattleAttack *ba) {
        debug("*** battleAttack ***");
        delegatee->battleAttack(bid, ba);
    }

    void BAI::battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) {
        debug("*** battleEnd ***");
        delegatee->battleEnd(bid, br, queryID);
    }

    void BAI::battleNewRoundFirst(const BattleID &bid) {
        debug("*** battleNewRoundFirst ***");
        delegatee->battleNewRoundFirst(bid);
    }

    void BAI::battleNewRound(const BattleID &bid) {
        debug("*** battleNewRound ***");
        delegatee->battleNewRound(bid);
    }

    void BAI::battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) {
        debug("*** battleStackMoved ***");
        delegatee->battleStackMoved(bid, stack, dest, distance, teleport);
    }

    void BAI::battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) {
        debug("*** battleSpellCast ***");
        delegatee->battleSpellCast(bid, sc);
    }

    void BAI::battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) {
        debug("*** battleStacksEffectsSet ***");
        delegatee->battleStacksEffectsSet(bid, sse);
    }

    void BAI::battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) {
        debug("*** battleTriggerEffect ***");
        delegatee->battleTriggerEffect(bid, bte);
    }

    void BAI::battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) {
        debug("*** battleStart ***");
        delegatee->battleStart(bid, army1, army2, tile, hero1, hero2, side, replayAllowed);
    }

    void BAI::battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & units) {
        debug("*** battleUnitsChanged ***");
        delegatee->battleUnitsChanged(bid, units);
    }

    void BAI::battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) {
        debug("*** battleCatapultAttacked ***");
        delegatee->battleCatapultAttacked(bid, ca);
    }
}
