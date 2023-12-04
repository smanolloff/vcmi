#include "NetPacks.h"
#include "BAI.h"
#include "vstd/CLoggerBase.h"

namespace MMAI {
    // HNS = short for HexNState
    #define HNS(hs) hexStateNMap.at(static_cast<size_t>(HexState::hs))

    using ErrType = Export::ErrType;

    void BAI::error(const std::string &text) const { logAi->error("BAI %s", text); }
    void BAI::warn(const std::string &text) const { logAi->warn("BAI %s", text); }
    void BAI::info(const std::string &text) const { logAi->info("BAI %s", text); }
    void BAI::debug(const std::string &text) const { logAi->debug("BAI %s", text); }

    void BAI::activeStack(const CStack * astack)
    {
        info("*** activeStack ***");
        // print("activeStack called for " + astack->nodeName());

        battlefield = std::make_unique<Battlefield>(cb.get(), astack);
        result = std::make_unique<Export::Result>(buildResult(*battlefield));

        std::shared_ptr<BattleAction> ba;

        while(true) {
            action = std::make_unique<Action>(getAction(result.get()), battlefield.get());
            debug("Got action: " + std::to_string(action->action) + " (" + action->name() + ")");
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

        if (!action.hex) {
            switch(NonHexAction(action.action)) {
            case NonHexAction::RETREAT:
                res.setAction(BattleAction::makeRetreat(cb->battleGetMySide()));
                break;
            case NonHexAction::DEFEND:
                res.setAction(BattleAction::makeDefend(bf.astack));
                break;
            case NonHexAction::WAIT:
                (bf.astack->waitedThisTurn)
                    ? res.addError(ErrType::ALREADY_WAITED)
                    : res.setAction(BattleAction::makeWait(bf.astack));

                break;
            default:
                throw std::runtime_error("Unexpected non-hex action: " + std::to_string(action.action));
            }

            return res;
        }

        auto &bhex = action.hex->bhex;
        auto destself = (bhex.hex == bf.astack->getPosition().hex);
        auto canShoot = cb->battleCanShoot(bf.astack);

        // switch does not allow initializing vars
        const CStack * estack;

        if (action.hex->hexactmask[EI(action.hexaction)]) {
            // Action is VALID
            switch(action.hexaction) {
            case HexAction::MOVE:
                res.setAction(BattleAction::makeMove(bf.astack, bhex));
                break;
            case HexAction::MOVE_AND_ATTACK_0:
            case HexAction::MOVE_AND_ATTACK_1:
            case HexAction::MOVE_AND_ATTACK_2:
            case HexAction::MOVE_AND_ATTACK_3:
            case HexAction::MOVE_AND_ATTACK_4:
            case HexAction::MOVE_AND_ATTACK_5:
            case HexAction::MOVE_AND_ATTACK_6:
                estack = bf.getEnemyStackBySlot(EI(action.hexaction));
                ASSERT(estack, "no stack at slot " + std::to_string(EI(action.hexaction)));

                if (destself && canShoot) {
                    res.setAction(BattleAction::makeShotAttack(bf.astack, estack));
                } else {
                    ASSERT(bf.astack->isMeleeAttackPossible(bf.astack, estack, bhex), "expected to be able to melee");
                    res.setAction(BattleAction::makeMeleeAttack(bf.astack, estack->position, bhex));
                }

                break;
            default:
                throw std::runtime_error("Unexpected hexaction: " + std::to_string(EI(action.hexaction)));
            }

            return res;
        }

        // Action is INVALID

        // switch does not allow initializing vars
        bool canGoThere;
        bool canMelee;

        switch(action.hexaction) {
            case HexAction::MOVE:
                ASSERT(action.hex->state != HexState::FREE_REACHABLE, "mask prevents move to a reachable hex");
                if (destself) res.addError(ErrType::MOVE_SELF);
                (action.hex->state == HexState::FREE_UNREACHABLE)
                    ? res.addError(ErrType::HEX_UNREACHABLE)
                    : res.addError(ErrType::HEX_BLOCKED);
                break;
            case HexAction::MOVE_AND_ATTACK_0:
            case HexAction::MOVE_AND_ATTACK_1:
            case HexAction::MOVE_AND_ATTACK_2:
            case HexAction::MOVE_AND_ATTACK_3:
            case HexAction::MOVE_AND_ATTACK_4:
            case HexAction::MOVE_AND_ATTACK_5:
            case HexAction::MOVE_AND_ATTACK_6:
                estack = bf.getEnemyStackBySlot(EI(action.hexaction));

                if (!estack) {
                    res.addError(ErrType::STACK_NA);
                    break;
                }

                canGoThere = action.hex->hexactmask[EI(HexAction::MOVE)];
                canMelee = bf.astack->isMeleeAttackPossible(bf.astack, estack, bhex);

                if (destself) {
                    ASSERT(!canShoot, "expected to be unable to shoot");
                    ASSERT(!canMelee, "expected impossible melee attack");
                    res.addError(ErrType::ATTACK_IMPOSSIBLE);
                } else if (canGoThere) {
                    if (canShoot) res.addError(ErrType::MOVE_SHOOT);
                    if (!canMelee) res.addError(ErrType::ATTACK_IMPOSSIBLE);
                } else {
                    (action.hex->state == HexState::FREE_UNREACHABLE)
                        ? res.addError(ErrType::HEX_UNREACHABLE)
                        : res.addError(ErrType::HEX_BLOCKED);
                }

                if (!estack->alive()) res.addError(ErrType::STACK_DEAD);
                if (!estack->isValidTarget()) res.addError(ErrType::STACK_INVALID);
                break;
            default:
                throw std::runtime_error("Unexpected hexaction: " + std::to_string(EI(action.hexaction)));
        }

        if (!res.errmask)
            throw std::runtime_error("Failed to identify the errors for action: " + std::to_string(action.action));

        return res;
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

    void BAI::actionFinished(const BattleAction &action) {
        debug("*** actionFinished ***");
        // NOTE: not triggered for retreat
        battlefield->offTurnUpdate(cb.get());
    }

    void BAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed) {
        info("*** battleStart ***");
        side = Side;
    }

    void BAI::battleEnd(const BattleResult *br, QueryID queryID) {
        info("*** battleEnd ***");
        // Export::Result res(std::move(*result), true);
        auto victory = br->winner == cb->battleGetMySide();
        result = std::make_unique<Export::Result>(buildResult(*battlefield), victory);
        ASSERT(result->ended, "expected result->ended to be true");
    }
}
