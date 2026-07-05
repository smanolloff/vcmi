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
#include <unordered_map>

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

    BattleHex PickIntermediateAirHex(
        const CStack * guard,
        std::span<const uint32_t> distances,
        const BattleHex & target
    ) {
        logAi->info("Looking for intermediate hex towards target=%d ...", target.toInt());
        BattleHex bestHex;
        int mindist = ReachabilityInfo::INFINITE_DIST;
        bool keepLeft = guard->unitSide() == BattleSide::ATTACKER;
        int bestX = keepLeft ? 999 : -999;

        if (distances.size() > GameConstants::BFIELD_SIZE)
            throw std::runtime_error("Unexpected distances size: " + std::to_string(distances.size()));

        for (int i = 0; i < GameConstants::BFIELD_SIZE; ++i) {
            auto dist = distances[i];
            auto hex = BattleHex(i);
            auto x = hex.getX();

            if (dist < mindist || (dist == mindist && (keepLeft ? x < bestX : x > bestX))) {
                mindist = dist;
                bestHex = hex;
                bestX = x;
                logAi->info("Potential intermediate air hex=%d dist=%d x=%d", target.toInt(), dist, x);
            } else {
                logAi->info("Bad intermediate air hex=%d dist=%d x=%d", target.toInt(), dist, x);
            }
        }

        logAi->info("Final intermediate air hex=%d", bestHex.toInt());

        return bestHex;

    }

    /*
     * From a list of candidate hexes, pick one which:
     *  1. is closest to the guard
     *  2. (if guard is attacker) has the lowest "X" coordinate ("keep left")
     *     (if guard is defender) has the highest "X" coordinate ("keep right")
     *
     * XXX: there is a BUG when the guard is flying:
     *      the "closer" hex may lead to a dead-end
     *
     * Example:
     *  stack "S" needs to reach "o" (x are unavailable/occupied hexes)
     *  If "S" is ground, then hex A is returned (lowest walking dist)
     *  If "S" is flying, then hex B is returned (lowest flying dist)
     *    If S can't reach B, then NearbyMoveHexes(B) is called which fails
     *    (all candidates are skipped as repeated)
     *
     * . . . . . S
     *  . . . . .
     * . . . . . .
     *  . . . x x
     * . . . A o B
     *  . . . x x
     *
     */
    BattleHex PickClosestLandHex(
        const CStack * guard,
        std::span<const uint32_t> distances,
        std::span<const BattleHex> candidates,
        std::span<bool> skips
    ) {
        if (candidates.empty()) return BattleHex();

        BattleHex best = BattleHex();

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
                best = h;
                bestVal = v;
                bestX = x;
            }
        }

        if (best.isValid() && distances[best.toInt()] < GameConstants::BFIELD_SIZE) {
            logAi->debug("Best candidate hex: %d", best.toInt());
            return best;
        } else {
            logAi->info("No good candidate hex (none reachable)");
            return BattleHex();
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

    // TODO: look for the strongest shooter instead
    //      + require at least 3 guards (+more if shooter is wide?)
    const auto * art = battle->battleGetMyHero()->getArt(ArtifactPosition::BACKPACK_START);
    if (art && art->getTypeId() == ArtifactID::GRAIL) {
        info("GRAIL found in hero -- looking for VIP stack");
        for (const auto & cstack : battle->battleGetStacks(CBattleInfoEssentials::EStackOwnership::ONLY_MINE)) {
            // growth > 0 excludes ballistas, commanders, etc.
            if (cstack->unitType()->getGrowth() > 0 && cstack->isShooter()) {
                vip = cstack;
                vipStartPos = vip->getPosition();
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

namespace
{
    BattleHex CloneHex(const BattleHex & bh, const std::initializer_list<BattleHex::EDir> & dirs)
    {
        auto res = bh;
        for (const auto dir : dirs)
            res = res.cloneInDirection(dir, false);
        return res;
    }

    // A custom hash function must be provided for the adjmap
    struct VipInfo
    {
        const BattleHex vipPos;
        const BattleSide vipSide;
        const bool vipWide;
        const bool guardWide;

        bool operator==(const VipInfo&) const = default;
    };

    struct VipInfoHash
    {
        std::size_t operator()(const VipInfo & vi) const
        {
            std::size_t h = std::hash<si16>{}(vi.vipPos.toInt());
            h ^= std::hash<int>{}(EI(vi.vipSide)) << 1;
            h ^= std::hash<bool>{}(vi.guardWide) << 2;
            h ^= std::hash<bool>{}(vi.vipWide) << 3;
            return h;
        }
    };

    std::vector<BattleHex> GuardableHexes(const CStack * vip, const CStack * guard)
    {
        auto res = std::vector<BattleHex>{};
        res.reserve(16);

        BattleHex vipHead = vip->getPosition();

        static auto cache = std::unordered_map<VipInfo, std::vector<BattleHex>, VipInfoHash>{};
        const auto vi = VipInfo{
            .vipPos=vip->getPosition(),
            .vipSide=vip->unitSide(),
            .vipWide=vip->doubleWide(),
            .guardWide=guard->doubleWide()
        };

        auto it = cache.find(vi);

        if(it != cache.end())
            return it->second;

        using EDir = BattleHex::EDir;
        auto L = EDir::LEFT;
        auto TL = EDir::TOP_LEFT;
        auto BL = EDir::BOTTOM_LEFT;
        auto R = EDir::RIGHT;
        auto TR = EDir::TOP_RIGHT;
        auto BR = EDir::BOTTOM_RIGHT;

        auto add = [&res, &vipHead](const std::initializer_list<BattleHex::EDir> dirs) {
            auto hex = CloneHex(vipHead, dirs);
            if (hex.isAvailable())
                res.push_back(hex);
        };

        auto convex = vip->unitSide() == BattleSide::LEFT_SIDE
            ? vipHead.getY() % 2 == 0
            : vipHead.getY() % 2 == 1;

        logAi->info("vipHead=%d vip->unitSide()=%d / guard->doubleWide()=%d / vip->doubleWide()=%d / vipHead.getY()=%d", vipHead.toInt(), EI(vip->unitSide()), EI(guard->doubleWide()), EI(vip->doubleWide()), EI(vipHead.getY()));

        if (vip->unitSide() == BattleSide::RIGHT_SIDE) {
            if (guard->doubleWide()) {
                if (vip->doubleWide()) {
                    if (vipHead.getY() < 5) {
                        /*
                         *  2-hex VIP, 2-hex guard
                         *  (R/upper/convex)         (R/upper/concave)
                         *  . A 8 C .                 . A 8 C .
                         * . . 6 1 3 .               . . 6 1 .
                         *  . 5 . x ~                 . 5 . x ~
                         * . . 7 2 4 .               . . 7 2 .
                         *  . B 9 D .                 . B 9 D .
                         */
                        add({TL}); // 1
                        add({BL}); // 2

                        if (convex) {
                            add({TR}); // 3
                            add({BR}); // 4
                        }

                        add({L, L}); // 5
                        add({L, TL}); // 6
                        add({L, BL}); // 7

                        add({TL, TL}); // 8
                        add({BL, BL}); // 9
                        add({TL, TL, L}); // A
                        add({BL, BL, L}); // B
                        add({TL, TR}); // C
                        add({BL, BR}); // D
                    } else {
                        /*
                         *  2-hex VIP, 2-hex guard
                         *  (R/lower/convex)       (R/lower/concave)
                         *  . B 9 D .               . B 9 D .
                         * . . 7 2 4 .             . . 7 2 .
                         *  . 5 . x ~               . 5 . x ~
                         * . . 6 1 3 .             . . 6 1 .
                         *  . A 8 C .               . A 8 C .
                         */
                        add({BL}); // 1
                        add({TL}); // 2

                        if (convex) {
                            add({BR}); // 3
                            add({TR}); // 4
                        }

                        add({L, L}); // 5
                        add({L, BL}); // 6
                        add({L, TL}); // 7

                        add({BL, BL}); // 8
                        add({TL, TL}); // 9
                        add({BL, BL, L}); // A
                        add({TL, TL, L}); // B
                        add({BL, BR}); // C
                        add({TL, TR}); // D
                    }
                } else {
                    if (vipHead.getY() < 5) {
                        /*
                         *  1-hex VIP, 2-hex guard
                         *  (R/upper/convex)       (R/upper/concave)
                         *  . . 6 8 .               . . 6 8 .
                         * . . . 4 1 .             . . . 4 .
                         *  . . 3 . x               . . 3 . x
                         * . . . 5 2 .             . . . 5 .
                         *  . . 7 9 .               . . 7 9 .
                         */
                        if (convex) {
                            add({TL}); // 1
                            add({BL}); // 2
                        }

                        add({L, L}); // 3
                        add({L, TL}); // 4
                        add({L, BL}); // 5

                        add({TL, TL, L}); // 6
                        add({BL, BL, L}); // 7
                        add({TL, TL}); // 8
                        add({BL, BL}); // 9
                    } else {
                        /*
                         *  1-hex VIP, 2-hex guard
                         *  (R/lower/convex)        (R/lower/concave)
                         *  . . 7 9 .                 . . 7 9 .
                         * . . . 5 2 .               . . . 5 .
                         *  . . 3 . x                 . . 3 . x
                         * . . . 4 1 .               . . . 4 .
                         *  . . 6 8 .                 . . 6 8 .
                         */
                        if (convex) {
                            add({BL}); // 1
                            add({TL}); // 2
                        }

                        add({L, L}); // 3
                        add({L, BL}); // 4
                        add({L, TL}); // 5

                        add({BL, BL, L}); // 6
                        add({TL, TL, L}); // 7
                        add({BL, BL}); // 8
                        add({TL, TL}); // 9
                    }
                }
            } else {
                if (vip->doubleWide()) {
                    if (vipHead.getY() < 5) {
                        /*
                         *  2-hex VIP, 1-hex guard
                         *  (R/upper/convex)         (R/upper/concave)
                         *  . . B D F                 . . . B D F
                         * . . 8 2 4 6                 . . 8 2 4
                         *  . A 1 x ~                 . . A 1 x ~
                         * . . 9 3 5 7                 . . 9 3 5
                         *  . . C E G                 . . . C E G
                         */
                        add({L}); // 1
                        add({TL}); // 2
                        add({BL}); // 3
                        add({TR}); // 4
                        add({BR}); // 5

                        if (convex) {
                            add({R, TR}); // 6
                            add({R, BR}); // 7
                        }

                        add({L, TL}); // 8
                        add({L, BL}); // 9
                        add({L, L}); // A
                        add({TL, TL}); // B
                        add({BL, BL}); // C
                        add({TL, TR}); // D
                        add({BL, BR}); // E
                        add({TR, TR}); // F
                        add({BR, BR}); // G
                    } else {
                        /*
                         *  2-hex VIP, 1-hex guard
                         *  (R/lower/convex)         (R/lower/concave)
                         *  . . C E G                 . . C E G
                         * . . 9 3 5 7               . . 9 3 5
                         *  . A 1 x ~                 . A 1 x ~
                         * . . 8 2 4 6               . . 8 2 4
                         *  . . B D F                 . . B D F
                         */
                        add({L}); // 1
                        add({BL}); // 2
                        add({TL}); // 3
                        add({BR}); // 4
                        add({TR}); // 5

                        if (convex) {
                            add({R, BR}); // 6
                            add({R, TR}); // 7
                        }

                        add({L, BL}); // 8
                        add({L, TL}); // 9
                        add({L, L}); // A
                        add({BL, BL}); // B
                        add({TL, TL}); // C
                        add({BL, BR}); // D
                        add({TL, TR}); // E
                        add({BR, BR}); // F
                        add({TR, TR}); // G
                    }
                } else {
                    if (vipHead.getY() < 5) {
                        /*
                         *  1-hex VIP, 1-hex guard
                         *  (R/upper/convex)         (R/upper/concave)
                         *  . . . 9 B                 . . . 9 B
                         * . . . 6 2 4               . . . 6 2
                         *  . . 8 1 x                 . . 8 1 x
                         * . . . 7 3 5               . . . 7 3
                         *  . . . A C                 . . . A C
                         */
                        add({L}); // 1
                        add({TL}); // 2
                        add({BL}); // 3

                        if (convex) {
                            add({TR}); // 4
                            add({BR}); // 5
                        }

                        add({L, TL}); // 6
                        add({L, BL}); // 7
                        add({L, L}); // 8
                        add({TL, TL}); // 9
                        add({BL, BL}); // A
                        add({TL, TR}); // B
                        add({BL, BR}); // C

                    } else {
                        /*
                         *  1-hex VIP, 1-hex guard
                         *  (R/lower/convex)         (R/lower/concave)
                         *  . . . A C                 . . . A C
                         * . . . 7 3 5               . . . 7 3
                         *  . . 8 1 x                 . . 8 1 x
                         * . . . 6 2 4               . . . 6 2
                         *  . . . 9 B                 . . . 9 B
                         */
                        add({L}); // 1
                        add({BL}); // 2
                        add({TL}); // 3

                        if (convex) {
                            add({BR}); // 4
                            add({TR}); // 5
                        }

                        add({L, BL}); // 6
                        add({L, TL}); // 7
                        add({L, L}); // 8
                        add({BL, BL}); // 9
                        add({TL, TL}); // A
                        add({BL, BR}); // B
                        add({TL, TR}); // C
                    }
                }
            }
        } else {
            if (guard->doubleWide()) {
                if (vip->doubleWide()) {
                    if (vipHead.getY() < 5) {
                        /*
                         *  2-hex VIP, 2-hex guard
                         *  (L/upper/convex)         (L/upper/concave)
                         *  . C 8 A .                 . C 8 A .
                         * . 3 1 6 . .                 . 1 6 . .
                         *  ~ x . 5 .                 ~ x . 5 .
                         * . 4 2 7 . .                 . 2 7 . .
                         *  . D 9 B .                 . D 9 B .
                         */
                        add({TR}); // 1
                        add({BR}); // 2

                        if (convex) {
                            add({TL}); // 3
                            add({BL}); // 4
                        }

                        add({R, R}); // 5
                        add({R, TR}); // 6
                        add({R, BR}); // 7

                        add({TR, TR}); // 8
                        add({BR, BR}); // 9
                        add({TR, TR, R}); // A
                        add({BR, BR, R}); // B
                        add({TR, TL}); // C
                        add({BR, BL}); // D
                    } else {
                        /*
                         *  2-hex VIP, 2-hex guard
                         *  (L/lower/convex)         (L/lower/concave)
                         *  . D 9 B .                 . D 9 B .
                         * . 4 2 7 . .                 . 2 7 . .
                         *  ~ x . 5 .                 ~ x . 5 .
                         * . 3 1 6 . .                 . 1 6 . .
                         *  . C 8 A .                 . C 8 A .
                         */
                        add({BR}); // 1
                        add({TR}); // 2

                        if (convex) {
                            add({BL}); // 3
                            add({TL}); // 4
                        }

                        add({R, R}); // 5
                        add({R, BR}); // 6
                        add({R, TR}); // 7

                        add({BR, BR}); // 8
                        add({TR, TR}); // 9
                        add({BR, BR, R}); // A
                        add({TR, TR, R}); // B
                        add({BR, BL}); // C
                        add({TR, TL}); // D
                    }
                } else {
                    if (vipHead.getY() < 5) {
                        /*
                         *  1-hex VIP, 2-hex guard
                         *  (L/upper/convex)       (L/upper/concave)
                         *  . 8 6 . .               . 8 6 . .
                         * . 1 4 . . .               . 4 . . .
                         *  x . 3 . .               x . 3 . .
                         * . 2 5 . . .               . 5 . . .
                         *  . 9 7 . .               . 9 7 . .
                         */
                        if (convex) {
                            add({TR}); // 1
                            add({BR}); // 2
                        }

                        add({R, R}); // 3
                        add({R, TR}); // 4
                        add({R, BR}); // 5

                        add({TR, TR, R}); // 6
                        add({BR, BR, R}); // 7
                        add({TR, TR}); // 8
                        add({BR, BR}); // 9
                    } else {
                        /*
                         *  1-hex VIP, 2-hex guard
                         *  (L/lower/convex)        (L/lower/concave)
                         *  . 9 7 . .                . 9 7 . .
                         * . 2 5 . . .                . 5 . . .
                         *  x . 3 . .                x . 3 . .
                         * . 1 4 . . .                . 4 . . .
                         *  . 8 6 . .                . 8 6 . .
                         */
                        if (convex) {
                            add({BR}); // 1
                            add({TR}); // 2
                        }

                        add({R, R}); // 3
                        add({R, BR}); // 4
                        add({R, TR}); // 5

                        add({BR, BR, R}); // 6
                        add({TR, TR, R}); // 7
                        add({BR, BR}); // 8
                        add({TR, TR}); // 9
                    }
                }
            } else {
                if (vip->doubleWide()) {
                    if (vipHead.getY() < 5) {
                        /*
                         *  2-hex VIP, 1-hex guard
                         *  (L/upper/convex)         (L/upper/concave)
                         *  F D B . .                 F D B . . .
                         * 6 4 2 8 . .                 4 2 8 . . .
                         *  ~ x 1 A .                 ~ x 1 A . .
                         * 7 5 3 9 . .                 5 3 9 . . .
                         *  G E C . .                 G E C . . .
                         */
                        add({R}); // 1
                        add({TR}); // 2
                        add({BR}); // 3
                        add({TL}); // 4
                        add({BL}); // 5

                        if (convex) {
                            add({L, TL}); // 6
                            add({L, BL}); // 7
                        }

                        add({R, TR}); // 8
                        add({R, BR}); // 9
                        add({R, R}); // A
                        add({TR, TR}); // B
                        add({BR, BR}); // C
                        add({TR, TL}); // D
                        add({BR, BL}); // E
                        add({TL, TL}); // F
                        add({BL, BL}); // G
                    } else {
                        /*
                         *  2-hex VIP, 1-hex guard
                         *  (L/lower/convex)         (L/lower/concave)
                         *  G E C . .                 G E C . .
                         * 7 5 3 9 . .                 5 3 9 . .
                         *  ~ x 1 A .                 ~ x 1 A .
                         * 6 4 2 8 . .                 4 2 8 . .
                         *  F D B . .                 F D B . .
                         */
                        add({R}); // 1
                        add({BR}); // 2
                        add({TR}); // 3
                        add({BL}); // 4
                        add({TL}); // 5

                        if (convex) {
                            add({L, BL}); // 6
                            add({L, TL}); // 7
                        }

                        add({R, BR}); // 8
                        add({R, TR}); // 9
                        add({R, R}); // A
                        add({BR, BR}); // B
                        add({TR, TR}); // C
                        add({BR, BL}); // D
                        add({TR, TL}); // E
                        add({BL, BL}); // F
                        add({TL, TL}); // G
                    }
                } else {
                    if (vipHead.getY() < 5) {
                        /*
                         *  1-hex VIP, 1-hex guard
                         *  (L/upper/convex)         (L/upper/concave)
                         *  B 9 . . .                 B 9 . . .
                         * 4 2 6 . . .                 2 6 . . .
                         *  x 1 8 . .                 x 1 8 . .
                         * 5 3 7 . . .                 3 7 . . .
                         *  C A . . .                 C A . . .
                         */
                        add({R}); // 1
                        add({TR}); // 2
                        add({BR}); // 3

                        if (convex) {
                            add({TL}); // 4
                            add({BL}); // 5
                        }

                        add({R, TR}); // 6
                        add({R, BR}); // 7
                        add({R, R}); // 8
                        add({TR, TR}); // 9
                        add({BR, BR}); // A
                        add({TR, TL}); // B
                        add({BR, BL}); // C
                    } else {
                        /*
                         *  1-hex VIP, 1-hex guard
                         *  (L/lower/convex)         (L/lower/concave)
                         *  C A . . .                 C A . . .
                         * 5 3 7 . . .                 3 7 . . .
                         *  x 1 8 . .                 x 1 8 . .
                         * 4 2 6 . . .                 2 6 . . .
                         *  B 9 . . .                 B 9 . . .
                         */
                        add({R}); // 1
                        add({BR}); // 2
                        add({TR}); // 3

                        if (convex) {
                            add({BL}); // 4
                            add({TL}); // 5
                        }

                        add({R, BR}); // 6
                        add({R, TR}); // 7
                        add({R, R}); // 8
                        add({BR, BR}); // 9
                        add({TR, TR}); // A
                        add({BR, BL}); // B
                        add({TR, TL}); // C
                    }
                }
            }
        }

        cache.try_emplace(vi, res);
        return res;
    }
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

    if(vip->getPosition().getX() != vipStartPos.getX()) {
        // Guard positions become weird when vip moves away from the edge
        info("VIP is displaced (x=%d, startx=%d) => invoke bot", vip->getPosition().getX(), vipStartPos.getX());
        bot->activeStack(bid, guard);
        return;
    }

    auto speed = guard->getMovementRange();
    if (speed == 0) {
        // Not guarding, but 0 speed => let bot decide (e.g. attack if possible)
        info("Speed is 0 => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    if(battle->battleIsUnitBlocked(vip) && !vip->canShootBlocked()) {
        info("VIP is blocked => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    // NOTE: Some (or even all) of these may be reachable by the current unit
    const auto hexes = GuardableHexes(vip, guard);
    const bool canWait = guard->willMove() && !guard->waitedThisTurn;

    // std::cout << "=== HEXES: [";
    // for (const auto & h : hexes)
    //     std::cout << " " << h.toInt();
    // std::cout << " ]\n";

    /*
     * Try moving towards a guard target hex (wait first)
     */

    const auto distances = battle->getReachability(guard).distances;

    auto skips = std::array<bool, GameConstants::BFIELD_SIZE> {};
    BattleHex target;
    uint32_t minDist = ReachabilityInfo::INFINITE_DIST;

    for (const auto hex : hexes) {
        auto dist = distances.at(hex.toInt());

        if (dist < minDist) {
            minDist = dist;
            target = hex;
            if (dist <= speed)
            {
                info("Found reachable target hex %d: dist=%d <= speed=%d", hex, dist, speed);

                // if target hex has a neighbouring enemy => move + attack
                for (const auto & enemy : battle->battleGetStacks())
                {
                    if (guard->unitSide() != enemy->unitSide() && CStack::isMeleeAttackPossible(guard, enemy, hex))
                    {
                        info("Will attack from hex %d at %s...", target.toInt(), enemy->getDescription());
                        cb->battleMakeUnitAction(bid, BattleAction::makeMeleeAttack(guard, enemy, hex));
                        return;
                    }
                }

                // otherwise, if already there, wait or defend
                info("No enemies neighbouring target hex %d", hex.toInt());

                if (guard->getPosition() == hex) {
                    if (canWait)
                    {
                        info("Already guarding hex %d, will wait...", hex.toInt());
                        cb->battleMakeUnitAction(bid, BattleAction::makeWait(guard));
                        return;
                    }

                    info("Already guarding hex %d, will defend...", hex.toInt());
                    cb->battleMakeUnitAction(bid, BattleAction::makeDefend(guard));
                    return;
                }

                // otherwise, just move there
                info("Moving to target hex %d", hex.toInt());
                cb->battleMakeUnitAction(bid, BattleAction::makeMove(guard, hex));
                return;
            }

            debug("Unreachable target hex %d: dist=%d > speed=%d", hex.toInt(), dist, speed);
        } else {
            debug("Irrelevant target hex %d: dist=%d >= minDist=%d", hex.toInt(), dist, minDist);
        }

    }

    if (!target.isValid()) {
        // Maybe there were no targets to begin with (vip already surrounded)
        info("could not find target hex (VIP already surrounded?) => invoke bot");
        bot->activeStack(bid, guard);
        return;
    }

    while(distances.at(target.toInt()) > speed) {
        debug("Target hex %d not reachable: dist=%d > speed=%d", target.toInt(), distances.at(target.toInt()), speed);

        if (canWait) {
            info("Waiting...");
            cb->battleMakeUnitAction(bid, BattleAction::makeWait(guard));
            return;
        }

        if (guard->hasBonusOfType(BonusType::FLYING)) {
            // this scans through the entire battlefield and directly returns the closest reachable hex
            target = PickIntermediateAirHex(guard, distances, target);
            break;
        }

        target = PickClosestLandHex(guard, distances, NearbyMoveHexes(guard, target), skips);

        // This should not happen because if we are here, it means distances[target] was < INFINITE_DIST to begin with
        // i.e. there exists some path to the target
        if (!target.isValid())
            throw std::runtime_error("Failed to find any hex towards the target. This should not happen.");

        info("Will try a closer hex: %d", target.toInt());
    }


    // Move towards the hex
    info("Intermediate reachable target: hex=%d dist=%d speed=%d", target.toInt(), distances.at(target.toInt()), speed);

    // if target hex has a neighbouring enemy => move + attack
    for (const auto & enemy : battle->battleGetStacks())
    {
        if (guard->unitSide() != enemy->unitSide() && CStack::isMeleeAttackPossible(guard, enemy, target))
        {
            info("Will move to hex %d and attack at %s", target.toInt(), enemy->getDescription());
            cb->battleMakeUnitAction(bid, BattleAction::makeMeleeAttack(guard, enemy, target));
            return;
        }
    }

    // else just move there
    info("Will move to hex %d", target.toInt());
    cb->battleMakeUnitAction(bid, BattleAction::makeMove(guard, target));
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
