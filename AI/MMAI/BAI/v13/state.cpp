/*
 * state.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "battle/CPlayerBattleCallback.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v13/encoder.h"
#include "BAI/v13/hexaction.h"
#include "BAI/v13/state.h"
#include "BAI/v13/supplementary_data.h"
#include "schema/v13/constants.h"
#include "schema/v13/types.h"

#include <algorithm>
#include <memory>

namespace MMAI::BAI::V13 {
    using HA = HexAttribute;
    using SA = StackAttribute;

    //
    // Prevent human errors caused by the Stack / Hex attr overlap
    //
    static_assert(EI(HA::STACK_SIDE)                    == EI(SA::SIDE) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_SLOT)                    == EI(SA::SLOT) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_QUANTITY)                == EI(SA::QUANTITY) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_ATTACK)                  == EI(SA::ATTACK) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DEFENSE)                 == EI(SA::DEFENSE) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_SHOTS)                   == EI(SA::SHOTS) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_MIN)                 == EI(SA::DMG_MIN) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_MAX)                 == EI(SA::DMG_MAX) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_HP)                      == EI(SA::HP) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_HP_LEFT)                 == EI(SA::HP_LEFT) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_SPEED)                   == EI(SA::SPEED) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_QUEUE)                   == EI(SA::QUEUE) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_ONE)               == EI(SA::VALUE_ONE) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_FLAGS1)                  == EI(SA::FLAGS1) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_FLAGS2)                  == EI(SA::FLAGS2) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_REL)               == EI(SA::VALUE_REL) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_REL0)              == EI(SA::VALUE_REL0) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_KILLED_REL)        == EI(SA::VALUE_KILLED_REL) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_KILLED_ACC_REL0)   == EI(SA::VALUE_KILLED_ACC_REL0) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_LOST_REL)          == EI(SA::VALUE_LOST_REL) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_VALUE_LOST_ACC_REL0)     == EI(SA::VALUE_LOST_ACC_REL0) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_DEALT_REL)           == EI(SA::DMG_DEALT_REL) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_DEALT_ACC_REL0)      == EI(SA::DMG_DEALT_ACC_REL0) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_RECEIVED_REL)        == EI(SA::DMG_RECEIVED_REL) + STACK_ATTR_OFFSET);
    static_assert(EI(HA::STACK_DMG_RECEIVED_ACC_REL0)   == EI(SA::DMG_RECEIVED_ACC_REL0) + STACK_ATTR_OFFSET);
    static_assert(EI(StackAttribute::_count) == 25, "whistleblower in case attributes change");

    // static
    std::vector<float> State::InitNullStack() {
        auto res = std::vector<float> {};
        for (int i=0; i<EI(StackAttribute::_count); ++i)
            Encoder::Encode(HA(STACK_ATTR_OFFSET + i), NULL_VALUE_UNENCODED, res);
        return res;
    };

    std::tuple<int, int, int, int> CalcGlobalStats(const CPlayerBattleCallback *battle) {
        int lv = 0, lh = 0, rv = 0, rh = 0;
        for (auto &stack : battle->battleGetStacks()) {
            auto v = stack->getCount() * Stack::CalcValue(stack->unitType());
            auto h = stack->getAvailableHealth();
            // std::cout << "[" << EI(stack->unitSide()) << "] v=" << v << ", h=" << h << ", lv=" << lv << ", rv=" << rv << "\n";

            if (stack->unitSide() == BattleSide::ATTACKER) {
                lv += v;
                lh += h;
            } else {
                rv += v;
                rh += h;
            }
        }

        return {lv, lh, rv, rh};
    }

    std::tuple<int, int, int, int, int, int, int, int> ProcessAttackLogs(
        std::vector<std::shared_ptr<AttackLog>> attackLogs,
        std::map<const CStack*, Stack::Stats> sstats
    ) {
        // dmg dealt / dmg received / value killed / value lost
        int ldd = 0, ldr = 0, lvk = 0, lvl = 0;
        int rdd = 0, rdr = 0, rvk = 0, rvl = 0;

        for (auto &[cstack, ss] : sstats) {
            ss.dmgDealtNow = 0;
            ss.dmgReceivedNow = 0;
            ss.valueKilledNow = 0;
            ss.valueLostNow = 0;
        }

        for (auto &al : attackLogs) {
            if (al->cattacker) {
                sstats[al->cattacker].dmgDealtNow += al->dmg;
                sstats[al->cattacker].dmgDealtTotal += al->dmg;
                sstats[al->cattacker].valueKilledNow += al->value;
                sstats[al->cattacker].valueKilledTotal += al->value;

                if (al->cattacker->unitSide() == BattleSide::LEFT_SIDE) {
                    ldd += al->dmg;
                    lvk += al->value;
                } else {
                    rdd += al->dmg;
                    rvk += al->value;
                }
            }

            ASSERT(al->cdefender, "AttackLog cdefender is nullptr!");
            sstats[al->cdefender].dmgReceivedNow += al->dmg;
            sstats[al->cdefender].dmgReceivedTotal += al->dmg;
            sstats[al->cdefender].valueLostNow += al->value;
            sstats[al->cdefender].valueLostTotal += al->value;

            if (al->cdefender->unitSide() == BattleSide::LEFT_SIDE) {
                ldr += al->dmg;
                lvl += al->value;
            } else {
                rdr += al->dmg;
                rvl += al->value;
            }
        }

        return {ldd, ldr, lvk, lvl, rdd, rdr, rvk, rvl};
    }



    State::State(const int version__, const std::string colorname_, const CPlayerBattleCallback* battle_)
    : version_(version__)
    , colorname(colorname_)
    , battle(battle_)
    , side(battle_->battleGetMySide())
    , nullstack(InitNullStack())
    {
        auto [lv, lh, rv, rh] = CalcGlobalStats(battle);
        gstats = std::make_unique<GlobalStats>(battle->battleGetMySide(), lv+rv, lh+rh);
        lpstats = std::make_unique<PlayerStats>(BattleSide::LEFT_SIDE, lv, lh);
        rpstats = std::make_unique<PlayerStats>(BattleSide::RIGHT_SIDE, rv, rh);

        battlefield = Battlefield::Create(battle_, nullptr, gstats.get(), gstats.get(), sstats, false);
        bfstate.reserve(Schema::V13::BATTLEFIELD_STATE_SIZE);
        actmask.reserve(Schema::V13::N_ACTIONS);
    }

    void State::onActiveStack(const CStack* astack, CombatResult result, bool recording, bool fastpath) {
        logAi->debug("onActiveStack: result=%d, recording=%d, fastpath=%d", EI(result), recording, fastpath);
        auto [lv, lh, rv, rh] = CalcGlobalStats(battle);
        auto [ldd, ldr, lvk, lvl, rdd, rdr, rvk, rvl] = ProcessAttackLogs(attackLogs, sstats);
        auto ogstats = *gstats;  // a copy of the "old" gstats

        (result == CombatResult::NONE)
            ? gstats->update(astack->unitSide(), result, lv+rv, lh+rh, !astack->waitedThisTurn)
            : gstats->update(BattleSide::NONE, result, lv+rv, lh+rh, false);
        lpstats->update(&ogstats, lv, lh, ldd, ldr, lvk, lvl);
        rpstats->update(&ogstats, rv, rh, rdd, rdr, rvk, rvl);

        // printf("Fastpath: %d\n", fastpath);
        if (fastpath) {
            // means we are done with onActiveStack, and we can safely clear transitions now
            transitions.clear();
            persistentAttackLogs.clear();
        } else {
            // XXX: uncomment when enabling transitions (1/2)
            // persistentAttackLogs.insert(persistentAttackLogs.end(), attackLogs.begin(), attackLogs.end());
            battlefield = Battlefield::Create(battle, astack, &ogstats, gstats.get(), sstats, isMorale);
            bfstate.clear();
            actmask.clear();

            for (int i=0; i<EI(GlobalAction::_count); i++) {
                switch (GlobalAction(i)) {
                break; case GlobalAction::RETREAT: actmask.push_back(battle->battleCanFlee());
                break; case GlobalAction::WAIT: actmask.push_back(battlefield->astack && !battlefield->astack->cstack->waitedThisTurn);
                break; default:
                    THROW_FORMAT("Unexpected GlobalAction: %d", i);
                }
            }

            encodeGlobal(result);
            encodePlayer(lpstats.get());
            encodePlayer(rpstats.get());

            for (auto &hexrow : *battlefield->hexes)
                for (auto &hex : hexrow)
                    encodeHex(hex.get());

            // Links are not part of the state
            // They are handled separately by the connector
            // for (auto &link : battlefield->links)
            //     encodeLink(link);

            verify();
        }

        isMorale = false;

        supdata = std::make_unique<SupplementaryData>(
            colorname,
            Side(side),
            gstats.get(),
            lpstats.get(),
            rpstats.get(),
            battlefield.get(),
            // XXX: replace with persistentAttackLogs when enabling transitions (2/2)
            attackLogs, // store the logs since OUR last turn
            transitions, // store the states since last turn
            result
        );

        if (recording) {
            ASSERT(startedAction >= 0, "unexpected startedAction: " + std::to_string(startedAction));
            // NOTE: this creates a copy of bfstate (which is what we want)
            transitions.push_back({startedAction, std::make_shared<Schema::ActionMask>(actmask), std::make_shared<Schema::BattlefieldState>(bfstate)});
        } else {
            actingStack = astack; // for fastpath, see onActionStarted
            startedAction = -1;
            // XXX: must NOT clear transitions here (can do it only after BAI's activeStack completes)
            // transitions.clear();
        }

        attackLogs.clear(); // accumulate new logs until next turn
    }


    void State::_onActionStarted(const BattleAction &action) {
        if (!action.isUnitAction()) {
            logAi->warn("Got non-unit action of type: %d", EI(action.actionType));
            return;
        }

        auto stacks = battle->battleGetStacks();

        // Case A: << ENEMY TURN >>
        // 1. StupidAI makes action; vcmi calls ->
        // 2. State::onActionStart() calls ->                           // actingStack is nullptr
        // 3. onActiveStack(recording=true) builds bf and returns to ->
        // 4. State::onActionStart() clears actingStack
        //
        // Case B: << OUR TURN >>
        // 1. BAI::activeStack() calls ->
        // 2. State::onActiveStack(recording=false) builds bf, sets actingStack and returns to ->
        // 3. BAI::activeStack() makes action; vcmi calls ->
        // 4. State::onActionStart() sets fastpath=true and calls ->    //  actingStack already present
        // 5. onActiveStack(recording=true) **skips building bf** and returns to ->
        // 6. State::onActionStart() clears actingStack

        //
        // no need to create battlefield in 5, as it's the same as in 2.
        bool fastpath = false;
        bool found = false;
        for (auto cstack : battle->battleGetAllStacks(true)) {
            if (cstack->unitId() == action.stackNumber) {
                if (actingStack) {
                    // XXX: actingStack is already set here only if it was set in onActiveStack() i.e. on our turn
                    // We could check only the unit's side, but since there are
                    // auto-acting units, comparing the exact unit seems safer.
                    fastpath = true;
                    if (cstack != actingStack) {
                        THROW_FORMAT("actingStack was already set to %s, but does not match the real acting stack %s", actingStack->getDescription() % cstack->getDescription());
                    }
                }

                actingStack = cstack;
                found = true;
                break;
            }
        }
        ASSERT(found, "could not find cstack with unitId: " + std::to_string(action.stackNumber));

        if (actingStack->creatureId() == CreatureID::FIRST_AID_TENT
            || actingStack->creatureId() == CreatureID::CATAPULT
            || actingStack->creatureId() == CreatureID::ARROW_TOWERS
        ) {
            // These are auto-acting for BAI
            // Cannot build state in this case
            return;
        }

        switch (action.actionType) {
        break; case EActionType::WAIT:
            startedAction = ACTION_WAIT;
        break; case EActionType::SHOOT: {
            auto bh = action.target.at(0).hexValue;
            auto id = Hex::CalcId(bh);
            startedAction = N_NONHEX_ACTIONS + id*EI(HexAction::_count) + EI(HexAction::SHOOT);
        }
        break; case EActionType::DEFEND: {
            auto bh = actingStack->getPosition();
            auto id = Hex::CalcId(bh);
            startedAction = N_NONHEX_ACTIONS + id*EI(HexAction::_count) + EI(HexAction::MOVE);
        }
        break; case EActionType::WALK: {
            auto bh = action.target.at(0).hexValue;
            auto id = Hex::CalcId(bh);
            startedAction = N_NONHEX_ACTIONS + id*EI(HexAction::_count) + EI(HexAction::MOVE);
        }
        break; case EActionType::WALK_AND_ATTACK: {
            auto bhMove = action.target.at(0).hexValue;
            auto bhTarget = action.target.at(1).hexValue;
            auto idMove = Hex::CalcId(bhMove);

            // Can't use `battlefield` (old state)
            auto it = std::find_if(stacks.begin(), stacks.end(), [&bhTarget](const CStack* cstack) {
                return cstack->coversPos(bhTarget);
            });

            if (it == stacks.end()) {
                THROW_FORMAT("Could not find stack for target bhex: %d", bhTarget.toInt());
            }

            auto targetStack = *it;

            if (!CStack::isMeleeAttackPossible(actingStack, targetStack, bhMove)) {
                THROW_FORMAT("Melee attack not possible from bh=%d to bh=%d (to %s)", bhMove.toInt() % bhTarget.toInt() % targetStack->getDescription());
            }

            const auto &nbhexes = Hex::NearbyBattleHexes(bhMove);

            for (int i=0; i<nbhexes.size(); ++i) {
                auto &n_bhex = nbhexes.at(i);
                if (n_bhex == bhTarget) {
                    startedAction = N_NONHEX_ACTIONS + idMove*EI(HexAction::_count) + EI(HexAction(i));
                    break;
                }
            }

            ASSERT(startedAction >= 0, "failed to determine startedAction"


            );
        }
        break; case EActionType::MONSTER_SPELL:
            logAi->warn("Got MONSTER_SPELL action (use cursed ground to prevent this)");
            return;
        break; default:
            // Don't record a state diff for the other actions
            // (most are irrelevant or should never occur during training,
            //  except for MONSTER_SPELL, which can be fixed via cursed ground)
            logAi->debug("Not recording actionType=%d", EI(action.actionType));
            return;
        }

        logAi->debug("Recording actionType=%d", EI(action.actionType));
        onActiveStack(actingStack, CombatResult::NONE, true, fastpath);
    }

    void State::encodeGlobal(CombatResult result) {
        for (int i=0; i<EI(GA::_count); ++i) {
            Encoder::Encode(GA(i), gstats->attrs.at(i), bfstate);
        }
    }

    void State::encodePlayer(const PlayerStats* pstats) {
        for (int i=0; i<EI(PA::_count); ++i) {
            Encoder::Encode(PA(i), pstats->attrs.at(i), bfstate);
        }
    }

    void State::encodeHex(const Hex* hex) {
        // Battlefield state
        for (int i=0; i<EI(HA::_count); ++i)
            Encoder::Encode(HA(i), hex->attrs.at(i), bfstate);

        // Action mask
        for (int m=0; m<hex->actmask.size(); ++m)
            actmask.push_back(hex->actmask.test(m));
    }

    void State::verify() {
        ASSERT(bfstate.size() == BATTLEFIELD_STATE_SIZE, "unexpected bfstate.size(): " + std::to_string(bfstate.size()));
        ASSERT(actmask.size() == N_ACTIONS, "unexpected actmask.size(): " + std::to_string(actmask.size()));
    }

    void State::onBattleStacksAttacked(const std::vector<BattleStackAttacked> &bsa) {
        auto stacks = battlefield->stacks;

        for(auto & elem : bsa) {
            auto cdefender = battle->battleGetStackByID(elem.stackAttacked, false);
            auto cattacker = battle->battleGetStackByID(elem.attackerID, false);

            ASSERT(cdefender, "defender cannot be NULL");
            // logAi->debug("Attack: %s -> %s (%d dmg, %d died)", attacker->getName(), defender->getName(), elem.damageAmount, elem.killedAmount);

            auto defender = std::find_if(stacks.begin(), stacks.end(), [&cdefender](std::shared_ptr<Stack> stack) {
                return cdefender == stack->cstack;
            });

            if (defender == stacks.end()) {
                logAi->info("defender cstack '%s' not found in stacks. Maybe it was just summoned/resurrected?", cdefender->getDescription());
            }

            auto attacker = std::find_if(stacks.begin(), stacks.end(), [&cattacker](std::shared_ptr<Stack> stack) {
                return cattacker == stack->cstack;
            });

            auto bf_valueNow = gstats->attr(GA::BFIELD_VALUE_NOW_ABS);
            auto bf_hpNow = gstats->attr(GA::BFIELD_HP_NOW_ABS);
            auto value = elem.killedAmount * Stack::CalcValue(cdefender->unitType());

            // std::cout << "AttackLog";
            // std::cout << ": attacker=" << (cattacker ? cattacker->getDescription() : "null");
            // std::cout << ", defender=" << (cdefender ? cdefender->getDescription() : "null");
            // std::cout << ", dmg=" << (elem.damageAmount);
            // std::cout << ", value=" << (value);
            // std::cout << "\n";

            attackLogs.push_back(std::make_shared<AttackLog>(
                // XXX: attacker can be NULL when an effect does dmg (eg. Acid)
                // XXX: attacker or defender can be NULL if it did not exist
                //      when `stacks` was built (e.g. during our last turn),
                //      but the enemy just summonned/resurrected it
                attacker != stacks.end() ? *attacker : nullptr,
                defender != stacks.end() ? *defender : nullptr,
                cattacker,
                cdefender,
                elem.damageAmount,
                1000 * elem.damageAmount / bf_hpNow,
                elem.killedAmount,
                value,
                1000 * value / bf_valueNow
            ));
        }
    }

    void State::onBattleTriggerEffect(const BattleTriggerEffect &bte) {
        if (static_cast<BonusType>(bte.effect) != BonusType::MORALE)
            return;

        isMorale = true;
    }

    void State::onActionFinished(const BattleAction &action) {
        // XXX: assuming action was OK (no server error about failed/fishy action)
    }

    /*
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * !!!!!! IMPORTANT: `battlefield` must not be used here (old state) !!!!!!
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    // XXX: this is never called (transitions are disabled for performance)
    //      See BAI::actionStarted
    void State::onActionStarted(const BattleAction &action) {
        _onActionStarted(action);
        actingStack = nullptr;
    }

    void State::onBattleEnd(const BattleResult *br) {
        switch(br->winner) {
        break; case BattleSide::LEFT_SIDE:
            onActiveStack(nullptr, CombatResult::LEFT_WINS);
        break; case BattleSide::RIGHT_SIDE:
            onActiveStack(nullptr, CombatResult::RIGHT_WINS);
        break; default:
            onActiveStack(nullptr, CombatResult::DRAW);
        }
    }
};
