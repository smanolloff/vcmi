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

#include "StdInc.h"
#include "CStack.h"
#include "battle/BattleAction.h"
#include "battle/BattleHex.h"
#include "battle/BattleHexArray.h"
#include "battle/CObstacleInstance.h"
#include "battle/ReachabilityInfo.h"
#include "callback/CBattleCallback.h"
#include "lib/CRandomGenerator.h"
#include "lib/callback/AIFactory.h"

#include "MLBot.h"
#include "schema/v15/constants.h"
#include <algorithm>
#include <boost/range/numeric.hpp>
#include <span>
#include <stdexcept>

namespace MMAI::BAI
{

namespace {
    /*
     * Return x's neighbouring hexes for moving to
     *
     * NOTE: x is NOT the VIP! vip->getAttackableHexes() is used in that case.
     *       Instead, x is only an unreachable MOVE target hex for the guard, where
     *       we want to see which neighbouring hex is the best towards reaching x.
     *
     *    1-hex guard:      2-hex guard (L):   2-hex guard (R):
     *  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
     * . . . . o o . . . . . . . o o o . . . . . . o o o . . .
     *  . . . o x o . . . . . . o - x o . . . . . o x - o . . .
     * . . . . o o . . . . . . . o o o . . . . . . o o o . . .
     *  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
     */
    BattleHexArray NearbyMoveHexes(const CStack * guard, const BattleHex & bh)
    {
        if(!guard->doubleWide())
            return bh.getAllNeighbouringTiles();

        // "side" in the context of getNeighbouringTilesDoubleWide is
        // the side of a unit standing on "x". The function then returns
        // the attack-hexes an OPPONENT unit could move to.
        // => we invert guard's side when passing it.
        return guard->unitSide() == BattleSide::ATTACKER
            ? bh.getNeighbouringTilesDoubleWide(BattleSide::DEFENDER)
            : bh.getNeighbouringTilesDoubleWide(BattleSide::ATTACKER);
    }

    /*
     * From a list of candidate hexes, pick one which:
     *  1. is closest to the guard
     *  2. (if guard is attacker) has the lowest "X" coordinate ("keep left")
     *     (if guard is defender) has the highest "X" coordinate ("keep right")
     */
    const BattleHex* PickClosestHex(
        const CStack * guard,
        std::span<const uint32_t> distances,
        std::span<const BattleHex> candidates,
        std::span<bool> skips
    ) {
        if (candidates.empty()) return nullptr;

        const BattleHex* best = nullptr;

        // if (best->toInt() < 0 || best->toInt() >= distances.size()) {
        //     logAi->error("Invalid candidate: %d (distances.size=%d)", best->toInt(), distances.size());
        //     throw std::runtime_error("Invalid candidate");
        // }

        bool keepLeft = guard->unitSide() == BattleSide::ATTACKER;
        uint32_t bestVal = 999;
        int bestX = keepLeft ? 999 : -999;

        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const BattleHex& h = candidates[i];

            if (h.toInt() < 0 || h.toInt() >= distances.size()) {
                // Can happen for nearby hexes to a stack occupying row 0 or row 14
                logAi->debug("Skip invalid candidate: %d", h.toInt());
                continue;
            }

            if (skips[h.toInt()]) {
                logAi->debug("Skip repeated candidate: %d", h.toInt());
                continue;
            }

            skips[h.toInt()] = true;

            const uint32_t v = distances[h.toInt()];
            const int x = h.getX();

            logAi->debug("Next candidate hex: %d (v=%d, bestVal=%d, keepLeft=%d, x=%d, bestX=%d)", h.toInt(), v, bestVal, keepLeft, x, bestX);

            if (v < bestVal || (v == bestVal && (keepLeft ? x < bestX : x > bestX))) {
                best = &h;
                bestVal = v;
                bestX = x;
            }
        }

        if (best && distances[best->toInt()] < GameConstants::BFIELD_SIZE) {
            logAi->debug("Best candidate hex: %d", best->toInt());
            return best;
        } else {
            logAi->info("No good candidate hex (none reachable)");
            return nullptr;
        }
    }

}

