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

#include "NetPacks.h"
#include "BAI.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "battle/ReachabilityInfo.h"
#include "types/hexaction.h"
#include "types/stack.h"
#include "vcmi/Creature.h"
#include "vstd/CLoggerBase.h"
#include <stdexcept>

namespace MMAI {
    using ErrType = Export::ErrType;

    void BAI::error(const std::string &text) const { logAi->error("BAI [%s] %s", sidestr, text); }
    void BAI::warn(const std::string &text) const { logAi->warn("BAI [%s] %s", sidestr, text); }
    void BAI::info(const std::string &text) const { logAi->info("BAI [%s] %s", sidestr, text); }
    void BAI::debug(const std::string &text) const { logAi->debug("BAI [%s] %s", sidestr, text); }

    // For debugging
    MMAI::Export::Action randomValidAction(const MMAI::Export::ActMask &mask) {
        auto validActions = std::vector<MMAI::Export::Action>{};

        for (int j = 1; j < mask.size(); j++) {
            if (mask.at(j))
                validActions.push_back(j);
        }

        if (validActions.empty())
            throw std::runtime_error("No valid actions?!");

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, validActions.size() - 1);
        int randomIndex = dist(gen);
        return validActions.at(randomIndex);
    }

    void BAI::activeStack(const CStack * astack) {
        info("*** activeStack ***");
        // print("activeStack called for " + astack->nodeName());

        battlefield = std::make_unique<Battlefield>(cb.get(), astack);
        result = std::make_unique<Export::Result>(buildResult(*battlefield));

        std::shared_ptr<BattleAction> ba;

        static_assert(EI(BattleSide::ATTACKER) == EI(Export::Side::ATTACKER));
        static_assert(EI(BattleSide::DEFENDER) == EI(Export::Side::DEFENDER));

        // printf("%s\n", renderANSI().c_str());
        // std::cout << "Press Enter to continue...";
        // std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        while(true) {
            auto _action = getAction(result.get());
            allactions.push_back(_action);
            action = std::make_unique<Action>(_action, battlefield.get());
            info("Got action: " + std::to_string(action->action) + " (" + action->name() + ")");
            auto actres = buildAction(*battlefield, *action);

            ba = actres.battleAction;
            result->errmask = actres.errmask;
            attackLogs.clear();

            if (ba) {
                ASSERT(actres.errmask == 0, "unexpected errmask: " + std::to_string(actres.errmask));
                debug("Action is VALID: " + action->name());
                break;
            } else {
                ASSERT(actres.errmask > 0, "unexpected errmask: " + std::to_string(actres.errmask));

                // DEBUG ONLY (uncomment if needed):
                // auto errstring = std::accumulate(errmsgs.begin(), errmsgs.end(), std::string(),
                //     [](auto &a, auto &b) { return a + "\n" + b; });
                //
                // warn("Action is INVALID: " + actname + ":\n" + errstring);
            }
        }

        debug("cb->battleMakeUnitAction(*ba)");
        cb->battleMakeUnitAction(*ba);
        return;
    }

    Export::Result BAI::buildResult(Battlefield &bf) {
        int dmgReceived = 0;
        int unitsLost = 0;
        int valueLost = 0;
        int dmgDealt = 0;
        int unitsKilled = 0;
        int valueKilled = 0;
        int side0ArmyValue = 0;
        int side1ArmyValue = 0;

        for (auto &al : attackLogs) {
            if (al.isOurStackAttacked) {
                // note: in VCMI there is no excess dmg if stack is killed
                dmgReceived += al.dmg;
                unitsLost += al.units;
                valueLost += al.value;
            } else {
                dmgDealt += al.dmg;
                unitsKilled += al.units;
                valueKilled += al.value;
            }
        }

        for (auto &stack : cb->battleGetStacks()) {
            auto value = stack->getCount() * stack->unitType()->getAIValue();
            stack->unitSide() == 0
                ? side0ArmyValue += value
                : side1ArmyValue += value;
        }

        return Export::Result(
            bf.exportState(),
            bf.exportActMask(),
            Export::Side(cb->battleGetMySide()),
            dmgDealt,
            dmgReceived,
            unitsLost,
            unitsKilled,
            valueLost,
            valueKilled,
            side0ArmyValue,
            side1ArmyValue
        );
    }

    BuildActionResult BAI::buildAction(Battlefield &bf, Action &action) {
        auto res = BuildActionResult();
        auto apos = bf.astack->getPosition();

        // auto origq = std::vector<battle::Units>{};
        // cb->battleGetTurnOrder(origq, QSIZE, 0);
        // auto myq = bf.getQueue(cb.get());
        auto [x, y] = Hex::CalcXY(apos);
        auto hex = bf.hexes.at(y).at(x);
        ASSERT(hex.stack->attrs.at(EI(StackAttr::QueuePos)) == 0, "expected 0 queue pos");
        ASSERT(hex.stack->attrs.at(EI(StackAttr::IsActive)) == 1, "expected active=1");

        if (!action.hex) {
            switch(NonHexAction(action.action)) {
            break; case NonHexAction::RETREAT:
                res.setAction(BattleAction::makeRetreat(cb->battleGetMySide()));
            break; case NonHexAction::WAIT:
                (bf.astack->waitedThisTurn)
                    ? res.addError(ErrType::ALREADY_WAITED)
                    : res.setAction(BattleAction::makeWait(bf.astack));
            break; default:
                throw std::runtime_error("Unexpected non-hex action: " + std::to_string(action.action));
            }

            return res;
        }

        // With action masking, invalid actions should never occur
        // However, for manual playing/testing, it's bad to raise exceptions
        // => return errmask and raise in Gym env if errmask != 0
        auto &bhex = action.hex->bhex;
        auto &cstack = action.hex->stack->cstack;
        if (action.hex->hexactmask.at(EI(action.hexaction))) {
            // Action is VALID
            // XXX: Do minimal asserts to prevent bugs with nullptr deref
            //      Server will log any attempted invalid actions otherwise
            switch(action.hexaction) {
            case HexAction::MOVE:
                (bhex.hex == bf.astack->getPosition().hex)
                ? res.setAction(BattleAction::makeDefend(bf.astack))
                : res.setAction(BattleAction::makeMove(bf.astack, bhex));
            break;
            case HexAction::SHOOT:
                ASSERT(cstack, "no target to shoot");
                res.setAction(BattleAction::makeShotAttack(bf.astack, cstack));
            break;
            case HexAction::AMOVE_TL:
            case HexAction::AMOVE_TR:
            case HexAction::AMOVE_R:
            case HexAction::AMOVE_BR:
            case HexAction::AMOVE_BL:
            case HexAction::AMOVE_L: {
                auto &edir = AMOVE_TO_EDIR.at(action.hexaction);
                auto nbh = bhex.cloneInDirection(edir, false); // neighbouring bhex
                ASSERT(nbh.isAvailable(), "mask allowed attack to an unavailable hex #" + std::to_string(nbh.hex));
                auto estack = cb->battleGetStackByPos(nbh);
                ASSERT(estack, "no enemy stack for melee attack");
                res.setAction(BattleAction::makeMeleeAttack(bf.astack, nbh, bhex));
            }
            break;
            case HexAction::AMOVE_2BL:
            case HexAction::AMOVE_2L:
            case HexAction::AMOVE_2TL:
            case HexAction::AMOVE_2TR:
            case HexAction::AMOVE_2R:
            case HexAction::AMOVE_2BR: {
                ASSERT(bf.astack->doubleWide(), "got AMOVE_2 action for a single-hex stack");
                auto &edir = AMOVE_TO_EDIR.at(action.hexaction);
                auto obh = bf.astack->occupiedHex();
                auto nbh = obh.cloneInDirection(edir, false); // neighbouring bhex
                ASSERT(nbh.isAvailable(), "mask allowed attack to an unavailable hex #" + std::to_string(nbh.hex));
                auto estack = cb->battleGetStackByPos(nbh);
                ASSERT(estack, "no enemy stack for melee attack");
                res.setAction(BattleAction::makeMeleeAttack(bf.astack, bhex, nbh));
            }
            break;
            default:
                throw std::runtime_error("Unexpected hexaction: " + std::to_string(EI(action.hexaction)));
            }

            return res;
        }

        // Action is INVALID

        // XXX:
        // mask prevents certain actions, but during TESTING
        // those actions may be taken anyway.
        //
        // IF we are here, it means the mask disallows that action
        //
        // => *throw* errors here only if the mask SHOULD HAVE ALLOWED it
        //    and *set* regular, non-throw errors otherwise
        //
        auto rinfo = cb->getReachability(bf.astack);
        auto ainfo = cb->getAccesibility();

        switch(action.hexaction) {
            case HexAction::MOVE:
            case HexAction::AMOVE_TL:
            case HexAction::AMOVE_TR:
            case HexAction::AMOVE_R:
            case HexAction::AMOVE_BR:
            case HexAction::AMOVE_BL:
            case HexAction::AMOVE_L:
            case HexAction::AMOVE_2BL:
            case HexAction::AMOVE_2L:
            case HexAction::AMOVE_2TL:
            case HexAction::AMOVE_2TR:
            case HexAction::AMOVE_2R:
            case HexAction::AMOVE_2BR: {
                auto a = ainfo.at(action.hex->bhex);
                if (a == EAccessibility::OBSTACLE) {
                    auto hs = action.hex->state;
                    ASSERT(hs == HexState::OBSTACLE, "incorrect hex state -- expected OBSTACLE, got: " + std::to_string(EI(hs)) + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_BLOCKED);
                } else if (a == EAccessibility::ALIVE_STACK) {
                    auto bh = action.hex->bhex;
                    if (bh.hex == bf.astack->getPosition().hex) {
                        // means we want to defend (moving to self)
                        // or attack from same hex we're currently at
                        // this should always be allowed
                        ASSERT(false, "mask prevented (A)MOVE to own hex" + debugInfo(action, bf.astack, nullptr));
                    } else if (bh.hex == bf.astack->occupiedHex().hex) {
                        ASSERT(rinfo.distances.at(bh) == ReachabilityInfo::INFINITE_DIST, "mask prevented (A)MOVE to self-occupied hex" + debugInfo(action, bf.astack, nullptr));
                        // means we can't fit on our own back hex
                        res.addError(ErrType::HEX_BLOCKED);
                    }
                    // means we try to move onto another stack
                    res.addError(ErrType::HEX_BLOCKED);
                }

                // only remaining is ACCESSIBLE
                expect(a == EAccessibility::ACCESSIBLE, "accessibility should've been ACCESSIBLE, was: %d", a);

                if (action.hexaction == HexAction::MOVE) {
                    ASSERT(action.hex->state == HexState::FREE_UNREACHABLE, "mask prevented (A)MOVE to a reachable bhex" + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_UNREACHABLE);
                    break;
                }

                auto nbh = BattleHex{};

                if (action.hexaction < HexAction::AMOVE_2BL) {
                    auto edir = AMOVE_TO_EDIR.at(action.hexaction);
                    nbh = bhex.cloneInDirection(edir, false);
                } else {
                    if (!bf.astack->doubleWide()) {
                        res.addError(ErrType::INVALID_DIR);
                        break;
                    }

                    auto edir = AMOVE_TO_EDIR.at(action.hexaction);
                    nbh = bf.astack->occupiedHex().cloneInDirection(edir, false);
                }

                if (!nbh.isAvailable()) {
                    res.addError(ErrType::HEX_MELEE_NA);
                    break;
                }

                auto estack = cb->battleGetStackByPos(nbh);

                if (!estack) {
                    res.addError(ErrType::STACK_NA);
                    break;
                }

                if (estack->unitSide() == bf.astack->unitSide()) {
                    res.addError(ErrType::FRIENDLY_FIRE);
                }
            }
            break;
            case HexAction::SHOOT:
                if (!cstack)
                    res.addError(ErrType::STACK_NA);
                else if (cstack->unitSide() == bf.astack->unitSide())
                    res.addError(ErrType::FRIENDLY_FIRE);
                else {
                    ASSERT(!cb->battleCanShoot(bf.astack, bhex), "mask prevented SHOOT at a shootable bhex " + action.hex->name());
                    res.addError(ErrType::CANNOT_SHOOT);
                }
            break;
            default:
                throw std::runtime_error("Unexpected hexaction: " + std::to_string(EI(action.hexaction)));
            }

        ASSERT(res.errmask, "Could not identify why the action is invalid" + debugInfo(action, bf.astack, nullptr));

        return res;
    }

    std::string BAI::debugInfo(Action &action, const CStack* astack, BattleHex* nbh) {
        auto info = std::stringstream();
        info << "\n*** DEBUG INFO ***\n";
        info << "action: " << action.name() << " [" << action.action << "]\n";
        info << "action.hex->bhex.hex = " << action.hex->bhex.hex << "\n";

        auto ainfo = cb->getAccesibility();
        auto rinfo = cb->getReachability(astack);

        info << "ainfo[bhex]=" << EI(ainfo.at(action.hex->bhex.hex)) << "\n";
        info << "rinfo.distances[bhex] <= astack->speed(): " << (rinfo.distances[action.hex->bhex.hex] <= astack->speed()) << "\n";

        info << "action.hex->name = " << action.hex->name() << "\n";
        info << "action.hex->state = " << EI(action.hex->state) << "\n";
        info << "action.hex->hexactmask = [";
        for (const auto& b : action.hex->hexactmask)
            info << int(b) << ",";
        info << "]\n";

        info << "action.hex->stack->attrs: [";
        for (const auto& a : action.hex->stack->attrs)
            info << a << ",";
        info << "]\n";

        auto cstack = action.hex->stack->cstack;
        if (cstack) {
            info << "cstack->getPosition().hex=" << cstack->getPosition().hex << "\n";
            info << "cstack->slot=" << cstack->unitSlot() << "\n";
            info << "cstack->doubleWide=" << cstack->doubleWide() << "\n";
            info << "cb->battleCanShoot(cstack)=" << cb->battleCanShoot(cstack) << "\n";
        } else {
            info << "cstack: (nullptr)\n";
        }

        info << "astack->getPosition().hex=" << astack->getPosition().hex << "\n";
        info << "astack->slot=" << astack->unitSlot() << "\n";
        info << "astack->doubleWide=" << astack->doubleWide() << "\n";
        info << "cb->battleCanShoot(astack)=" << cb->battleCanShoot(astack) << "\n";

        if (nbh) {
            info << "nbh->hex=" << nbh->hex << "\n";
            info << "ainfo[nbh]=" << EI(ainfo.at(*nbh)) << "\n";
            info << "rinfo.distances[nbh] <= astack->speed(): " << (rinfo.distances[*nbh] <= astack->speed()) << "\n";

            if (cstack)
                info << "astack->isMeleeAttackPossible(...)=" << astack->isMeleeAttackPossible(astack, cstack, *nbh) << "\n";
        }

        info << "\nACTION TRACE:\n";
        for (const auto& a : allactions)
            info << a << ",";

        info << "\nRENDER:\n";
        info << renderANSI();

        return info.str();
    }

    void BAI::battleStacksAttacked(const std::vector<BattleStackAttacked> &bsa, bool ranged) {
        info("*** battleStacksAttacked ***");

        for(auto & elem : bsa) {
            auto * defender = cb->battleGetStackByID(elem.stackAttacked, false);
            auto * attacker = cb->battleGetStackByID(elem.attackerID, false);

            ASSERT(defender, "defender is NULL");

            auto al = AttackLog(
                // XXX: attacker can be NULL when an effect does dmg (eg. Acid)
                attacker ? attacker->unitSlot() : SlotID(-1),
                defender->unitSlot(),
                defender->getOwner() == cb->getPlayerID(),
                elem.damageAmount,
                elem.killedAmount,
                elem.killedAmount * defender->creatureId().toCreature()->getAIValue()
            );
            attackLogs.push_back(std::move(al));
        }
    }

    // NOTE: not triggered for retreat
    void BAI::actionFinished(const BattleAction &action) {
        debug("*** actionFinished - START***");
        auto shouldupdate = false;

        for (auto &cstack : cb->battleGetAllStacks()) {
            if(cstack && !cstack->alive()) {
                shouldupdate = true;
                break;
            }
        }

        if (shouldupdate) {
            if (!battlefield) {
                // Enemy was first and attacked us before our first turn
                // => battlefield will be nullptr in this case
                auto stack = cb->battleGetStacks(CBattleCallback::ONLY_MINE).at(0);
                battlefield = std::make_unique<Battlefield>(cb.get(), stack);
            }
            battlefield->offTurnUpdate(cb.get());
        }
    }

    void BAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side_, bool replayAllowed) {
        info("*** battleStart ***");
        // side is FALSE for attacker
        if (side_) {
            debug("Side: DEFENDER (" + std::to_string(static_cast<int>(side)) + ")");
            side = BattleSide::DEFENDER;
        } else {
            debug("Side: ATTACKER (" + std::to_string(static_cast<int>(side)) + ")");
            side = BattleSide::ATTACKER;
        }

        ASSERT(getAction, "BAI->getAction is null!");
    }

    void BAI::battleEnd(const BattleResult *br, QueryID queryID) {
        info("*** battleEnd (QueryID: " + std::to_string(queryID.getNum()) + ") ***");
        // Export::Result res(std::move(*result), true);
        auto victory = br->winner == cb->battleGetMySide();

        // Null battlefield means the battle ended without us receiving a turn
        // (can only happen if we are DEFENDER)
        if (!battlefield) {
            ASSERT(side == BattleSide::DEFENDER, "no battlefield, but we are ATTACKER");
            return;
        }

        result = std::make_unique<Export::Result>(buildResult(*battlefield), victory);
        ASSERT(result->ended, "expected result->ended to be true");
    }
}
