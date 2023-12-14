#include "NetPacks.h"
#include "BAI.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "types/hexaction.h"
#include "types/stack.h"
#include "vstd/CLoggerBase.h"
#include <stdexcept>

namespace MMAI {
    // HNS = short for HexNState
    #define HNS(hs) hexStateNMap.at(static_cast<size_t>(HexState::hs))

    using ErrType = Export::ErrType;

    void BAI::error(const std::string &text) const { logAi->error("BAI [%s] %s", sidestr, text); }
    void BAI::warn(const std::string &text) const { logAi->warn("BAI [%s] %s", sidestr, text); }
    void BAI::info(const std::string &text) const { logAi->info("BAI [%s] %s", sidestr, text); }
    void BAI::debug(const std::string &text) const { logAi->debug("BAI [%s] %s", sidestr, text); }

    void BAI::activeStack(const CStack * astack)
    {
        info("*** activeStack ***");
        // print("activeStack called for " + astack->nodeName());

        printf("ASFGDFDSAFSAFSADFSADFASD\n");
        info("BAI1");
        battlefield = std::make_unique<Battlefield>(cb.get(), astack);
        info("BAI2");
        result = std::make_unique<Export::Result>(buildResult(*battlefield));

        std::shared_ptr<BattleAction> ba;

        static_assert(EI(BattleSide::ATTACKER) == EI(Export::Side::ATTACKER));
        static_assert(EI(BattleSide::DEFENDER) == EI(Export::Side::DEFENDER));
        info("BAI3");

        while(true) {
            auto _action = getAction(result.get());
            // getAction(result.get());
            // auto _action = recorded.at(i);
            i++;
            info("BAI4");
            allactions.push_back(_action);
            info("BAI5?!?!?!?!?!?!?!?");
            action = std::make_unique<Action>(_action, battlefield.get());
            info("Got action: " + std::to_string(action->action) + " (" + action->name() + ")");
            auto actres = buildAction(*battlefield, *action);

            ba = actres.battleAction;
            result->errmask = actres.errmask;
            attackLogs.clear();

            if (ba) {
                ASSERT(actres.errmask == 0, "unexpected errmask: " + std::to_string(actres.errmask));
                info("Action is VALID: " + action->name());
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

        return Export::Result(
            bf.exportState(),
            bf.exportActMask(),
            dmgDealt,
            dmgReceived,
            unitsLost,
            unitsKilled,
            valueLost,
            valueKilled
        );
    }

    BuildActionResult BAI::buildAction(Battlefield &bf, Action &action) {
        auto res = BuildActionResult();
        auto apos = bf.astack->getPosition();

        // auto origq = std::vector<battle::Units>{};
        // cb->battleGetTurnOrder(origq, QSIZE, 0);
        // auto myq = bf.getQueue(cb.get());

        ASSERT(bf.hexes[Hex::calcId(apos)].stack->attrs[EI(StackAttr::QueuePos)] == 0, "expected 0 queue pos");

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

        auto &bhex = action.hex->bhex;
        auto &estack = action.hex->stack->cstack;

        if (action.hex->hexactmask[EI(action.hexaction)]) {
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
                ASSERT(estack, "no target to shoot");
                res.setAction(BattleAction::makeShotAttack(bf.astack, estack));
            break;
            case HexAction::MELEE_TL:
            case HexAction::MELEE_TR:
            case HexAction::MELEE_R:
            case HexAction::MELEE_BR:
            case HexAction::MELEE_BL:
            case HexAction::MELEE_L: {
                ASSERT(estack, "no target to melee");
                auto &edir = MELEE_TO_EDIR[action.hexaction];
                auto nbh = bhex.cloneInDirection(edir, false); // neighbouring bhex

                if (bf.astack->doubleWide()) {
                    auto &[special, tspecial, bspecial] = EDIR_SPECIALS[side];
                    if (edir == special || edir == tspecial || edir == bspecial)
                        nbh = nbh.cloneInDirection(special, false);
                }

                ASSERT(nbh.isAvailable(), "mask allowed attack from an unavailable hex #" + std::to_string(nbh.hex));

                res.setAction(BattleAction::makeMeleeAttack(bf.astack, bhex, nbh));
            }
            break;
            case HexAction::MELEE_T:
            case HexAction::MELEE_B: {
                ASSERT(bf.astack->doubleWide(), "got T/B action for a single-hex stack");
                auto &[_, tspecial, bspecial] = EDIR_SPECIALS[side];
                auto &edir = (action.hexaction == HexAction::MELEE_T) ? tspecial : bspecial;
                auto nbh = bhex.cloneInDirection(edir, false); // neighbouring bhex
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
            case HexAction::MOVE: {
                auto a = ainfo[action.hex->bhex];
                if (a == EAccessibility::ACCESSIBLE) {
                    ASSERT(action.hex->state == HexState::FREE_UNREACHABLE, "mask prevented move to a reachable bhex" + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_UNREACHABLE);
                } else if (a == EAccessibility::OBSTACLE) {
                    auto hs = action.hex->state;
                    ASSERT(hs == HexState::OBSTACLE, "incorrect hex state -- expected OBSTACLE, got: " + std::to_string(EI(hs)) + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_BLOCKED);
                } else if (a == EAccessibility::ALIVE_STACK) {
                    auto bh = action.hex->bhex;
                    if (bh.hex == bf.astack->getPosition().hex) {
                        // means we want to defend (moving to self)
                        // this should always be allowed
                        ASSERT(false, "mask prevented move to own hex (defend)" + debugInfo(action, bf.astack, nullptr));
                    } else if (bh.hex == bf.astack->occupiedHex().hex) {
                        ASSERT(!ainfo.accessible(bh, true, bf.astack->unitSide()), "mask prevented move to self-occupied hex" + debugInfo(action, bf.astack, nullptr));
                        // means we can't fit on our own back hex
                        res.addError(ErrType::HEX_BLOCKED);
                    }
                    // means we try to move onto another stack
                    res.addError(ErrType::HEX_BLOCKED);
                } else {
                    throw std::runtime_error("Unexpected accessibility: " + std::to_string(EI(a)) + debugInfo(action, bf.astack, nullptr));
                }
            }
            break;
            case HexAction::SHOOT:
                if (!estack)
                    res.addError(ErrType::STACK_NA);
                else if (estack->unitSide() == bf.astack->unitSide())
                    res.addError(ErrType::FRIENDLY_FIRE);
                else {
                    ASSERT(!cb->battleCanShoot(bf.astack, bhex), "mask prevented SHOOT at a shootable bhex " + action.hex->name());
                    res.addError(ErrType::CANNOT_SHOOT);
                }
            break;
            case HexAction::MELEE_TL:
            case HexAction::MELEE_TR:
            case HexAction::MELEE_R:
            case HexAction::MELEE_BR:
            case HexAction::MELEE_BL:
            case HexAction::MELEE_L:
            case HexAction::MELEE_T:
            case HexAction::MELEE_B: {
                if (!estack) {
                    res.addError(ErrType::STACK_NA);
                    break;
                }

                auto frfire = false;

                if (estack->unitSide() == bf.astack->unitSide()) {
                    res.addError(ErrType::FRIENDLY_FIRE);
                    frfire = true;
                }

                auto nbh = BattleHex();

                if (action.hexaction == HexAction::MELEE_T || action.hexaction == HexAction::MELEE_B) {
                    if (!bf.astack->doubleWide()) {
                        res.addError(ErrType::INVALID_DIR);
                        break;
                    }
                    auto &[_, tspecial, bspecial] = EDIR_SPECIALS[side];
                    auto &edir = (action.hexaction == HexAction::MELEE_T) ? tspecial : bspecial;
                    nbh = bhex.cloneInDirection(edir, false); // neighbouring bhex
                } else {
                    auto &edir = MELEE_TO_EDIR[action.hexaction];
                    nbh = bhex.cloneInDirection(edir, false); // neighbouring bhex
                    if (bf.astack->doubleWide()) {
                        auto &[special, tspecial, bspecial] = EDIR_SPECIALS[side];
                        if (edir == special || edir == tspecial || edir == bspecial)
                            nbh = nbh.cloneInDirection(special, false);
                    }
                }

                if (!nbh.isAvailable()) {
                    res.addError(ErrType::HEX_MELEE_NA);
                    break;
                }

                auto &nh = bf.hexes.at(Hex::calcId(nbh));
                auto reachable = rinfo.distances[nbh] <= bf.astack->speed();

                switch (nh.state) {
                case HexState::OBSTACLE:
                    ASSERT(!reachable, "OBSTACLE state for reachable hex " + nh.name() + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_BLOCKED);
                    break;
                case HexState::FREE_UNREACHABLE:
                    ASSERT(!reachable, "FREE_UNREACHABLE state for reachable hex " + nh.name() + debugInfo(action, bf.astack, nullptr));
                    res.addError(ErrType::HEX_UNREACHABLE);
                    break;
                case HexState::OCCUPIED:
                    if (nbh == bf.astack->getPosition()) {
                        ASSERT(frfire, "mask prevented attack from a neighbouring hex " + nh.name() + debugInfo(action, bf.astack, nullptr));
                    } else {
                        res.addError(ErrType::HEX_BLOCKED);
                    }

                    break;
                case HexState::FREE_REACHABLE:
                    // possible only if our "nbh" and "bhex" are somehow not neighbours (vcmi bug in cloneInDirection()?)
                    ASSERT(bf.astack->isMeleeAttackPossible(bf.astack, estack, nbh), "VCMI says melee attack is impossible, but hex is reachable [BUG?]" + debugInfo(action, bf.astack, nullptr));
                    ASSERT(estack->unitSide() == bf.astack->unitSide(), "Mask prevented possible attack from a reachable hex " + nh.name() + debugInfo(action, bf.astack, nullptr));

                    // friendly fire is added early to prevent hiding it if other errors also occurred
                    ASSERT(res.errmask & std::get<0>(Export::ERRORS.at(ErrType::FRIENDLY_FIRE)), "FRIENDLY_FIRE error should already have been added" + debugInfo(action, bf.astack, nullptr));
                    break;
                default:
                    throw std::runtime_error("Unexpected hex state while error checking: " + std::to_string(EI(nh.state)));
                }
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

        info << "ainfo[bhex]=" << EI(ainfo[action.hex->bhex.hex]) << "\n";
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

        auto estack = action.hex->stack->cstack;
        if (estack) {
            info << "estack->getPosition().hex=" << estack->getPosition().hex << "\n";
            info << "estack->slot=" << estack->unitSlot() << "\n";
            info << "estack->doubleWide=" << estack->doubleWide() << "\n";
            info << "cb->battleCanShoot(estack)=" << cb->battleCanShoot(estack) << "\n";
        } else {
            info << "estack: (nullptr)\n";
        }

        info << "astack->getPosition().hex=" << astack->getPosition().hex << "\n";
        info << "astack->slot=" << astack->unitSlot() << "\n";
        info << "astack->doubleWide=" << estack->doubleWide() << "\n";
        info << "cb->battleCanShoot(astack)=" << cb->battleCanShoot(astack) << "\n";

        if (nbh) {
            info << "nbh->hex=" << nbh->hex << "\n";
            info << "ainfo[nbh]=" << EI(ainfo[*nbh]) << "\n";
            info << "rinfo.distances[nbh] <= astack->speed(): " << (rinfo.distances[*nbh] <= astack->speed()) << "\n";

            if (estack)
                info << "astack->isMeleeAttackPossible(...)=" << astack->isMeleeAttackPossible(astack, estack, *nbh) << "\n";
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

            auto al = AttackLog(
                attacker->unitSlot(),
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
        debug("*** actionFinished ***");
        auto shouldupdate = false;

        for (auto &cstack : cb->battleGetAllStacks()) {
            if(cstack && !cstack->alive()) {
                shouldupdate = true;
                break;
            }
        }

        if (shouldupdate)
            battlefield->offTurnUpdate(cb.get());
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