MLBot::MLBot(const std::string & botname)
: botname(botname), msgbuf(500)
{
    std::ostringstream oss;
    // Store the memory address and include it in logging
    const auto * ptr = static_cast<const void *>(this);
    oss << ptr;
    addrstr = oss.str();
    info("+++ constructor +++"); // log after addrstr is set
    bot = AIFactory::createBattleAI(botname);
}

MLBot::~MLBot()
{
    info("--- destructor ---");
}

void MLBot::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AICombatOptions aiCombatOptions)
{
    info("*** initBattleInterface ***");
    cb = CB;
    colorname = cb->getPlayerID()->toString();
    bot->initBattleInterface(ENV, cb, aiCombatOptions);
}

void MLBot::addmsg(const CStack* astack, const CStack* vip, const std::string & event) {
    std::ostringstream oss;
    oss << boost::str(boost::format("[round %d][%d] %s") % nrounds % nturns % event) << "\n";

    for (const auto & cstack : battle->battleGetAllStacks()) {
        oss << boost::str(boost::format("- %s active=%d vip=%d alive=%d side=%d qty=%d basqty=%d position=%d shots=%d text=%s")
            % (cstack->alive() ? "S" : "X")
            % (cstack == astack)
            % (cstack == vip)
            % cstack->alive()
            % static_cast<int>(cstack->unitSide())
            % cstack->getCount()
            % cstack->unitBaseAmount()
            % cstack->getPosition().toInt()
            % cstack->shots.available()
            % cstack->getDescription()) << "\n";
    }

    for (const auto & obstacle : battle->battleGetAllObstacles()) {
        const auto & affected = obstacle->getAffectedTiles();
        auto tostr = [&](const BattleHexArray & v){
            std::ostringstream os;
            bool first = true;
            for (const auto& h : v) {
                if (!first) os << ' ';
                first = false;
                os << h.toInt();
            }
            return os.str();
        };

        std::string affectedstr = tostr(obstacle->getAffectedTiles());
        std::string blockedstr = tostr(obstacle->getBlockedTiles());

        oss << boost::str(boost::format("- O type=%d affected=[%s] blocked=[%s]")
            % static_cast<int>(obstacle->obstacleType)
            % affectedstr
            % blockedstr) << "\n";
    }


    msgbuf.push_back(oss.str());
}

void MLBot::actionStarted(const BattleID & bid, const BattleAction & action) {
    // addmsg(battle->battleActiveUnit(), vip, "actionStarted: " + action.toString()))
    const CStack * astack = nullptr;
    if (battle->battleActiveUnit())
        astack = battle->battleGetStackByID(battle->battleActiveUnit()->unitId());

    addmsg(astack, vip, "actionStarted: " + action.toString());
};

void MLBot::battleNewRound(const BattleID & bid) {
    ++nrounds;
    msgbuf.push_back(boost::str(boost::format("[round %d][%d] battleNewRound\n") % nrounds % nturns));
}


void MLBot::battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed)
{
    vip = nullptr;
    nturns = 0;
    nrounds = 0;
    battle = cb->getBattle(battleID);
    bot->battleStart(battleID, army1, army2, tile, hero1, hero2, side, replayAllowed);

    const auto * art = battle->battleGetMyHero()->getArt(ArtifactPosition::BACKPACK_START);
    if (art && art->getTypeId() == ArtifactID::GRAIL) {
        info("GRAIL found in hero -- looking for VIP stack");
        for (const auto & cstack : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE)) {
            // growth > 0 excludes ballistas, commanders, etc.
            if (cstack->unitType()->getGrowth() > 0 && cstack->isShooter()) {
                vip = cstack;
                break;
            }
        }
    }

    if (vip)
        info("Found VIP stack: %s", vip->getDescription());
    else
        info("Could not find VIP stack, will delegate all calls to %s", botname);

    msgbuf.push_back(boost::str(boost::format("[round %d][%d] battleStart\n") % nrounds % nturns));
}

void MLBot::yourTacticPhase(const BattleID & battleID, int distance)
{
    bot->yourTacticPhase(battleID, distance);
}

