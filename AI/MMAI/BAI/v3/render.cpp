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

#include "BAI/v3/render.h"
#include "BAI/v3/hex.h"
#include "Global.h"
#include "battle/AccessibilityInfo.h"
#include "battle/CObstacleInstance.h"
#include "bonuses/Bonus.h"
#include "bonuses/BonusCustomTypes.h"
#include "bonuses/BonusEnum.h"
#include "bonuses/BonusList.h"
#include "bonuses/BonusSelector.h"
#include "constants/EntityIdentifiers.h"
#include "modding/IdentifierStorage.h"
#include "modding/ModScope.h"
#include "schema/v3/constants.h"
#include "schema/v3/types.h"
#include "spells/CSpellHandler.h"
#include "vcmi/spells/Caster.h"
#include <algorithm>
#include <memory>
#include <sys/errno.h>

namespace MMAI::BAI::V3 {
    std::string PadLeft(const std::string& input, size_t desiredLength, char paddingChar) {
        std::ostringstream ss;
        ss << std::right << std::setfill(paddingChar) << std::setw(desiredLength) << input;
        return ss.str();
    }

    std::string PadRight(const std::string& input, size_t desiredLength, char paddingChar) {
        std::ostringstream ss;
        ss << std::left << std::setfill(paddingChar) << std::setw(desiredLength) << input;
        return ss.str();
    }

    void Verify(const State* state) {
        auto battle = state->battle;
        auto hexes = Hexes();

        expect(battle, "no battle to verify");

        const CStack* astack = nullptr; // XXX: can remain nullptr (for terminal obs)
        auto l_CStacks = std::array<const CStack*, 7>{};
        auto r_CStacks = std::array<const CStack*, 7>{};
        auto l_CStacksSummons = std::vector<const CStack*>{};
        auto r_CStacksSummons = std::vector<const CStack*>{};
        auto l_CStacksMachines = std::vector<const CStack*>{};
        auto r_CStacksMachines = std::vector<const CStack*>{};
        auto hexstacks = std::array<const CStack*, 165> {};

        // XXX: special containers for "reserved" unit slots:
        // summoned creatures (-3), war machines (-4), arrow towers (-5)

        auto rinfos = std::map<const CStack*, ReachabilityInfo>{};
        auto allstacks = battle->battleGetStacks();
        std::sort(allstacks.begin(), allstacks.end(), [&](const CStack* a, const CStack* b) {
            return a->unitId() < b->unitId();
        });

        for (auto &cstack : allstacks) {
            if (cstack->unitId() == battle->battleActiveUnit()->unitId())
                astack = cstack;

            if (cstack->unitSlot() < 0) {
                if (cstack->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER)
                    cstack->unitSide()
                        ? r_CStacksSummons.push_back(cstack)
                        : l_CStacksSummons.push_back(cstack);
                else if (cstack->unitSlot() == SlotID::WAR_MACHINES_SLOT)
                    cstack->unitSide()
                        ? r_CStacksMachines.push_back(cstack)
                        : l_CStacksMachines.push_back(cstack);
            } else {
                cstack->unitSide()
                    ? r_CStacks.at(cstack->unitSlot()) = cstack
                    : l_CStacks.at(cstack->unitSlot()) = cstack;
            }

            rinfos.insert({cstack, battle->getReachability(cstack)});

            for (auto &bh : cstack->getHexes()) {
                if (!bh.isAvailable()) continue; // war machines rear hex, arrow towers
                expect(!hexstacks.at(Hex::CalcId(bh)), "hex occupied by multiple stacks?");
                hexstacks.at(Hex::CalcId(bh)) = cstack;
            }
        }

        auto l_CStacksAll = std::vector<const CStack*> {};
        l_CStacksAll.insert(l_CStacksAll.end(), l_CStacks.begin(), l_CStacks.end());
        l_CStacksAll.insert(l_CStacksAll.end(), l_CStacksSummons.begin(), l_CStacksSummons.end());
        l_CStacksAll.insert(l_CStacksAll.end(), l_CStacksMachines.begin(), l_CStacksMachines.end());

        auto r_CStacksAll = std::vector<const CStack*> {};
        r_CStacksAll.insert(r_CStacksAll.end(), r_CStacks.begin(), r_CStacks.end());
        r_CStacksAll.insert(r_CStacksAll.end(), r_CStacksSummons.begin(), r_CStacksSummons.end());
        r_CStacksAll.insert(r_CStacksAll.end(), r_CStacksMachines.begin(), r_CStacksMachines.end());

        auto l_CStacksExtra = std::vector<const CStack*>{};
        l_CStacksExtra.insert(l_CStacksExtra.end(), l_CStacksSummons.begin(), l_CStacksSummons.end());
        l_CStacksExtra.insert(l_CStacksExtra.end(), l_CStacksMachines.begin(), l_CStacksMachines.end());

        auto r_CStacksExtra = std::vector<const CStack*>{};
        r_CStacksExtra.insert(r_CStacksExtra.end(), r_CStacksSummons.begin(), r_CStacksSummons.end());
        r_CStacksExtra.insert(r_CStacksExtra.end(), r_CStacksMachines.begin(), r_CStacksMachines.end());

        auto getAllStacksForSide = [&](bool side) {
            return side ? r_CStacksAll : l_CStacksAll;
        };

        auto ended = state->supdata->ended;

        if (!astack) // draw?
            expect(ended, "astack is NULL, but ended is not true");
        else if (ended) {
            // at battle-end, activeStack is usually the ENEMY stack
            // XXX: this expect will incorrectly throw if we retreated as a regular action
            //      (in which case our stack will be active, but we would have lost the battle)
            // expect(state->supdata->victory == (astack->getOwner() == battle->battleGetMySide()), "state->supdata->victory is %d, but astack->side=%d and myside=%d", state->supdata->victory, astack->getOwner(), battle->battleGetMySide());

            // at battle-end, even regardless of the actual active stack,
            // battlefield->astack must be nullptr
            expect(!state->battlefield->astack, "ended, but battlefield->astack is not NULL");
        }

        // Return (attr == N/A), but after performing some checks
        auto isNA = [](int v, const CStack* stack, const char* attrname) {
            if (v == NULL_VALUE_UNENCODED) {
                expect(!stack, "%s: N/A but stack != nullptr", attrname);
                return true;
            }
            expect(stack, "%s: != N/A but stack = nullptr", attrname);
            return false;
        };

        //
        // XXX: if v=0, returns true when UNreachable
        //      if v=1, returns true when reachable
        //
        auto checkReachable = [=](BattleHex bh, int v, const CStack* stack) {
            auto distance = rinfos.at(stack).distances.at(bh);
            auto canreach = (stack->getMovementRange() >= distance);
            if (v == 0)
                return !canreach;
            else if (v == 1)
                return canreach;
            else
                THROW_FORMAT("Unexpected v: %d", v);
        };

        auto ensureReachability = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            expect(checkReachable(bh, v, stack), "%s: (bhex=%d) reachability expected: %d", attrname, bh.hex, v);
        };

