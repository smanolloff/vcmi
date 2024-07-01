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
#include "BAI/router.h"
#include "BAI/base.h"
#include "common.h"
#include "scripted/summoner.h"

namespace MMAI::BAI {
    Router::Router() {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++"); // log after addrstr is set
    }

    Router::~Router() {
        info("--- destructor ---");
    }

    void Router::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage_, std::string color_) {
        info("*** initBattleInterface -- WITH baggage ***");
        colorname = color_;
        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Schema::Baggage*), "baggage of unexpected type");
        auto baggage = std::any_cast<Schema::Baggage*>(baggage_);

        auto version = colorname == "red" ? baggage->versionRed : baggage->versionBlue;

        switch (version) {
        break; case MMAI_RESERVED_VERSION_SUMMONER:
            bai = std::make_unique<Scripted::Summoner>();
            bai->initBattleInterface(ENV, CB);
        break; default:
            bai = Base::Create(colorname, baggage, ENV, CB);
        }
    }

    void Router::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) {
        info("*** initBattleInterface -- BUT NO BAGGAGE ***");
        throw std::runtime_error("Baggage is required for all calls to Router::initBattleInterface");
    }

    void Router::initBattleInterface(
        std::shared_ptr<Environment> ENV,
        std::shared_ptr<CBattleCallback> CB,
        AutocombatPreferences autocombatPreferences
    ) {
        initBattleInterface(ENV, CB);
    }

    /*
     * Delegated methods
     */

    void Router::actionFinished(const BattleID &bid, const BattleAction &action) {
        bai->actionFinished(bid, action);
    }

    void Router::actionStarted(const BattleID &bid, const BattleAction &action) {
        bai->actionStarted(bid, action);
    }

    void Router::activeStack(const BattleID &bid, const CStack * astack) {
        bai->activeStack(bid, astack);
    }

    void Router::battleAttack(const BattleID &bid, const BattleAttack *ba) {
        bai->battleAttack(bid, ba);
    }

    void Router::battleCatapultAttacked(const BattleID &bid, const CatapultAttack &ca) {
        bai->battleCatapultAttacked(bid, ca);
    }

    void Router::battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) {
        bai->battleEnd(bid, br, queryID);
    }

    void Router::battleGateStateChanged(const BattleID &bid, const EGateState state) {
        bai->battleGateStateChanged(bid, state);
    };

    void Router::battleLogMessage(const BattleID &bid, const std::vector<MetaString> &lines) {
        bai->battleLogMessage(bid, lines);
    };

    void Router::battleNewRound(const BattleID &bid) {
        bai->battleNewRound(bid);
    }

    void Router::battleNewRoundFirst(const BattleID &bid) {
        bai->battleNewRoundFirst(bid);
    }

    void Router::battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> &obstacles) {
        bai->battleObstaclesChanged(bid, obstacles);
    };

    void Router::battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) {
        bai->battleSpellCast(bid, sc);
    }

    void Router::battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) {
        bai->battleStackMoved(bid, stack, dest, distance, teleport);
    }

    void Router::battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) {
        bai->battleStacksAttacked(bid, bsa, ranged);
    }

    void Router::battleStacksEffectsSet(const BattleID &bid, const SetStackEffect &sse) {
        bai->battleStacksEffectsSet(bid, sse);
    }

    void Router::battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) {
        bai->battleStart(bid, army1, army2, tile, hero1, hero2, side, replayAllowed);
    }

    void Router::battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect &bte) {
        bai->battleTriggerEffect(bid, bte);
    }

    void Router::battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> &units) {
        bai->battleUnitsChanged(bid, units);
    }

    void Router::yourTacticPhase(const BattleID &bid, int distance) {
        bai->yourTacticPhase(bid, distance);
    }

    /*
     * private
     */

    void Router::error(const std::string &text) const { log(ELogLevel::ERROR, text); }
    void Router::warn(const std::string &text) const { log(ELogLevel::WARN, text); }
    void Router::info(const std::string &text) const { log(ELogLevel::INFO, text); }
    void Router::debug(const std::string &text) const { log(ELogLevel::DEBUG, text); }
    void Router::trace(const std::string &text) const { log(ELogLevel::TRACE, text); }
    void Router::log(ELogLevel::ELogLevel level, const std::string &text) const {
        if (logAi->getLevel() <= level) logAi->debug("Router-%s [%s] %s", addrstr, colorname, text);
    }
}