void MLBot::activeStack(const BattleID & bid, const CStack * astack)
{
    ++nturns;
    msgbuf.push_back(boost::str(boost::format("[round %d][%d] activeStack: %s\n") % nrounds % nturns % astack->getDescription()));

    // XXX: this absolutely can happen if defender is BattleAI: destroys catapult then camps in town forever
    // However, MMAI should retreat after MAX_ROUNDS anyway
    if (battle->battleGetRound() > Schema::V15::MAX_ROUNDS + 2) {
        error("More than %d rounds in this battle (vip=%d)", Schema::V15::MAX_ROUNDS + 2, vip ? vip->getDescription() : "n/a");
        for (const auto & msg : msgbuf)
            error(msg);
        throw std::runtime_error("More than " + std::to_string(Schema::V15::MAX_ROUNDS) + " rounds in this battle, aborting");
    }

    if (!vip) {
        debug("No VIP => invoke bot");
        bot->activeStack(bid, astack);
        return;
    }

    astack == vip
        ? handleVip(bid, astack)
        : handleGuard(bid, astack, vip);
}

/*
 * private
 */

void MLBot::handleVip(const BattleID & bid, const CStack * vip)
{
    // Just let the bot act (should shoot at someone)
    info("Handling VIP stack %s => invoke bot", vip->getDescription());
    bot->activeStack(bid, vip);
}