        auto ensureReachabilityOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            ensureReachability(bh, v, stack, attrname);
        };

        // as opposed to ensureHexShootableOrNA, this hexattr works with a mask
        // values are 0 or 1 (not 0..2) and this check requires a valid target
        auto ensureShootability = [=](BattleHex bh, int v, const CStack* cstack, const char* attrname) {
            auto canshoot = battle->battleCanShoot(cstack);
            auto estacks = getAllStacksForSide(!cstack->unitSide());
            auto it = std::find_if(estacks.begin(), estacks.end(), [&bh](auto estack) {
                return estack && estack->coversPos(bh);
            });

            auto haveEstack = (it != estacks.end());

            if (v) {
                expect(canshoot && haveEstack, "%s: =%d but canshoot=%d and haveEstack=%d", attrname, v, canshoot, haveEstack);
            } else {
                expect(!canshoot || !haveEstack, "%s: =%d but canshoot=%d and haveEstack=%d", attrname, v, canshoot, haveEstack);
            }
        };

        auto getHexesForFixedTarget = [rinfos](BattleHex bh, const CStack* stack){
            auto nbhs = std::vector<BattleHex>{
                bh.cloneInDirection(BattleHex::BOTTOM_LEFT, false),
                bh.cloneInDirection(BattleHex::TOP_LEFT, false),
                bh.cloneInDirection(BattleHex::TOP_RIGHT, false),
                bh.cloneInDirection(BattleHex::BOTTOM_RIGHT, false),
            };

            if (stack->doubleWide()) {
                if (stack->unitSide() == BattleSide::ATTACKER) {
                    // attacker's "back" hex is to-the-left
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::TOP_RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::BOTTOM_RIGHT, false));
                } else {
                    // attacker's "back" hex is to-the-right
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::BOTTOM_LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::TOP_LEFT, false));
                }
            } else {
                nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false));
                nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false));
            }

            auto res = std::vector<int>{};

            for (auto &nbh : nbhs)
                if (nbh.isAvailable() && rinfos.at(stack).distances.at(nbh) <= stack->getMovementRange())
                    res.push_back(nbh);

            return res;
        };

        auto ensureValueMatch = [=](int have, int want, const char* attrname) {
            expect(have == want, "%s: have: %d, want: %d", attrname, have, want);
        };

        auto ensureStackValueMatch = [=](StackAttribute a, int have, int want, const char* attrname) {
            auto vmax = std::get<3>(STACK_ENCODING.at(EI(a)));
            if (want > vmax) want = vmax;
            ensureValueMatch(have, want, attrname);
        };

        auto ensureMeleeability = [=](BattleHex bh, HexActMask mask, HexAction ha, const CStack* cstack, const char* attrname) {
            auto mv = mask.test(EI(ha));

            // if AMOVE is allowed, we must be able to reach hex
            // (no else -- we may still be able to reach it)
            if (mv == 1)
                ensureReachability(bh, 1, cstack, attrname);

            auto r_nbh = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
            auto l_nbh = bh.cloneInDirection(BattleHex::EDir::LEFT, false);
            auto nbh = BattleHex{};

            switch (ha) {
            break; case HexAction::AMOVE_TR: nbh = bh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false);
            break; case HexAction::AMOVE_R: nbh = r_nbh;
            break; case HexAction::AMOVE_BR: nbh = bh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false);
            break; case HexAction::AMOVE_BL: nbh = bh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false);
            break; case HexAction::AMOVE_L: nbh = l_nbh;
            break; case HexAction::AMOVE_TL: nbh = bh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false);
            break; case HexAction::AMOVE_2TR: nbh = r_nbh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false);
            break; case HexAction::AMOVE_2R: nbh = r_nbh.cloneInDirection(BattleHex::EDir::RIGHT, false);
            break; case HexAction::AMOVE_2BR: nbh = r_nbh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false);
            break; case HexAction::AMOVE_2BL: nbh = l_nbh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false);
            break; case HexAction::AMOVE_2L: nbh = l_nbh.cloneInDirection(BattleHex::EDir::LEFT, false);
            break; case HexAction::AMOVE_2TL: nbh = l_nbh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false);
            break; default:
                THROW_FORMAT("Unexpected HexAction: %d", EI(ha));
              break;
            }

            auto estacks = getAllStacksForSide(!cstack->unitSide());
            auto it = std::find_if(estacks.begin(), estacks.end(), [&nbh](auto stack) {
                return stack && stack->coversPos(nbh);
            });

            if (mv) {
                expect(it != estacks.end(), "%s: =%d (bhex %d, nbhex %d), but there's no stack on nbhex", attrname, bh.hex, nbh.hex);
                auto estack = *it;
                // must not pass "nbh" for defender position, as it could be its rear hex
                expect(cstack->isMeleeAttackPossible(cstack, estack, bh), "%s: =1 (bhex %d, nbhex %d), but VCMI says isMeleeAttackPossible=0", attrname, bh.hex, nbh.hex);
            }
            //  else {
            //     if (it != estacks.end()) {
            //         auto estack = *it;

            //         // MASK may prohibit attack, but hex may still be reachable
            //         if (checkReachable(bh, 1, cstack))
            //             // MASK may prohibita this specific attack from a reachable hex
            //             // it does not mean any attack is impossible
            //             expect(!cstack->isMeleeAttackPossible(cstack, estack, bh), "%s: =0 (bhex %d, nbhex %d), but bhex is reachable and VCMI says isMeleeAttackPossible=1", attrname, bh.hex, nbh.hex);
            //     }
            // }
        };

        auto ensureCorrectMaskOrNA = [=](BattleHex bh, int v, const CStack* cstack, const char* attrname) {
            if (isNA(v, cstack, attrname)) return;

            auto basename = std::string(attrname);
            auto mask = HexActMask(v);

            ensureReachability(bh, mask.test(EI(HexAction::MOVE)), cstack, (basename + "{MOVE}").c_str());
            ensureShootability(bh, mask.test(EI(HexAction::SHOOT)), cstack, (basename + "{SHOOT}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_TR, cstack, (basename + "{AMOVE_TR}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_R, cstack, (basename + "{AMOVE_R}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_BR, cstack, (basename + "{AMOVE_BR}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_BL, cstack, (basename + "{AMOVE_BL}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_L, cstack, (basename + "{AMOVE_L}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_TL, cstack, (basename + "{AMOVE_TL}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2TR, cstack, (basename + "{AMOVE_2TR}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2R, cstack, (basename + "{AMOVE_2R}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2BR, cstack, (basename + "{AMOVE_2BR}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2BL, cstack, (basename + "{AMOVE_2BL}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2L, cstack, (basename + "{AMOVE_2L}").c_str());
            ensureMeleeability(bh, mask, HexAction::AMOVE_2TL, cstack, (basename + "{AMOVE_2TL}").c_str());
        };

        // ihex = 0, 1, 2, .. 164
        // ibase = 0, 15, 30, ... 2460
        for (int ihex=0; ihex < BF_SIZE; ihex++) {
            int x = ihex%15;
            int y = ihex/15;
            auto &hex = state->battlefield->hexes->at(y).at(x);
            auto bh = hex->bhex;
            expect(bh == BattleHex(x+1, y), "hex->bhex mismatch");

            auto ainfo = battle->getAccesibility();
            auto aa = ainfo.at(bh);

            for (int i=0; i < EI(HexAttribute::_count); i++) {
                auto attr = HexAttribute(i);
                auto v = hex->attrs.at(i);
                auto cstack = hexstacks.at(ihex);

                switch(attr) {
                break; case HA::Y_COORD:
                    expect(v == y, "HEX.Y_COORD: %d != %d", v, y);
                break; case HA::X_COORD:
                    expect(v == x, "HEX.X_COORD: %d != %d", v, x);
                break; case HA::STATE_MASK: {
                    auto obstacles = battle->battleGetAllObstaclesOnPos(bh, false);
                    auto anyobstacle = [&obstacles](std::function<bool(const CObstacleInstance*)> fn) {
                        return std::any_of(obstacles.begin(), obstacles.end(), [&fn](std::shared_ptr<const CObstacleInstance> obstacle) {
                            return fn(obstacle.get());
                        });
                    };

                    auto mask = HexStateMask(v);
                    bool side = astack->unitSide();


                    if (mask.test(EI(HexState::PASSABLE))) {
                        expect(
                            aa == EAccessibility::ACCESSIBLE || (side && aa == EAccessibility::GATE),
                            "HEX.STATE_MASK: PASSABLE bit is set, but accessibility is %d (side: %d)", EI(aa), side
                        );
                    } else {
                        expect(
                            aa == EAccessibility::OBSTACLE || aa == EAccessibility::ALIVE_STACK,
                            "HEX.STATE_MASK: PASSABLE bit not set, but accessibility is %d", EI(aa)
                        );
                    }

                    if (mask.test(EI(HexState::STOPPING))) {
                        // auto stopping = std::any_of(obstacles.begin(), obstacles.end(), [](auto obstacle) {
                        //     return obstacle->stopsMovement();
                        // });
                        auto stopping = anyobstacle(std::mem_fn(&CObstacleInstance::stopsMovement));
                        expect(stopping, "HEX.STATE_MASK: STOPPING bit is set, but no obstacle stops movement");
                    }

                    if (mask.test(EI(HexState::DAMAGING))) {
                        auto damaging = anyobstacle([](const CObstacleInstance* o) {
                            return o->triggersEffects() && o->getTrigger().toSpell()->isDamage();
                        });

                        expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
                    }

                    // if (mask.test(EI(HexState::GATE))) {
                    //     expect(battle->battleGetSiegeLevel(), "HEX.STATE_MASK: GATE bit is set, but there is no fort");
                    //     expect(bh == BattleHex::GATE_INNER || bh == BattleHex::GATE_OUTER, "HEX.STATE_MASK: GATE bit is set on bhex#%d", bh.hex);
                    // }
                }
                break; case HA::ACTION_MASK: {
                    if (ended) {
                        expect(v == NULL_VALUE_UNENCODED, "HEX.ACTION_MASK: battle ended, but action mask is %d", v);
                    } else if(v == NULL_VALUE_UNENCODED) {
                        // NULL action mask happens if there are too many stacks
                        // and the active stack is one of the ignored stacks
                        // XXX: cstack may be null here (this may be a blank hex)
                        auto stacks = battle->battleGetMySide() ? r_CStacksAll : l_CStacksAll;
                        expect(stacks.size() > MAX_STACKS_PER_SIDE, "HEX.ACTION_MASK: is null and hex->stack is null, but n_stacks is %d", stacks.size());
                    } else {
                        ensureCorrectMaskOrNA(bh, v, astack, "HEX.ACTION_MASK");
                    }
                }
                break; case HA::STACK_ID: {
                    // XXX: isNA may fail for ignored stacks (4th+ extra stacks)
                    //      If cstack is nullptr => v MUST be -1
                    //      If cstack != nullptr => v MAY be -1 (for ignored stacks)
                    if (cstack == nullptr) {
                        expect(v == NULL_VALUE_UNENCODED, "HEX.STACK_ID: v=%d, but cstack is nullptr", v);
                        break;
                    }

                    auto side = cstack->unitSide();
                    auto slot = cstack->unitSlot();

                    if (slot >= 0) {
                        // regular stack -- STACK_ID is 0..6 (or 10..16 for R side)
                        auto id = side ? slot + MAX_STACKS_PER_SIDE : slot;
                        expect(v == id, "HEX.STACK_ID: =%d, but cstack->unitSlot()=%d and side=%d", v, slot, side);
                        break;
                    }

                    // special (extra) stack (summoned creature or war machine)
                    expect(
                        slot == SlotID::WAR_MACHINES_SLOT || slot == SlotID::SUMMONED_SLOT_PLACEHOLDER,
                        "unexpected unitSlot: %d", slot
                    );

                    auto extras = side ? r_CStacksExtra : l_CStacksExtra;
                    auto it = std::find(extras.begin(), extras.end(), cstack);
                    if (it == extras.end()) {
                        THROW_FORMAT("HEX.STACK_ID: cstack is an extra stack (unitSlot()=%d), but did not find it among the extra stacks", slot);
                    }

                    //  A special stack has STACK_ID of either:
                    //  a) 7..9 (or 17..19 for R side)
                    //  b) 0..6 (or 10..16 for R side)
                    //  c) -1 (too many other stacks in the army)
                    //
                    // Case a) is for when cstack is within the first 3 "extra" stacks.
                    // Case b) is for when cstack is 4th or later "extra" stack,
                    //          but there are free IDs 0..6 which it could use.
                    // Case c) is same as b), but no free IDs 0..6 -- stack is ignored.
                    //

                    // First see if the extra stack a "designated" extra STACK_ID
                    // Check a)
                    int extraSeqNumber = std::distance(extras.begin(), it); // 1-based
                    if (extraSeqNumber < MAX_STACKS_PER_SIDE - 7) {
                        // stack is within the first 3 "extra" stacks
                        // => it will be assigned STACK_ID of 7..9 (or 17..19 for R side)
                        int idForSide = 7 + extraSeqNumber;
                        int globalID = side ? 10 + idForSide : idForSide;
                        expect(v == globalID, "HEX.STACK_ID: v=%d, but expected %d (extraSeqNumber=%d)", v, globalID, extraSeqNumber);
                        break;
                    }

                    // The extra stack is at least 4th or later in line
                    // => Check b)
                    auto regulars = cstack->unitSide() ? r_CStacks : l_CStacks;
                    auto regularIds = std::vector<int> {};
                    for (auto s : regulars) if (s) regularIds.push_back(s->unitSlot());

                    auto nFreeRegularIds = 7 - regularIds.size();
                    // 4th extra stack would take first free regular id, 5th - the second, etc.
                    auto nFreeRegularIdsNeeded = (extraSeqNumber+1) - (MAX_STACKS_PER_SIDE - 7);
                    if (nFreeRegularIds >= nFreeRegularIdsNeeded) {
                        // the extra stack was assigned a regular ID
                        expect(v % MAX_STACKS_PER_SIDE < 7,
                            "HEX.STACK_ID: v=%d, but expected v%%%d to be <7 (extraSeqNumber=%d, nFreeRegularIds=%d)",
                            v, MAX_STACKS_PER_SIDE, extraSeqNumber, nFreeRegularIds
                        );

                        auto idForSide = v % MAX_STACKS_PER_SIDE;
                        expect(idForSide < regularIds.size(),
                            "HEX.STACK_ID: v=%d, i.e. idForSide=%d (a regular id), but there are only %d stacks with regular ids.",
                            v, idForSide, regularIds.size()
                        );

                        // the ID our extra stack uses must NOT be used by a regular stack
                        auto cstackAtID = regulars.at(idForSide);
                        expect(cstackAtID == nullptr,
                            "HEX.STACK_ID: v=%d, i.e. idForSide=%d, but there's a stack at this regular id",
                            v, idForSide
                        );

                        break;
                    }

                    // There are no free regular ids to use
                    // => Check c)
                    expect(v == NULL_VALUE_UNENCODED,
                        "HEX.STACK_ID: v=%d, but expected NULL (extraSeqNumber=%d, nFreeRegularIds=%d)",
                        v, extraSeqNumber, nFreeRegularIds
                    );
                }
                break; default:
                    THROW_FORMAT("Unexpected HexAttribute: %d", EI(attr));
                }
            }
        }

        for (int side : {0, 1}) {
            for (int istack=0; istack < MAX_STACKS_PER_SIDE; istack++) {
                auto stack = state->battlefield->stacks->at(side).at(istack);

                if (!stack)
                    continue;

                const CStack *cstack = stack->cstack;

                expect(cstack, "cstack is nullptr, but stack is present");
                expect(stack->cstack, "stack->cstack is nullptr, but cstack is present");

                auto [x, y] = Hex::CalcXY(cstack->getPosition());
                auto bonuses = cstack->getAllBonuses(Selector::all, nullptr);

                // See docs/modders/Bonus/Bonus_Types.md
                auto spell_after_attack_bonuses = BonusList();
                bonuses->getBonuses(spell_after_attack_bonuses, Selector::type()(BonusType::SPELL_AFTER_ATTACK), nullptr);

                auto castchance = [&spell_after_attack_bonuses](std::vector<SpellID> spellids) {
                    // TODO: what about BLOCK_MAGIC_ABOVE / BLOCK_ALL_MAGIC
                    //       Isn't the chance always 0 then?
                    int res = 0;

                    auto sel = CSelector(Selector::subtype()(spellids.at(0)));
                    for (auto it = spellids.begin()+1; it != spellids.end(); ++it)
                        sel = sel.Or(Selector::subtype()(*it));

                    auto selected = BonusList();
                    spell_after_attack_bonuses.getBonuses(selected, sel);
                    for (auto &bonus : selected)
                        res += bonus->val;
                    return res;
                };

                // XXX: alternative (simpler?) implementation
                // TODO: compare performance
                // auto castchance = [&spell_after_attack_bonuses](std::vector<SpellIDBase::Type> spellids) {
                //     int res = 0;
                //     for (auto &bonus : spell_after_attack_bonuses) {
                //         auto st = bonus->subtype;
                //         for (auto sid : spellids) {
                //             if (st == sid) res += bonus->val;
                //         }
                //     }
                //     return res;
                // };

                // See docs/modders/Game_Identifiers.md
                auto hate_bonuses = BonusList();
                bonuses->getBonuses(hate_bonuses, Selector::type()(BonusType::HATE), nullptr);
                auto hatevalue = [&hate_bonuses](std::vector<std::string> creaturenames) {
                    int res = 0;
                    for (auto &bonus : hate_bonuses) {
                        auto st = bonus->subtype;
                        for (auto name : creaturenames) {
                            auto cid = VLC->identifiers()->getIdentifier(ModScope::scopeGame(), "creature", name, false);
                            if (cid)
                                if (st == CreatureID(cid.value())) res += bonus->val;
                        }
                    }
                    return res;
                };

                auto find_bonus = [&bonuses](BonusType t) {
                    return bonuses->getFirst(Selector::type()(t));
                };

                auto find_bonus2 = [&bonuses](BonusType t1, BonusSubtypeID t2) {
                    return bonuses->getFirst(Selector::typeSubtype(t1, t2));
                };

                auto has_bonus = [&find_bonus](BonusType t) {
                    return !!find_bonus(t);
                };

                auto has_bonus2 = [&find_bonus2](BonusType t1, BonusSubtypeID t2) {
                    return !!find_bonus2(t1, t2);
                };

                auto get_bonus_duration = [&find_bonus](BonusType t) {
                    auto b = find_bonus(t);
                    if (!b)
                        return 0;

                    if ((b->duration & BonusDuration::PERMANENT).any())
                        return 100;
                    if ((b->duration & BonusDuration::N_TURNS).any())
                        return static_cast<int>(b->turnsRemain);

                    return 0;
                };


                // Now validate all attributes...
                for (int i=0; i < EI(SA::_count); i++) {
                    auto a = SA(i);
                    auto v = stack->attr(a);
                    auto [_, e, n, vmax] = STACK_ENCODING.at(i);

                    int want = -666;

                    switch(a) {
                    break; case SA::ID:
                    break; case SA::Y_COORD:
                        expect(v == y, "STACK.Y_COORD: %d != %d", v, y);
                    break; case SA::X_COORD:
                        expect(v == x, "STACK.X_COORD: %d != %d", v, x);
                    break; case SA::SIDE:
                        ensureStackValueMatch(a, v, cstack->unitSide(), "STACK.SIDE");
                    break; case SA::QUANTITY:
                        ensureStackValueMatch(a, v, std::min(cstack->getCount(), vmax), "STACK.QUANTITY");
                    break; case SA::ATTACK:
                        ensureStackValueMatch(a, v, cstack->getAttack(false), "STACK.ATTACK");
                    break; case SA::DEFENSE:
                        ensureStackValueMatch(a, v, cstack->getDefense(false), "STACK.DEFENSE");
                    break; case SA::SHOTS:
                        ensureStackValueMatch(a, v, cstack->shots.available(), "STACK.SHOTS");
                    break; case SA::DMG_MIN:
                        ensureStackValueMatch(a, v, cstack->getMinDamage(false), "STACK.DMG_MIN");
                    break; case SA::DMG_MAX:
                        ensureStackValueMatch(a, v, cstack->getMaxDamage(false), "STACK.DMG_MAX");
                    break; case SA::HP:
                        ensureStackValueMatch(a, v, cstack->getMaxHealth(), "STACK.HP");
                    break; case SA::HP_LEFT:
                        ensureStackValueMatch(a, v, cstack->getFirstHPleft(), "STACK.HP_LEFT");
                    break; case SA::SPEED:
                        ensureStackValueMatch(a, v, cstack->getMovementRange(), "STACK.SPEED");
                    break; case SA::WAITED:
                        ensureStackValueMatch(a, v, cstack->waitedThisTurn, "STACK.WAITED");
                    break; case SA::QUEUE_POS:
                        // at battle end, queue is messed up
                        // (the stack that dealt the killing blow is still "active", but not on 0 pos)
                        if (ended)
                            break;

                        if (v == 0)
                            expect(cstack == astack, "STACK.QUEUE_POS: =0 but is different from astack");
                        else
                            expect(cstack != astack, "STACK.QUEUE_POS: =%d but is same as astack", v);
                    break; case SA::RETALIATIONS_LEFT:
                        // not verifying unlimited retals, just check 0
                        if (v == 0)
                            expect(!cstack->ableToRetaliate(), "STACK.RETALIATIONS_LEFT: =0 but ableToRetaliate=true");
                        else
                            expect(cstack->ableToRetaliate(), "STACK.RETALIATIONS_LEFT: =%d but ableToRetaliate=false", v);
                    break; case SA::MAGIC_RESISTANCE:
                        // XXX: being near a unicorn does NOT not give MAGIC_RESISTSNCE
                        //      bonus -- cstack->magicResistance() checks neighbouring
                        //      hexes manually and adds the percentage, but that is not
                        //      done when building the state (too expensive operation)
                        // ensureStackValueMatch(a, v, cstack->magicResistance(), "STACK.MAGIC_RESISTANCE");
                        want = cstack->valOfBonuses(BonusType::MAGIC_RESISTANCE);
                        ensureStackValueMatch(a, v, want, "STACK.MAGIC_RESISTANCE");
                    break; case SA::IS_WIDE:
                        ensureStackValueMatch(a, v, cstack->doubleWide(), "STACK.IS_WIDE");
                    break; case SA::AI_VALUE:
                        ensureStackValueMatch(a, v, cstack->unitType()->getAIValue(), "STACK.AI_VALUE");
                    break; case SA::MORALE:
                        ensureStackValueMatch(a, v, cstack->moraleVal(), "STACK.MORALE");
                    break; case SA::LUCK:
                        ensureStackValueMatch(a, v, cstack->luckVal(), "STACK.LUCK");
                    break; case SA::BLIND_LIKE_ATTACK:
                        want = castchance({SpellID::BLIND, SpellID::STONE_GAZE, SpellID::PARALYZE});
                        ensureStackValueMatch(a, v, want, "STACK.BLIND_LIKE_ATTACK");
                    break; case SA::WEAKENING_ATTACK:
                        want = castchance({SpellID::WEAKNESS, SpellID::DISEASE});
                        ensureStackValueMatch(a, v, want, "STACK.WEAKENING_ATTACK");
                    break; case SA::DISPELLING_ATTACK:
                        want = castchance({SpellID::DISPEL, SpellID::DISPEL_HELPFUL_SPELLS});
                        ensureStackValueMatch(a, v, want, "STACK.DISPELLING_ATTACK");
                    break; case SA::POISONOUS_ATTACK:
                        want = castchance({SpellID::POISON});
                        ensureStackValueMatch(a, v, want, "STACK.POISONOUS_ATTACK");
                    break; case SA::CURSING_ATTACK:
                        want = castchance({SpellID::CURSE});
                        ensureStackValueMatch(a, v, want, "STACK.CURSING_ATTACK");
                    break; case SA::AGING_ATTACK:
                        want = castchance({SpellID::AGE});
                        ensureStackValueMatch(a, v, want, "STACK.AGING_ATTACK");
                    break; case SA::ACID_ATTACK: {
                        auto b = find_bonus(BonusType::ACID_BREATH);
                        want = b ? b->additionalInfo[0] : 0;
                        ensureStackValueMatch(a, v, want, "STACK.ACID_ATTACK");
                    }
                    break; case SA::BINDING_ATTACK:
                        want = castchance({SpellID::BIND});
                        ensureStackValueMatch(a, v, want, "STACK.BINDING_ATTACK");
                    break; case SA::LIGHTNING_ATTACK:
                        want = castchance({SpellID::THUNDERBOLT});
                        ensureStackValueMatch(a, v, want, "STACK.LIGHTNING_ATTACK");
                    break; case SA::AREA_ATTACK:
                        if (has_bonus2(BonusType::SPELL_LIKE_ATTACK, BonusCustomSubtype(SpellID::LIGHTNING_BOLT)))
                            want = 2;
                        if (has_bonus2(BonusType::SPELL_LIKE_ATTACK, BonusCustomSubtype(SpellID::FIREBALL)))
                            want = 1;
                        else
                            want = 0;
                        ensureStackValueMatch(a, v, want, "STACK.AREA_ATTACK");
                    break; case SA::HATES_ANGELS:
                        want = hatevalue({"angel"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_ANGELS");
                    break; case SA::HATES_DEVILS:
                        want = hatevalue({"devil"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_DEVILS");
                    break; case SA::HATES_TITANS:
                        want = hatevalue({"titan"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_TITANS");
                    break; case SA::HATES_BLACK_DRAGONS:
                        want = hatevalue({"blackDragon"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_BLACK_DRAGONS");
                    break; case SA::HATES_GENIES:
                        want = hatevalue({"genie"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_GENIES");
                    break; case SA::HATES_EFREET:
                        want = hatevalue({"efreet"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_EFREET");
                    break; case SA::HATES_AIR_ELEMENTALS:
                        want = hatevalue({"airElemental"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_AIR_ELEMENTALS");
                    break; case SA::HATES_EARTH_ELEMENTALS:
                        want = hatevalue({"earthElemental"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_EARTH_ELEMENTALS");
                    break; case SA::HATES_WATER_ELEMENTALS:
                        want = hatevalue({"waterElemental"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_WATER_ELEMENTALS");
                    break; case SA::HATES_FIRE_ELEMENTALS:
                        want = hatevalue({"fireElemental"});
                        ensureStackValueMatch(a, v, want, "STACK.HATES_FIRE_ELEMENTALS");
                    break; case SA::FREE_SHOOTING:
                        want = has_bonus(BonusType::FREE_SHOOTING);
                        ensureStackValueMatch(a, v, want, "STACK.FREE_SHOOTING");
                    break; case SA::FLYING:
                        want = has_bonus(BonusType::FLYING);
                        ensureStackValueMatch(a, v, want, "STACK.FLYING");
                    break; case SA::SHOOTER:
                        want = has_bonus(BonusType::SHOOTER);
                        ensureStackValueMatch(a, v, want, "STACK.SHOOTER");
                        if (v > 0)
                            expect(cstack->shots.canUse(), "STACK.SHOOTER: v=%d, but stack->shots.canUse() is false", v);
                    break; case SA::ADDITIONAL_ATTACK:
                        want = has_bonus(BonusType::ADDITIONAL_ATTACK);
                        ensureStackValueMatch(a, v, want, "STACK.ADDITIONAL_ATTACK");
                    break; case SA::NO_MELEE_PENALTY:
                        want = has_bonus(BonusType::NO_MELEE_PENALTY);
                        ensureStackValueMatch(a, v, want, "STACK.NO_MELEE_PENALTY");
                    break; case SA::JOUSTING:
                        want = has_bonus(BonusType::JOUSTING);
                        ensureStackValueMatch(a, v, want, "STACK.JOUSTING");
                    break; case SA::SPELL_RESISTANCE_AURA:
                        want = cstack->valOfBonuses(BonusType::SPELL_RESISTANCE_AURA);
                        ensureStackValueMatch(a, v, want, "STACK.SPELL_RESISTANCE_AURA");
                    break; case SA::LEVEL_SPELL_IMMUNITY:
                        want = cstack->valOfBonuses(BonusType::LEVEL_SPELL_IMMUNITY);
                        ensureStackValueMatch(a, v, want, "STACK.LEVEL_SPELL_IMMUNITY");
                    break; case SA::FIRE_DAMAGE_REDUCTION:
                        want = 0;
                        if (has_bonus2(BonusType::SPELL_DAMAGE_REDUCTION, BonusSubtypeID(SpellSchool::FIRE)))
                            want += 100;
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::FIRE);
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::ANY);
                        ensureStackValueMatch(a, v, want, "STACK.FIRE_DAMAGE_REDUCTION");
                    break; case SA::WATER_DAMAGE_REDUCTION:
                        want = 0;
                        if (has_bonus2(BonusType::SPELL_DAMAGE_REDUCTION, BonusSubtypeID(SpellSchool::WATER)))
                            want += 100;
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::WATER);
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::ANY);
                        ensureStackValueMatch(a, v, want, "STACK.WATER_DAMAGE_REDUCTION");
                    break; case SA::AIR_DAMAGE_REDUCTION:
                        want = 0;
                        if (has_bonus2(BonusType::SPELL_DAMAGE_REDUCTION, BonusSubtypeID(SpellSchool::AIR)))
                            want += 100;
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::AIR);
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::ANY);
                        ensureStackValueMatch(a, v, want, "STACK.AIR_DAMAGE_REDUCTION");
                    break; case SA::EARTH_DAMAGE_REDUCTION:
                        want = 0;
                        if (has_bonus2(BonusType::SPELL_DAMAGE_REDUCTION, BonusSubtypeID(SpellSchool::EARTH)))
                            want += 100;
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::EARTH);
                        want += cstack->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, SpellSchool::ANY);
                        ensureStackValueMatch(a, v, want, "STACK.EARTH_DAMAGE_REDUCTION");
                    break; case SA::TWO_HEX_ATTACK_BREATH:
                        want = has_bonus(BonusType::TWO_HEX_ATTACK_BREATH);
                        ensureStackValueMatch(a, v, want, "STACK.TWO_HEX_ATTACK_BREATH");
                    break; case SA::NO_WALL_PENALTY:
                        want = has_bonus(BonusType::NO_WALL_PENALTY);
                        ensureStackValueMatch(a, v, want, "STACK.NO_WALL_PENALTY");
                    break; case SA::NON_LIVING:
                        want = has_bonus(BonusType::UNDEAD) \
                                || has_bonus(BonusType::NON_LIVING)
                                || has_bonus(BonusType::SIEGE_WEAPON);
                        ensureStackValueMatch(a, v, want, "STACK.NON_LIVING");
                    break; case SA::BLOCKS_RETALIATION:
                        want = has_bonus(BonusType::BLOCKS_RETALIATION);
                        ensureStackValueMatch(a, v, want, "STACK.BLOCKS_RETALIATION");
                    break; case SA::THREE_HEADED_ATTACK:
                        want = has_bonus(BonusType::THREE_HEADED_ATTACK);
                        ensureStackValueMatch(a, v, want, "STACK.THREE_HEADED_ATTACK");
                    break; case SA::MIND_IMMUNITY:
                        want = has_bonus(BonusType::MIND_IMMUNITY)
                                || has_bonus(BonusType::UNDEAD)
                                || has_bonus(BonusType::NON_LIVING)
                                || has_bonus(BonusType::SIEGE_WEAPON);
                        ensureStackValueMatch(a, v, want, "STACK.MIND_IMMUNITY");
                    break; case SA::FIRE_SHIELD:
                        want = has_bonus(BonusType::FIRE_SHIELD);
                        ensureStackValueMatch(a, v, want, "STACK.FIRE_SHIELD");
                    break; case SA::LIFE_DRAIN:
                        want = cstack->valOfBonuses(BonusType::LIFE_DRAIN);
                        ensureStackValueMatch(a, v, want, "STACK.LIFE_DRAIN");
                    break; case SA::DOUBLE_DAMAGE_CHANCE:
                        want = cstack->valOfBonuses(BonusType::DOUBLE_DAMAGE_CHANCE);
                        ensureStackValueMatch(a, v, want, "STACK.DOUBLE_DAMAGE_CHANCE");
                    break; case SA::RETURN_AFTER_STRIKE:
                        want = has_bonus(BonusType::RETURN_AFTER_STRIKE);
                        ensureStackValueMatch(a, v, want, "STACK.RETURN_AFTER_STRIKE");
                    break; case SA::DEFENSIVE_STANCE:
                        want = has_bonus(BonusType::DEFENSIVE_STANCE);
                        ensureStackValueMatch(a, v, want, "STACK.DEFENSIVE_STANCE");
                    break; case SA::ATTACKS_ALL_ADJACENT:
                        want = has_bonus(BonusType::ATTACKS_ALL_ADJACENT);
                        ensureStackValueMatch(a, v, want, "STACK.ATTACKS_ALL_ADJACENT");
                    break; case SA::NO_DISTANCE_PENALTY:
                        want = has_bonus(BonusType::NO_DISTANCE_PENALTY);
                        ensureStackValueMatch(a, v, want, "STACK.NO_DISTANCE_PENALTY");
                    break; case SA::HYPNOTIZED:
                        want = get_bonus_duration(BonusType::HYPNOTIZED);
                        ensureStackValueMatch(a, v, want, "STACK.HYPNOTIZED");
                    break; case SA::MAGIC_MIRROR:
                        want = cstack->valOfBonuses(BonusType::MAGIC_MIRROR);
                        ensureStackValueMatch(a, v, want, "STACK.MAGIC_MIRROR");
                    break; case SA::ATTACKS_NEAREST_CREATURE:
                        want = has_bonus(BonusType::ATTACKS_NEAREST_CREATURE);
                        ensureStackValueMatch(a, v, want, "STACK.ATTACKS_NEAREST_CREATURE");
                    break; case SA::SLEEPING:
                        want = cstack->unitType()->getId() == CreatureID::AMMO_CART
                            ? 0 : get_bonus_duration(BonusType::NOT_ACTIVE);

                        ensureStackValueMatch(a, v, want, "STACK.SLEEPING");
                    break; case SA::DEATH_STARE:
                        want = has_bonus(BonusType::DEATH_STARE);
                        ensureStackValueMatch(a, v, want, "STACK.DEATH_STARE");
                    break; case SA::POISON:
                        want = has_bonus(BonusType::POISON);
                        ensureStackValueMatch(a, v, want, "STACK.POISON");
                    break; case SA::REBIRTH:
                        want = has_bonus(BonusType::REBIRTH);
                        ensureStackValueMatch(a, v, want, "STACK.REBIRTH");
                    break; case SA::ENEMY_DEFENCE_REDUCTION:
                        want = cstack->valOfBonuses(BonusType::ENEMY_DEFENCE_REDUCTION);
                        ensureStackValueMatch(a, v, want, "STACK.ENEMY_DEFENCE_REDUCTION");
                    break; case SA::MELEE_DAMAGE_REDUCTION:
                        want = cstack->valOfBonuses(BonusType::GENERAL_DAMAGE_REDUCTION, BonusCustomSubtype::damageTypeAll);
                        want += cstack->valOfBonuses(BonusType::GENERAL_DAMAGE_REDUCTION, BonusCustomSubtype::damageTypeMelee);
                        ensureStackValueMatch(a, v, want, "STACK.MELEE_DAMAGE_REDUCTION");
                    break; case SA::RANGED_DAMAGE_REDUCTION:
                        want = cstack->valOfBonuses(BonusType::GENERAL_DAMAGE_REDUCTION, BonusCustomSubtype::damageTypeAll);
                        want += cstack->valOfBonuses(BonusType::GENERAL_DAMAGE_REDUCTION, BonusCustomSubtype::damageTypeRanged);
                        ensureStackValueMatch(a, v, want, "STACK.RANGED_DAMAGE_REDUCTION");
                    break; case SA::MELEE_ATTACK_REDUCTION:
                        want = cstack->valOfBonuses(BonusType::GENERAL_ATTACK_REDUCTION, BonusCustomSubtype::damageTypeAll);
                        want += cstack->valOfBonuses(BonusType::GENERAL_ATTACK_REDUCTION, BonusCustomSubtype::damageTypeMelee);
                        ensureStackValueMatch(a, v, want, "STACK.MELEE_ATTACK_REDUCTION");
                    break; case SA::RANGED_ATTACK_REDUCTION:
                        want = cstack->valOfBonuses(BonusType::GENERAL_ATTACK_REDUCTION, BonusCustomSubtype::damageTypeAll);
                        want += cstack->valOfBonuses(BonusType::GENERAL_ATTACK_REDUCTION, BonusCustomSubtype::damageTypeRanged);
                        ensureStackValueMatch(a, v, want, "STACK.RANGED_ATTACK_REDUCTION");
                    break; default:
                        THROW_FORMAT("Unexpected StackAttribute: %d", EI(a));
                    }
                }
            }
        }

        // TODO: validate global state
    }

    // This intentionally uses the IState interface to ensure that
    // the schema is properly exposing all needed informaton
    std::string Render(const Schema::IState* istate, const Action *action) {
        auto bfstate = istate->getBattlefieldState();
        auto supdata_ = istate->getSupplementaryData();
        expect(supdata_.has_value(), "supdata_ holds no value");
        expect(supdata_.type() == typeid(const ISupplementaryData*), "supdata_ of unexpected type");
        auto supdata = std::any_cast<const ISupplementaryData*>(supdata_);
        expect(supdata, "supdata holds a nullptr");
        auto hexes = supdata->getHexes();
        auto allstacks = supdata->getStacks();
        auto color = supdata->getColor();

        IStack* astack;
        auto idstacks = std::array<IStack*, MAX_STACKS>{};

        // find an active hex (i.e. with active stack on it)
        for (auto &sidestacks : allstacks) {
            for (auto &stack : sidestacks) {
                if (stack) {
                    idstacks.at(stack->getAttr(SA::ID)) = stack;
                    if (stack->getAttr(SA::QUEUE_POS) == 0) {
                        expect(!astack, "two active stacks found");
                        astack = stack;
                    }
                }
            }
        }

        if (!astack)
            logAi->warn("could not find an active stack. Are there more than %d stacks in this army?", MAX_STACKS_PER_SIDE);

        std::string nocol = "\033[0m";
        std::string redcol = "\033[31m"; // red
        std::string bluecol = "\033[34m"; // blue
        std::string darkcol = "\033[90m";
        std::string activemod = "\033[107m\033[7m"; // bold+reversed

        std::vector<std::stringstream> lines;

        //
        // 1. Add logs table:
        //
        // #1 attacks #5 for 16 dmg (1 killed)
        // #5 attacks #1 for 4 dmg (0 killed)
        // ...
        //
        for (auto &alog : supdata->getAttackLogs()) {
            auto row = std::stringstream();
            auto attcol = redcol;
            auto defcol = bluecol;

            if (alog->getDefender()->getAttr(SA::SIDE) == 0) {
                attcol = bluecol;
                defcol = redcol;
            }

            if (alog->getAttacker())
                row << attcol << "#" << alog->getAttacker()->getAlias() << nocol;
            else
                row << "\033[7m" << "FX" << nocol;

            row << " attacks ";
            row << defcol << "#" << alog->getDefender()->getAlias() << nocol;
            row << " for " << alog->getDamageDealt() << " dmg";
            row << " (kills: " << alog->getUnitsKilled() << ", value: " << alog->getValueKilled() << ")";

            lines.push_back(std::move(row));
        }

        //
        // 2. Build ASCII table
        //    (+populate aliveStacks var)
        //    NOTE: the contents below look mis-aligned in some editors.
        //          In (my) terminal, it all looks correct though.
        //
        //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
        //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃
        // ¹┨  1 ◌ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 1 ┠¹
        // ²┨ ◌ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠²
        // ³┨  ◌ ○ ○ ○ ○ ○ ○ ◌ ▦ ▦ ◌ ◌ ◌ ◌ ◌ ┠³
        // ⁴┨ ◌ ○ ○ ○ ○ ○ ○ ○ ▦ ▦ ▦ ◌ ◌ ◌ ◌  ┠⁴
        // ⁵┨  2 ◌ ○ ○ ▦ ▦ ◌ ○ ◌ ◌ ◌ ◌ ◌ ◌ 2 ┠⁵
        // ⁶┨ ◌ ○ ○ ○ ▦ ▦ ◌ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁶
        // ⁷┨  3 3 ○ ○ ○ ▦ ◌ ○ ○ ◌ ◌ ▦ ◌ ◌ 3 ┠⁷
        // ⁸┨ ◌ ○ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌  ┠⁸
        // ⁹┨  ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠⁹
        // ⁰┨ ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁰
        // ¹┨  4 ◌ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 4 ┠¹
        //  ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃
        //   ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕
        //

        // s=special; can be any number, slot is always 7 (SPECIAL), visualized A,B,C...

        static_assert(MAX_STACKS == 20, "code assumes 20 stacks");

        auto tablestartrow = lines.size();

        lines.emplace_back() << "  ▕₀▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕";
        lines.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

        bool addspace = true;

        auto seenstacks = std::bitset<MAX_STACKS>(0);

        for (int y=0; y < BF_YMAX; y++) {
            for (int x=0; x < BF_XMAX; x++) {
                auto sym = std::string("?");
                auto &hex = hexes.at(y).at(x);

                IStack* stack = nullptr;

                if (hex->getAttr(HA::STACK_ID) != NULL_VALUE_UNENCODED) {
                    stack = idstacks.at(hex->getAttr(HA::STACK_ID));
                    expect(stack, "stack with ID=%d not found", hex->getAttr(HA::STACK_ID));
                }

                auto &row = (x == 0)
                    ? (lines.emplace_back() << nummap.at(y%10) << "┨" << (y % 2 == 0 ? " " : ""))
                    : lines.back();

                if (addspace)
                    row << " ";

                addspace = true;

                auto smask = HexStateMask(hex->getAttr(HA::STATE_MASK));
                auto col = nocol;

                // First put symbols based on hex state.
                // If there's a stack on this hex, symbol will be overriden.
                HexStateMask mpass = 1<<EI(HexState::PASSABLE);
                HexStateMask mstop = 1<<EI(HexState::STOPPING);
                HexStateMask mdmg = 1<<EI(HexState::DAMAGING);
                HexStateMask mdefault = 0; // or mother :)

                std::map<std::string, HexStateMask> symbols {
                    {"○", mpass},
                    {"△", mpass|mstop},
                    {"✶", mpass|mdmg},
                    {"⨻", mpass|mstop|mdmg},
                    {"▦",  mdefault}
                };

                for (auto &pair : symbols) {
                    auto &[s, m] = pair;
                    if ((smask & m) == m) {
                        sym = s;
                        break;
                    }
                }

                auto amask = HexActMask(hex->getAttr(HA::ACTION_MASK));
                if (amask.test(EI(HexAction::MOVE)) <= 0) // || supdata->getIsBattleEnded()
                    col = darkcol;

                if (hex->getAttr(HA::STACK_ID) >= 0) {
                    auto strslot = std::string(1, stack->getAlias());
                    auto seen = seenstacks.test(stack->getAttr(SA::ID));
                    auto col = stack->getAttr(SA::SIDE) ? bluecol : redcol;

                    if (stack->getAttr(SA::QUEUE_POS) == 0) // && !supdata->getIsBattleEnded()
                        col += activemod;

                    if (stack->getAttr(SA::IS_WIDE) > 0 && !seen) {
                        if (stack->getAttr(SA::SIDE) == 0) {
                            strslot += "↠";
                            addspace = false;
                        } else if(stack->getAttr(SA::SIDE) == 1 && hex->getAttr(HA::X_COORD) < 14) {
                            strslot += "↞";
                            addspace = false;
                        }
                    }

                    sym = col + strslot + nocol;
                }

                row << sym;

                if (x == BF_XMAX-1) {
                    row << (y % 2 == 0 ? " " : "  ") << "┠" << nummap.at(y % 10);
                }
            }
        }

        lines.emplace_back() << " ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃";
        lines.emplace_back() << "  ▕⁰▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕";

        //
        // 3. Add side table stuff
        //
        //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
        //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃         Player: RED
        // ₁┨  ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠₁    Last action:
        // ₂┨ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠₂      DMG dealt: 0
        // ₃┨  1 ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌ ◌ ◌ 1 ┠₃   Units killed: 0
        // ...

        for (int i=0; i<=lines.size(); i++) {
            std::string name;
            std::string value;
            auto side = EI(supdata->getSide());

            switch(i) {
            case 1:
                name = "Player";
                if (supdata->getIsBattleEnded())
                    value = "";
                else
                    value = side ? bluecol + "BLUE" + nocol : redcol + "RED" + nocol;
                break;
            case 2:
                name = "Last action";
                value = action ? action->name() + " [" + std::to_string(action->action) + "]" : "";
                break;
            case 3: name = "DMG dealt"; value = std::to_string(supdata->getDmgDealt()); break;
            case 4: name = "Units killed"; value = std::to_string(supdata->getUnitsKilled()); break;
            case 5: name = "Value killed"; value = std::to_string(supdata->getValueKilled()); break;
            case 6: name = "DMG received"; value = std::to_string(supdata->getDmgReceived()); break;
            case 7: name = "Units lost"; value = std::to_string(supdata->getUnitsLost()); break;
            case 8: name = "Value lost"; value = std::to_string(supdata->getValueLost()); break;
            case 9: {
                // XXX: if there's a draw, this text will be incorrect
                auto restext = supdata->getIsVictorious()
                    ? (side ? bluecol + "BLUE WINS" : redcol + "RED WINS")
                    : (side ? redcol + "RED WINS" : bluecol + "BLUE WINS" );

                name = "Battle result"; value = supdata->getIsBattleEnded() ? (restext + nocol) : "";
                break;
            }
            // case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
            default:
                continue;
            }

            lines.at(tablestartrow + i) << PadLeft(name, 15, ' ') << ": " << value;
        }

        lines.emplace_back() << "";

        //
        // 5. Add stacks table:
        //
        //          Stack # |   0   1   2   3   4   5   6   A   B   C   0   1   2   3   4   5   6   A   B   C
        // -----------------+--------------------------------------------------------------------------------
        //              Qty |   0  34   0   0   0   0   0   0   0   0   0  17   0   0   0   0   0   0   0   0
        //           Attack |   0   8   0   0   0   0   0   0   0   0   0   6   0   0   0   0   0   0   0   0
        //    ...10 more... | ...
        // -----------------+--------------------------------------------------------------------------------
        //
        // table with 24 columns (1 header, 3 dividers, 10 stacks per side)
        // Each row represents a separate attribute

        using RowDef = std::tuple<StackAttribute, std::string>;

        // All cell text is aligned right
        auto colwidths = std::array<int, 4 + MAX_STACKS>{};
        colwidths.fill(4); // default col width
        colwidths.at(0) = 10; // header col

        // Divider column indexes
        auto divcolids = {1, MAX_STACKS_PER_SIDE+2, MAX_STACKS+3};

        for (int i : divcolids)
            colwidths.at(i) = 2; // divider col

        // {Attribute, name, colwidth}
        const auto rowdefs = std::vector<RowDef> {
            RowDef{SA::ID, "Stack #"},
            RowDef{SA::X_COORD, ""},  // divider row
            RowDef{SA::QUANTITY, "Qty"},
            RowDef{SA::ATTACK, "Attack"},
            RowDef{SA::DEFENSE, "Defense"},
            RowDef{SA::SHOTS, "Shots"},
            RowDef{SA::DMG_MIN, "Dmg (min)"},
            RowDef{SA::DMG_MAX, "Dmg (max)"},
            RowDef{SA::HP, "HP"},
            RowDef{SA::HP_LEFT, "HP left"},
            RowDef{SA::SPEED, "Speed"},
            RowDef{SA::WAITED, "Waited"},
            RowDef{SA::QUEUE_POS, "Queue"},
            RowDef{SA::RETALIATIONS_LEFT, "Ret. left"},
            RowDef{SA::X_COORD, ""},  // divider row
        };

        // Table with nrows and ncells, each cell a 3-element tuple
        // cell: color, width, txt
        using TableCell = std::tuple<std::string, int, std::string>;
        using TableRow = std::array<TableCell, colwidths.size()>;

        auto table = std::vector<TableRow> {};

        auto divrow = TableRow{};
        for (int i=0; i<colwidths.size(); i++)
            divrow[i] = {nocol, colwidths.at(i), std::string(colwidths.at(i), '-')};

        for (int i : divcolids)
            divrow.at(i) = {nocol, colwidths.at(i), std::string(colwidths.at(i)-1, '-') + "+"};

        // Attribute rows
        for (auto& [a, aname] : rowdefs) {
            if (a == SA::X_COORD) { // divider row
                table.push_back(divrow);
                continue;
            }

            auto row = TableRow{};

            // Header col
            row.at(0) = {nocol, colwidths.at(0), aname};

            // Div cols
            for (int i : {1, 2+MAX_STACKS_PER_SIDE, int(colwidths.size()-1)})
                row.at(i) = {nocol, colwidths.at(i), "|"};

            // Stack cols
            for (auto side : {0, 1}) {
                auto &sidestacks = allstacks.at(side);
                for (int i=0; i<sidestacks.size(); ++i) {
                    auto stack = sidestacks.at(i);
                    auto color = nocol;
                    std::string value = "";

                    if (stack) {
                        color = stack->getAttr(SA::SIDE) ? bluecol : redcol;
                        value = a == SA::ID
                            ? std::string(1, stack->getAlias())
                            : std::to_string(stack->getAttr(a));
                        if (stack->getAttr(SA::QUEUE_POS) == 0 && !supdata->getIsBattleEnded()) color += activemod;
                    }

                    auto colid = 2 + i + side + (MAX_STACKS_PER_SIDE*side);
                    row.at(colid) = {color, colwidths.at(colid), value};
                }
            }

            table.push_back(row);
        }

        for (auto &r : table) {
            auto line = std::stringstream();
            for (auto& [color, width, txt] : r)
                line << color << PadLeft(txt, width, ' ') << nocol;

            lines.push_back(std::move(line));
        }

        //
        // 7. Join rows into a single string
        //
        std::string res = lines[0].str();
        for (int i=1; i<lines.size(); i++)
            res += "\n" + lines[i].str();

        return res;
    }
}
