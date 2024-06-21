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

#include <memory>
#include <stdexcept>
#include <string>

#include "base.h"
#include "networkPacks/PacksForClientBattle.h"
#include "networkPacks/SetStackEffect.h"
#include "spells/CSpellHandler.h"
#include "v1/BAI.h"
#include "v2/BAI.h"
#include "v3/BAI.h"

namespace MMAI::BAI {
    // static
    std::unique_ptr<Base> Base::Create(
        const std::string colorname,
        const Schema::Baggage* baggage,
        const std::shared_ptr<Environment> env,
        const std::shared_ptr<CBattleCallback> cb
    ) {
        std::unique_ptr<Base> res;

        auto version = colorname == "red" ? baggage->versionRed : baggage->versionBlue;

        switch (version) {
        break; case 1:
            res = std::make_unique<V1::BAI>(version, colorname, baggage, env, cb);
        break; case 2:
            res = std::make_unique<V2::BAI>(version, colorname, baggage, env, cb);
        break; case 3:
            res = std::make_unique<V3::BAI>(version, colorname, baggage, env, cb);
        break; default:
            throw std::runtime_error("Unsupported schema version: " + std::to_string(version));
        }

        res->init();
        return res;
    }

    Base::Base(
        const int version_,
        const std::string colorname_,
        const Schema::Baggage* baggage_,
        const std::shared_ptr<Environment> env_,
        const std::shared_ptr<CBattleCallback> cb_
    ) : version(version_)
      , name("BAI-v" + std::to_string(version_))
      , colorname(colorname_)
      , baggage(baggage_)
      , env(env_)
      , cb(cb_)
    {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++");

        // Not sure if this is needed
        cb->waitTillRealize = false;
        cb->unlockGsWhenWaiting = false;

        if (colorname == "red") {
            // ansicolor = "\033[41m";  // red background
            f_getAction = baggage->f_getActionRed;
            f_getValue = baggage->f_getValueRed;
            debug("using f_getActionRed");
        } else if (colorname == "blue") {
            // ansicolor = "\033[44m";  // blue background
            f_getAction = baggage->f_getActionBlue;
            f_getValue = baggage->f_getValueBlue;
            debug("using f_getActionBlue");
        } else {
            error("expected red or blue, got: " + colorname);
            throw std::runtime_error("unexpected color");
        }
    }

    /*
     * These methods MUST be overridden by derived BAI (e.g. BAI::V1)
     * Their base implementation is is for logging purposes only.
     */

    void Base::activeStack(const BattleID &bid, const CStack *astack) {
        debug("*** activeStack ***");
        trace("activeStack called for " + astack->nodeName());
    };

    void Base::yourTacticPhase(const BattleID &bid, int distance) {
        debug("*** yourTacticPhase ***");
    };

    /*
     * These methods MAY be overriden by derived BAI (e.g. BAI::V1)
     * Their implementation here is a no-op.
     */

    void Base::init() {
        debug("*** init ***");
    }

    void Base::actionFinished(const BattleID &bid, const BattleAction &action) {
        debug("*** actionFinished ***");
    }

    void Base::actionStarted(const BattleID &bid, const BattleAction &action) {
        debug("*** actionStarted ***");
    }

    void Base::battleAttack(const BattleID &bid, const BattleAttack *ba) {
        debug("*** battleAttack ***");
    }