void MLBot::handleGuard(const BattleID & bid, const CStack * guard, const CStack * vip)
{
    info("Handling GUARD stack %s (vip=%s)", guard->getDescription(), vip->getDescription());

    if(!(vip->alive() && vip->canShoot())) {
        // XXX: for alive VIPs, this can only trigger when out of shots or forgetful
        //      (i.e. will NOT trigger if blocked by enemy)
        info("VIP is dead or can't shoot => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    // XXX: using getAttackableHexes with a friendly attacker is a convenient
    // way to get VIP's neighbouring hexes for moving guards to, as it handles
    // cases where VIP and/or guard are 2-hex units.
    // NOTE: hexes will be *accessible*, but not necessarily *reachable*
    const auto hexes = vip->getAttackableHexes(guard);

    if(hexes.empty()) {
        // No "attackable" hexes (i.e. no free spots next to VIP for this guard)
        // => let the bot act (should throw them forward)
        info("No 'attackable' hexes => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    auto targets = std::vector<BattleHex>{};
    targets.reserve(8); // max for 2-hex stack in the open

    bool canWait = guard->willMove() && !guard->waitedThisTurn;

    /*
     * First check if we already stand on any of those hexes => in-place attack, wait or defend
     * (avoids expensive reachability calculations)
     */

    for (const auto & hex : hexes) {
        debug("Consider guard hex %d ...", hex.toInt());
        if (guard->coversPos(hex)) {
            info("Already guarding on that hex");

            // 1. if neighbouring enemy => attack (in-place)
            for (const auto & target : battle->battleGetStacks())
            {
                if (guard->unitSide() != target->unitSide() && CStack::isMeleeAttackPossible(guard, target, guard->getPosition()))
                {
                    info("Attacking nearby unit...");
                    cb->battleMakeUnitAction(bid, BattleAction::makeMeleeAttack(guard, target, guard->getPosition()));
                    return;
                }
            }

            // 2. else if can wait => wait
            if (canWait)
            {
                info("Waiting...");
                cb->battleMakeUnitAction(bid, BattleAction::makeWait(guard));
                return;
            }

            // 3. else defend
            info("Defending...");
            cb->battleMakeUnitAction(bid, BattleAction::makeDefend(guard));
            return;
        }
        targets.push_back(hex);
    }

    auto speed = guard->getMovementRange();
    if (speed == 0) {
        // Not guarding, but 0 speed => let bot decide (e.g. attack if possible)
        info("Speed is 0 => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    /*
     * Try moving towards a guard target hex (wait first)
     */

    if (targets.empty()){
        throw std::runtime_error("no guard targets");
    }

    const auto rdebug = battle->getReachability(guard);

    auto skips = std::array<bool, GameConstants::BFIELD_SIZE> {};
    const BattleHex * target = PickClosestHex(guard, rdebug.distances, targets, skips);

    while(target && rdebug.distances.at(target->toInt()) > speed) {
        debug("Target hex %d not reachable: dist(%d) > speed(%d)", target->toInt(), rdebug.distances.at(target->toInt()), speed);
        if (canWait)
        {
            info("Waiting...");
            cb->battleMakeUnitAction(bid, BattleAction::makeWait(guard));
            return;
        }

        target = PickClosestHex(guard, rdebug.distances, NearbyMoveHexes(guard, *target), skips);
        info("Will try a closer hex: %d", target ? target->toInt() : -1);
    }

    if (!target) {
        // Maybe there were no targets to begin with (vip already surrounded)
        info("could not find new suitable target hex (VIP already surrounded?) => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    // Should not happen
    if (!target->isAvailable())
        throw std::runtime_error("Final target is not available: " + std::to_string(target->toInt()));


    debug("Final target hex towards VIP: %d", target->toInt());

    // Move towards the hex
    info("Moving towards final target at dist=%d (speed=%d)", rdebug.distances.at(target->toInt()), speed);

    // XXX: construct an explicit copy here (*target will dangling)
    cb->battleMakeUnitAction(bid, BattleAction::makeMove(guard, BattleHex(*target)));
}


/*
 * Logging
 */

template<typename... Args>
void MLBot::_log(const ELogLevel::ELogLevel level, const std::string & format, Args... args) const
{
    logAi->log(level, "MLBot-%s [%s] " + format, addrstr, colorname, args...);
}

template<typename... Args>
void MLBot::error(const std::string & format, Args... args) const
{
    log(ELogLevel::ERROR, format, args...);
}
template<typename... Args>
void MLBot::warn(const std::string & format, Args... args) const
{
    log(ELogLevel::WARN, format, args...);
}
template<typename... Args>
void MLBot::info(const std::string & format, Args... args) const
{
    log(ELogLevel::INFO, format, args...);
}
template<typename... Args>
void MLBot::debug(const std::string & format, Args... args) const
{
    log(ELogLevel::DEBUG, format, args...);
}
template<typename... Args>
void MLBot::trace(const std::string & format, Args... args) const
{
    log(ELogLevel::DEBUG, format, args...);
}
template<typename... Args>
void MLBot::log(const ELogLevel::ELogLevel level, const std::string & format, Args... args) const
{
    if(logAi->getEffectiveLevel() <= level)
        _log(level, format, args...);
}

void MLBot::error(const std::string & text) const
{
    log(ELogLevel::ERROR, text);
}
void MLBot::warn(const std::string & text) const
{
    log(ELogLevel::WARN, text);
}
void MLBot::info(const std::string & text) const
{
    log(ELogLevel::INFO, text);
}
void MLBot::debug(const std::string & text) const
{
    log(ELogLevel::DEBUG, text);
}
void MLBot::trace(const std::string & text) const
{
    log(ELogLevel::TRACE, text);
}
void MLBot::log(ELogLevel::ELogLevel level, const std::string & text) const
{
    if(logAi->getEffectiveLevel() <= level)
        _log(level, "%s", text);
}

void MLBot::error(const std::function<std::string()> & cb) const
{
    log(ELogLevel::ERROR, cb);
}
void MLBot::warn(const std::function<std::string()> & cb) const
{
    log(ELogLevel::WARN, cb);
}
void MLBot::info(const std::function<std::string()> & cb) const
{
    log(ELogLevel::INFO, cb);
}
void MLBot::debug(const std::function<std::string()> & cb) const
{
    log(ELogLevel::DEBUG, cb);
}
void MLBot::trace(const std::function<std::string()> & cb) const
{
    log(ELogLevel::TRACE, cb);
}
void MLBot::log(ELogLevel::ELogLevel level, const std::function<std::string()> & cb) const
{
    if(logAi->getEffectiveLevel() <= level)
        _log(level, "%s", cb());
}

}