    void Base::battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) {
        debug("*** battleCatapultAttacked ***");
    }

    void Base::battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) {
        debug("*** battleEnd ***");
    }

    void Base::battleGateStateChanged(const BattleID &bid, const EGateState state) {
        debug("*** battleGateStateChanged ***");
        trace("New gate state: %d", EI(state));
    }

    void Base::battleLogMessage(const BattleID &bid, const std::vector<MetaString> &lines) {
        debug("*** battleLogMessage ***");
        trace([&](){
            std::string res = "Messages:";
            for(const auto & line : lines) {
                std::string formatted = line.toString();
                boost::algorithm::trim(formatted);
                res = res + "\n\t* " + formatted;
            }
            return res;
        });
    }

    void Base::battleNewRound(const BattleID &bid) {
        debug("*** battleNewRound ***");
    }

    void Base::battleNewRoundFirst(const BattleID &bid) {
        debug("*** battleNewRoundFirst ***");
    }

    void Base::battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> &obstacles) {
        debug("*** battleObstaclesChanged ***");
    }

    void Base::battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) {
        debug("*** battleSpellCast ***");
        trace([&](){
            std::string res = "Spellcast info:";
            auto battle = cb->getBattle(bid);
            auto caster = battle->battleGetStackByID(sc->casterStack);

            res += "\n\t* spell: " + sc->spellID.toSpell()->identifier;
            res += "\n\t* castByHero=" + std::to_string(sc->castByHero);
            res += "\n\t* casterStack=" + (caster ? caster->getDescription() : "");
            res += "\n\t* activeCast=" + std::to_string(sc->activeCast);
            res += "\n\t* side=" + std::to_string(sc->side);
            res += "\n\t* tile=" + std::to_string(sc->tile);

            res += "\n\t* affected:";
            for (auto &cid : sc->affectedCres)
                res += "\n\t  > " + battle->battleGetStackByID(cid)->getDescription();

            res += "\n\t* resisted:";
            for (auto &cid : sc->resistedCres)
                res += "\n\t  > " + battle->battleGetStackByID(cid)->getDescription();

            res += "\n\t* reflected:";
            for (auto &cid : sc->reflectedCres)
                res += "\n\t  > " + battle->battleGetStackByID(cid)->getDescription();

            return res;
        });
    }

    void Base::battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) {
        debug("*** battleStackMoved ***");
        trace([&](){
            auto battle = cb->getBattle(bid);
            std::string fmt = "Movement info:";

            fmt += "\n\t* stack description=%s";
            fmt += "\n\t* stack owner=%s";
            fmt += "\n\t* dest[0]=%d (Hex#%d, y=%d, x=%d)";
            fmt += "\n\t* distance=%d";
            fmt += "\n\t* teleport=%d";

            auto bh0 = dest.at(0);
            auto hexid0 = V1::Hex::CalcId(dest.at(0));
            auto [x0, y0] = V1::Hex::CalcXY(dest.at(0));

            auto res = boost::format(fmt)
                % stack->getDescription()
                % stack->getOwner().toString()
                % bh0.hex % hexid0 % y0 % x0
                % distance
                % teleport;

            return boost::str(res);
        });
    }

    void Base::battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) {
        debug("*** battleStacksAttacked ***");
    }

    void Base::battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) {
        debug("*** battleStacksEffectsSet ***");
        trace([&](){
            auto battle = cb->getBattle(bid);

            std::string res = "Effects:";

            for (auto &[unitid, bonuses] : sse.toAdd) {
                auto cstack = battle->battleGetStackByID(unitid);
                res += "\n\t* stack=" + (cstack ? cstack->getDescription() : "");
                for (auto &bonus : bonuses) {
                    res += "\n\t  > add bonus=" + bonus.Description();
                }
            }

            for (auto &[unitid, bonuses] : sse.toRemove) {
                auto cstack = battle->battleGetStackByID(unitid);
                res += "\n\t* stack=" + (cstack ? cstack->getDescription() : "");
                for (auto &bonus : bonuses) {
                    res += "\n\t  > remove bonus=" + bonus.Description();
                }
            }

            for (auto &[unitid, bonuses] : sse.toUpdate) {
                auto cstack = battle->battleGetStackByID(unitid);
                res += "\n\t* stack=" + (cstack ? cstack->getDescription() : "");
                for (auto &bonus : bonuses) {
                    res += "\n\t  > update bonus=" + bonus.Description();
                }
            }

            return res;
        });
    }

    void Base::battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) {
        debug("*** battleStart ***");
    }

    // XXX: positive morale triggers an effect
    //      negative morale just skips turn
    void Base::battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) {
        debug("*** battleTriggerEffect ***");
        trace([&](){
            auto battle = cb->getBattle(bid);
            auto cstack = battle->battleGetStackByID(bte.stackID);
            std::string res = "Effect:";
            res += "\n\t* bonus id=" + std::to_string(bte.effect);
            res += "\n\t* bonus value=" + std::to_string(bte.val);
            res += "\n\t* stack=" + (cstack ? cstack->getDescription() : "");
            return res;
        });
    }

    void Base::battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> &changes) {
        debug("*** battleUnitsChanged ***");
        trace([&](){
            std::string res = "Changes:";
            for(const auto & change : changes) {
                res += "\n\t* operation=" + std::to_string(EI(change.operation));
                res += "\n\t* healthDelta=" + std::to_string(change.healthDelta);
            }
            return res;
        });
    }

    /*
     * private
     */

    template<typename ... Args> void Base::_log(const ELogLevel::ELogLevel level, const std::string &format, Args ... args) const {
        logAi->log(level, "%s-%s [%s] " + format, name, addrstr, colorname, args...);
    }

    template<typename ... Args> void Base::error(const std::string &format, Args ... args) const { log(ELogLevel::ERROR, format, args...); }
    template<typename ... Args> void Base::warn(const std::string &format, Args ... args) const { log(ELogLevel::WARN, format, args...); }
    template<typename ... Args> void Base::info(const std::string &format, Args ... args) const { log(ELogLevel::INFO, format, args...); }
    template<typename ... Args> void Base::debug(const std::string &format, Args ... args) const { log(ELogLevel::DEBUG, format, args...); }
    template<typename ... Args> void Base::trace(const std::string &format, Args ... args) const { log(ELogLevel::DEBUG, format, args...); }
    template<typename ... Args> void Base::log(ELogLevel::ELogLevel level, const std::string &format, Args ... args) const {
        if (logAi->getLevel() <= level) _log(level, format, args...);
    }


    void Base::error(const std::string &text) const { log(ELogLevel::ERROR, text); }
    void Base::warn(const std::string &text) const { log(ELogLevel::WARN, text); }
    void Base::info(const std::string &text) const { log(ELogLevel::INFO, text); }
    void Base::debug(const std::string &text) const { log(ELogLevel::DEBUG, text); }
    void Base::trace(const std::string &text) const { log(ELogLevel::TRACE, text); }
    void Base::log(ELogLevel::ELogLevel level, const std::string &text) const {
        if (logAi->getLevel() <= level) _log(level, "%s", text);
    }

    void Base::error(const std::function<std::string()> &cb) const { log(ELogLevel::ERROR, cb); }
    void Base::warn(const std::function<std::string()> &cb) const { log(ELogLevel::WARN, cb); }
    void Base::info(const std::function<std::string()> &cb) const { log(ELogLevel::INFO, cb); }
    void Base::debug(const std::function<std::string()> &cb) const { log(ELogLevel::DEBUG, cb); }
    void Base::trace(const std::function<std::string()> &cb) const { log(ELogLevel::TRACE, cb); }
    void Base::log(ELogLevel::ELogLevel level, const std::function<std::string()> &cb) const {
        if (logAi->getLevel() <= level) _log(level, "%s", cb());
    }
}
