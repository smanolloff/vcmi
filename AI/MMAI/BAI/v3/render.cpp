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
#include "schema/v3/constants.h"
#include "schema/v3/types.h"
#include <algorithm>

namespace MMAI::BAI::V3 {
    using A = HexAttribute;

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

    // TODO: fix for terminal observation
    //       info after off-turn update is somewhat inconsistent
    //       (investigation needed)
    void Verify(const State* state) {
        auto battle = state->battle;
        auto hexes = Hexes();

        expect(battle, "no battle nothing to verify");

        const CStack* astack; // XXX: can remain nullptr (for terminal obs)
        auto l_CStacks = std::array<const CStack*, 7>{};
        auto r_CStacks = std::array<const CStack*, 7>{};

        // XXX: special containers for "reserved" unit slots:
        // summoned creatures (-3), war machines (-4), arrow towers (-5)

        auto rinfos = std::map<const CStack*, ReachabilityInfo>{};
        auto l_CStacksSpecial = std::vector<const CStack*>{};
        auto r_CStacksSpecial = std::vector<const CStack*>{};

        for (auto &cstack : battle->battleGetStacks()) {
            if (cstack->unitId() == battle->battleActiveUnit()->unitId())
                astack = cstack;

            if (cstack->unitSlot() < 0) {
                cstack->unitSide()
                    ? r_CStacksSpecial.push_back(cstack)
                    : l_CStacksSpecial.push_back(cstack);
            } else {
                cstack->unitSide()
                    ? r_CStacks.at(cstack->unitSlot()) = cstack
                    : l_CStacks.at(cstack->unitSlot()) = cstack;
            }

            rinfos.insert({cstack, battle->getReachability(cstack)});
        }

        bool isSpecialAStack = (astack && astack->unitSlot() < 0);

        auto l_CStacksAll = std::vector<const CStack*> {};
        l_CStacksAll.insert(l_CStacksAll.end(), l_CStacks.begin(), l_CStacks.end());
        l_CStacksAll.insert(l_CStacksAll.end(), l_CStacksSpecial.begin(), l_CStacksSpecial.end());

        auto r_CStacksAll = std::vector<const CStack*> {};
        r_CStacksAll.insert(r_CStacksAll.end(), r_CStacks.begin(), r_CStacks.end());
        r_CStacksAll.insert(r_CStacksAll.end(), r_CStacksSpecial.begin(), r_CStacksSpecial.end());

        auto getAllStacksForSide = [&](bool side) {
            return side ? r_CStacksAll : l_CStacksAll;
        };

        auto ended = state->supdata->ended;

        if (!astack) // draw?
            expect(ended, "astack is NULL, but ended is not true");
        if (ended) {
            // at battle-end, activeStack is usually the ENEMY stack
            expect(state->supdata->victory == (astack->getOwner() == battle->battleGetMySide()), "state->supdata->victory is %d, but astack->side=%d and myside=%d", state->supdata->victory, astack->getOwner(), battle->battleGetMySide());
            expect(!state->battlefield->astack, "ended, but battlefield->astack is not NULL");
        }

        // Return (attr == N/A), but after performing some checks
        auto isNA = [](int v, const CStack* stack, const char* attrname) {
            if (v == BATTLEFIELD_STATE_VALUE_NA) {
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

        auto ensureMeleeableOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto bhs = getHexesForFixedTarget(bh, stack);
            if (v == 0)
                expect(bhs.size() == 0, "%s: =0 but there are %d neighbouring hexes to attack from", attrname, bhs.size());
            else
                expect(bhs.size() > 0, "%s: =%d but there are no neighbouring hexes to attack from", attrname, v);
        };


        auto ensureHexShootableOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto canshoot = battle->battleCanShoot(stack);
            auto distance = bh.getDistance(bh, stack->getPosition());
            auto norangepen = stack->hasBonusOfType(BonusType::NO_DISTANCE_PENALTY);

            if (v == 0) {
                static_assert(ShootDistance(0) == ShootDistance::NA);
                expect(!canshoot, "%s: =0 but the stack is able to shoot", attrname);
            } else if (v == 1) {
                static_assert(ShootDistance(1) == ShootDistance::FAR);
                expect(canshoot, "%s: =1, but the stack is unable to shoot", attrname);
                expect(distance > 10, "%s: =1, but distance=%d (<=10)", attrname, distance);
            } else if (v == 2) {
                static_assert(ShootDistance(2) == ShootDistance::NEAR);
                expect(canshoot, "%s: =2, but the stack is unable to shoot", attrname);
                expect(norangepen || distance <= 10, "%s: =2, but norangepen=0 and distance=%d (>10)", attrname, distance, norangepen);
            } else {
                THROW_FORMAT("Unexpected v: %d", v);
            }
        };

        auto ensureValueMatch = [=](int v, int vreal, const char* attrname) {
            expect(v == vreal, "%s: =%d, but is %d", attrname, v, vreal);
        };

        auto ensureMeleePossibility = [=](BattleHex bh, HexActMask mask, HexAction ha, const CStack* cstack, const char* attrname) {
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
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_TR, cstack, (basename + "{AMOVE_TR}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_R, cstack, (basename + "{AMOVE_R}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_BR, cstack, (basename + "{AMOVE_BR}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_BL, cstack, (basename + "{AMOVE_BL}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_L, cstack, (basename + "{AMOVE_L}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_TL, cstack, (basename + "{AMOVE_TL}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2TR, cstack, (basename + "{AMOVE_2TR}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2R, cstack, (basename + "{AMOVE_2R}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2BR, cstack, (basename + "{AMOVE_2BR}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2BL, cstack, (basename + "{AMOVE_2BL}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2L, cstack, (basename + "{AMOVE_2L}").c_str());
            ensureMeleePossibility(bh, mask, HexAction::AMOVE_2TL, cstack, (basename + "{AMOVE_2TL}").c_str());
        };

        auto ensureCorresponding_R_L_attr = [=](A activeattr, int v, HexAttrs &attrs, const CStack* astack, std::string attrname) {
            // Check meleeability is the same for the corresponding FRIENDLY_STACK_ attribute

            // no stack is active at battle end
            if (ended) {
                expect(v == -1, "%s: =%d but there is no active stack", attrname.c_str(), v);
                return;
            }

            expect(astack->unitSlot() >= 0, "passing special stacks to ensureCorresponding_R_L_attr(%s) is an error", attrname.c_str());

            // Static checks ensuring attributes are defined in the expected order
            static_assert(EI(A::HEX_ACTION_MASK_FOR_L_STACK_0) == EI(A::HEX_ACTION_MASK_FOR_ACT_STACK) + 1);
            static_assert(EI(A::HEX_ACTION_MASK_FOR_R_STACK_0) == EI(A::HEX_ACTION_MASK_FOR_ACT_STACK) + 8);
            static_assert(EI(A::HEX_MELEE_MODIFIER_FOR_L_STACK_0) == EI(A::HEX_MELEE_MODIFIER_FOR_ACT_STACK) + 1);
            static_assert(EI(A::HEX_MELEE_MODIFIER_FOR_R_STACK_0) == EI(A::HEX_MELEE_MODIFIER_FOR_ACT_STACK) + 8);
            static_assert(EI(A::HEX_RANGED_MODIFIER_FOR_L_STACK_0) == EI(A::HEX_RANGED_MODIFIER_FOR_ACT_STACK) + 1);
            static_assert(EI(A::HEX_RANGED_MODIFIER_FOR_R_STACK_0) == EI(A::HEX_RANGED_MODIFIER_FOR_ACT_STACK) + 8);

            // (e.g. if HEX_MELEE_MODIFIER_FOR_R_STACK_0 is index )
            auto baseattr = A(EI(activeattr) + (astack->unitSide() ? 8 : 1));

            expect(
                v == attrs.at(EI(baseattr) + astack->unitSlot()),
                "%s: =%d but different corresponding R/L v=%d",
                attrname.c_str(), v, attrs.at(EI(baseattr) + astack->unitSlot())
            );

        };

        // ihex = 0, 1, 2, .. 164
        // ibase = 0, 15, 30, ... 2460
        for (int ihex=0; ihex < BF_SIZE; ihex++) {
            int x = ihex%15;
            int y = ihex/15;
            auto &hex = state->battlefield->hexes.at(y).at(x);
            auto bh = hex->bhex;

            expect(bh == BattleHex(x+1, y), "hex->bhex mismatch");

            const CStack *cstack = nullptr;
            bool isActive = false;
            bool isRight = false;
            bool isSpecialStack = false;

            if (hex->attr(A::STACK_QUANTITY) != BATTLEFIELD_STATE_VALUE_NA) {
                // there's a stack on this hex
                isRight = hex->attr(A::STACK_SIDE);

                if (hex->attr(A::STACK_SLOT) == STACK_SPECIAL_SLOT) {
                    auto vec = isRight ? r_CStacksSpecial : l_CStacksSpecial;
                    auto it = std::find_if(vec.begin(), vec.end(), [&hex](auto cstack) {
                        return cstack->coversPos(hex->bhex);
                    });
                    expect(it != vec.end(), "could not find special stack");
                    cstack = *it;
                    isSpecialStack = true;
                } else {
                    cstack = isRight ? r_CStacks.at(hex->attr(A::STACK_SLOT)) : l_CStacks.at(hex->attr(A::STACK_SLOT));
                }

                hex->cstack = cstack;
                isActive = (cstack == astack);
                expect(cstack, "could not find cstack");

                if (cstack->doubleWide())
                    expect(cstack->coversPos(bh), "Hex's stack info about side+slot is incorrect (two-hex stack)");
                else
                    expect(cstack->getPosition() == bh, "Hex's stack info about side+slot is incorrect");
            }

            // auto rinfo = battle->getReachability(cstack);
            auto ainfo = battle->getAccesibility();
            auto aa = ainfo.at(bh);

            // Now validate all attributes...
            for (int i=0; i < EI(A::_count); i++) {
                auto attr = A(i);
                auto v = hex->attrs.at(i);

                switch(attr) {
                break; case A::PERCENT_CUR_TO_START_TOTAL_VALUE:
                    // expect(v == y, "HEX_Y_COORD: %d != %d", v, y);
                break; case A::HEX_Y_COORD:
                    expect(v == y, "HEX_Y_COORD: %d != %d", v, y);
                break; case A::HEX_X_COORD:
                    expect(v == x, "HEX_X_COORD: %d != %d", v, x);
                break; case A::HEX_STATE:
                    switch (HexState(v)) {
                    break; case HexState::OBSTACLE:
                        expect(aa == EAccessibility::OBSTACLE, "HEX_STATE: OBSTACLE -> %d", aa);
                    break; case HexState::STACK_FRONT:
                           case HexState::STACK_BACK:
                        expect(aa == EAccessibility::ALIVE_STACK, "HEX_STATE: OCCUPIED -> %d", aa);
                    break; case HexState::FREE:
                        expect(aa == EAccessibility::ACCESSIBLE, "HEX_STATE: FREE -> %d", aa);
                    break; default:
                        THROW_FORMAT("HEX_STATE: Unexpected HexState: %d", v);
                    }
                break; case A::HEX_ACTION_MASK_FOR_ACT_STACK:
                    isSpecialAStack
                        ? ensureCorrectMaskOrNA(bh, v, astack, "HEX_ACTION_MASK_FOR_ACT_STACK")
                        : ensureCorresponding_R_L_attr(attr, v, hex->attrs, astack, "HEX_ACTION_MASK_FOR_ACT_STACK");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_0:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(0), "HEX_ACTION_MASK_FOR_R_STACK_0");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_1:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(1), "HEX_ACTION_MASK_FOR_R_STACK_1");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_2:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(2), "HEX_ACTION_MASK_FOR_R_STACK_2");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_3:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(3), "HEX_ACTION_MASK_FOR_R_STACK_3");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_4:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(4), "HEX_ACTION_MASK_FOR_R_STACK_4");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_5:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(5), "HEX_ACTION_MASK_FOR_R_STACK_5");
                break; case A::HEX_ACTION_MASK_FOR_R_STACK_6:
                    ensureCorrectMaskOrNA(bh, v, r_CStacks.at(6), "HEX_ACTION_MASK_FOR_R_STACK_6");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_0:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(0), "HEX_ACTION_MASK_FOR_L_STACK_0");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_1:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(1), "HEX_ACTION_MASK_FOR_L_STACK_1");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_2:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(2), "HEX_ACTION_MASK_FOR_L_STACK_2");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_3:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(3), "HEX_ACTION_MASK_FOR_L_STACK_3");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_4:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(4), "HEX_ACTION_MASK_FOR_L_STACK_4");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_5:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(5), "HEX_ACTION_MASK_FOR_L_STACK_5");
                break; case A::HEX_ACTION_MASK_FOR_L_STACK_6:
                    ensureCorrectMaskOrNA(bh, v, l_CStacks.at(6), "HEX_ACTION_MASK_FOR_L_STACK_6");
                break; case A::HEX_MELEE_MODIFIER_FOR_ACT_STACK:
                    isSpecialAStack
                        ? ensureMeleeableOrNA(hex->bhex, v, astack, "HEX_MELEE_MODIFIER_FOR_ACT_STACK")
                        : ensureCorresponding_R_L_attr(attr, v, hex->attrs, astack, "HEX_MELEE_MODIFIER_FOR_ACT_STACK");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_0:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(0), "HEX_MELEE_MODIFIER_FOR_L_STACK_0");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_1:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(1), "HEX_MELEE_MODIFIER_FOR_L_STACK_1");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_2:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(2), "HEX_MELEE_MODIFIER_FOR_L_STACK_2");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_3:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(3), "HEX_MELEE_MODIFIER_FOR_L_STACK_3");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_4:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(4), "HEX_MELEE_MODIFIER_FOR_L_STACK_4");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_5:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(5), "HEX_MELEE_MODIFIER_FOR_L_STACK_5");
                break; case A::HEX_MELEE_MODIFIER_FOR_L_STACK_6:
                    ensureMeleeableOrNA(hex->bhex, v, l_CStacks.at(6), "HEX_MELEE_MODIFIER_FOR_L_STACK_6");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_0:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(0), "HEX_MELEE_MODIFIER_FOR_R_STACK_0");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_1:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(1), "HEX_MELEE_MODIFIER_FOR_R_STACK_1");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_2:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(2), "HEX_MELEE_MODIFIER_FOR_R_STACK_2");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_3:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(3), "HEX_MELEE_MODIFIER_FOR_R_STACK_3");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_4:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(4), "HEX_MELEE_MODIFIER_FOR_R_STACK_4");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_5:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(5), "HEX_MELEE_MODIFIER_FOR_R_STACK_5");
                break; case A::HEX_MELEE_MODIFIER_FOR_R_STACK_6:
                    ensureMeleeableOrNA(hex->bhex, v, r_CStacks.at(6), "HEX_MELEE_MODIFIER_FOR_R_STACK_6");
                break; case A::HEX_RANGED_MODIFIER_FOR_ACT_STACK:
                    isSpecialAStack
                        ? ensureHexShootableOrNA(hex->bhex, v, astack, "HEX_RANGED_MODIFIER_FOR_ACT_STACK")
                        : ensureCorresponding_R_L_attr(attr, v, hex->attrs, astack, "HEX_RANGED_MODIFIER_FOR_ACT_STACK");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_0:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(0), "HEX_RANGED_MODIFIER_FOR_L_STACK_0");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_1:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(1), "HEX_RANGED_MODIFIER_FOR_L_STACK_1");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_2:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(2), "HEX_RANGED_MODIFIER_FOR_L_STACK_2");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_3:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(3), "HEX_RANGED_MODIFIER_FOR_L_STACK_3");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_4:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(4), "HEX_RANGED_MODIFIER_FOR_L_STACK_4");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_5:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(5), "HEX_RANGED_MODIFIER_FOR_L_STACK_5");
                break; case A::HEX_RANGED_MODIFIER_FOR_L_STACK_6:
                    ensureHexShootableOrNA(hex->bhex, v, l_CStacks.at(6), "HEX_RANGED_MODIFIER_FOR_L_STACK_6");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_0:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(0), "HEX_RANGED_MODIFIER_FOR_R_STACK_0");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_1:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(1), "HEX_RANGED_MODIFIER_FOR_R_STACK_1");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_2:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(2), "HEX_RANGED_MODIFIER_FOR_R_STACK_2");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_3:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(3), "HEX_RANGED_MODIFIER_FOR_R_STACK_3");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_4:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(4), "HEX_RANGED_MODIFIER_FOR_R_STACK_4");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_5:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(5), "HEX_RANGED_MODIFIER_FOR_R_STACK_5");
                break; case A::HEX_RANGED_MODIFIER_FOR_R_STACK_6:
                    ensureHexShootableOrNA(hex->bhex, v, r_CStacks.at(6), "HEX_RANGED_MODIFIER_FOR_R_STACK_6");
                break; case A::STACK_QUANTITY:
                    // need separate N/A check (cstack may be nullptr)
                    if (isNA(v, cstack, "STACK_QUANTITY")) break;
                    ensureValueMatch(v, std::min(cstack->getCount(), 1023), "STACK_QUANTITY");
                break; case A::STACK_ATTACK:
                    if (isNA(v, cstack, "STACK_ATTACK")) break;
                    ensureValueMatch(v, cstack->getAttack(false), "STACK_ATTACK");
                break; case A::STACK_DEFENSE:
                    if (isNA(v, cstack, "STACK_DEFENSE")) break;
                    ensureValueMatch(v, cstack->getDefense(false), "STACK_DEFENSE");
                break; case A::STACK_SHOTS:
                    if (isNA(v, cstack, "STACK_SHOTS")) break;
                    ensureValueMatch(v, cstack->shots.available(), "STACK_SHOTS");
                break; case A::STACK_DMG_MIN:
                    if (isNA(v, cstack, "STACK_DMG_MIN")) break;
                    ensureValueMatch(v, cstack->getMinDamage(false), "STACK_DMG_MIN");
                break; case A::STACK_DMG_MAX:
                    if (isNA(v, cstack, "STACK_DMG_MAX")) break;
                    ensureValueMatch(v, cstack->getMaxDamage(false), "STACK_DMG_MAX");
                break; case A::STACK_HP:
                    if (isNA(v, cstack, "STACK_HP")) break;
                    ensureValueMatch(v, cstack->getMaxHealth(), "STACK_HP");
                break; case A::STACK_HP_LEFT:
                    if (isNA(v, cstack, "STACK_HP_LEFT")) break;
                    ensureValueMatch(v, cstack->getFirstHPleft(), "STACK_HP_LEFT");
                break; case A::STACK_SPEED:
                    if (isNA(v, cstack, "STACK_SPEED")) break;
                    ensureValueMatch(v, cstack->getMovementRange(), "STACK_SPEED");
                break; case A::STACK_WAITED:
                    if (isNA(v, cstack, "STACK_WAITED")) break;
                    ensureValueMatch(v, cstack->waitedThisTurn, "STACK_WAITED");
                break; case A::STACK_QUEUE_POS:
                    if (isNA(v, cstack, "STACK_QUEUE_POS")) break;
                    if (v == 0)
                        expect(isActive, "STACK_QUEUE_POS: =0 but isActive=false");
                    else
                        expect(!isActive, "STACK_QUEUE_POS: !=0 but isActive=true");
                break; case A::STACK_RETALIATIONS_LEFT:
                    if (isNA(v, cstack, "STACK_RETALIATIONS_LEFT")) break;
                    // not verifying unlimited retals, just check 0
                    if (v == 0)
                        expect(!cstack->ableToRetaliate(), "STACK_RETALIATIONS_LEFT: =0 but ableToRetaliate=true");
                    else
                        expect(cstack->ableToRetaliate(), "STACK_RETALIATIONS_LEFT: >0 but ableToRetaliate=false");
                break; case A::STACK_SIDE:
                    if (isNA(v, cstack, "STACK_SIDE")) break;
                    ensureValueMatch(v, cstack->unitSide(), "STACK_SIDE");
                break; case A::STACK_SLOT:
                    if (isNA(v, cstack, "STACK_SLOT")) break;
                    ensureValueMatch(v, isSpecialStack ? STACK_SPECIAL_SLOT : cstack->unitSlot(), "STACK_SLOT");
                break; case A::STACK_CREATURE_TYPE:
                    if (isNA(v, cstack, "STACK_CREATURE_TYPE")) break;
                    ensureValueMatch(v, cstack->creatureId(), "STACK_CREATURE_TYPE");
                break; case A::STACK_AI_VALUE_TENTH:
                    if (isNA(v, cstack, "STACK_AI_VALUE_TENTH")) break;
                    ensureValueMatch(v, cstack->creatureId().toCreature()->getAIValue() / 10, "STACK_AI_VALUE_TENTH");
                break; case A::STACK_IS_ACTIVE:
                    if (isNA(v, cstack, "STACK_IS_ACTIVE")) break;
                    ensureValueMatch(v, isActive, "STACK_IS_ACTIVE");
                break; case A::STACK_IS_WIDE:
                    if (isNA(v, cstack, "STACK_IS_WIDE")) break;
                    ensureValueMatch(v, cstack->doubleWide(), "STACK_IS_WIDE");
                break; case A::STACK_FLYING:
                    if (isNA(v, cstack, "STACK_FLYING")) break;
                    ensureValueMatch(v, cstack->hasBonusOfType(BonusType::FLYING), "STACK_FLYING");
                break; case A::STACK_NO_MELEE_PENALTY:
                    if (isNA(v, cstack, "STACK_NO_MELEE_PENALTY")) break;
                    ensureValueMatch(v, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "STACK_NO_MELEE_PENALTY");
                break; case A::STACK_TWO_HEX_ATTACK_BREATH:
                    if (isNA(v, cstack, "STACK_TWO_HEX_ATTACK_BREATH")) break;
                    ensureValueMatch(v, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "STACK_TWO_HEX_ATTACK_BREATH");
                break; case A::STACK_BLOCKS_RETALIATION:
                    if (isNA(v, cstack, "STACK_BLOCKS_RETALIATION")) break;
                    ensureValueMatch(v, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "STACK_BLOCKS_RETALIATION");
                break; case A::STACK_DEFENSIVE_STANCE:
                    if (isNA(v, cstack, "STACK_DEFENSIVE_STANCE")) break;
                    ensureValueMatch(v, cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE), "STACK_DEFENSIVE_STANCE");
                break; default:
                    THROW_FORMAT("Unexpected attr: %d", EI(attr));
                }
            }
        }
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
        auto color = supdata->getColor();

        // find an active hex (i.e. with active stack on it)
        IHex* ahex;

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                if (hex->getAttrs().at(EI(HexAttribute::STACK_IS_ACTIVE)) == 1) {
                    ahex = hex;
                    break;
                }
            }
        }

        if (!ahex)
            logAi->warn("could not find an active hex. Is this a draw?");

        std::string nocol = "\033[0m";
        std::string redcol = "\033[31m"; // red
        std::string bluecol = "\033[34m"; // blue
        std::string activemod = "\033[107m\033[7m"; // bold+reversed

        std::vector<std::stringstream> lines;

        auto ourcol = redcol;
        auto enemycol = bluecol;

        if (color != "red") {
            ourcol = bluecol;
            enemycol = redcol;
        }

        //
        // 1. Add logs table:
        //
        // #1 attacks #5 for 16 dmg (1 killed)
        // #5 attacks #1 for 4 dmg (0 killed)
        // ...
        //
        for (auto &alog : supdata->getAttackLogs()) {
            auto row = std::stringstream();
            auto col1 = ourcol;
            auto col2 = enemycol;

            if (alog->getDefenderSide() == EI(supdata->getSide())) {
                col1 = enemycol;
                col2 = ourcol;
            }

            if (alog->getAttackerSlot() >= 0)
                row << col1 << "#" << alog->getAttackerSlot() << nocol;
            else
                row << "\033[7m" << "FX" << nocol;

            row << " attacks ";
            row << col2 << "#" << alog->getDefenderSlot() << nocol;
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

        auto stackhexes = std::array<IHex*, 14>{};

        // s=special; can be any number, slot is always 7 (SPECIAL), visualized A,B,C...
        char s_char = 'A';
        auto s_stackhexes = std::map<char, IHex*>{};

        auto tablestartrow = lines.size();

        lines.emplace_back() << "  ▕₀▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕";
        lines.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

        bool addspace = true;

        for (int y=0; y < BF_YMAX; y++) {
            for (int x=0; x < BF_XMAX; x++) {
                auto sym = std::string("?");
                auto &hex = hexes.at(y).at(x);
                auto amask = HexActMask(hex->getAttr(A::HEX_ACTION_MASK_FOR_ACT_STACK));
                auto masks = std::array<std::array<HexActMask, 7>, 2> {};

                for (int side=0; side<2; side++) {
                    auto baseattr = side ? A::HEX_ACTION_MASK_FOR_R_STACK_0 : A::HEX_ACTION_MASK_FOR_L_STACK_0;
                    for (int i=0; i<7; i++) {
                        auto attr = A(EI(baseattr) + i);
                        auto v = hex->getAttr(attr);
                        if (v != ATTR_UNSET) masks.at(side).at(i) = HexActMask(v);
                    }
                }

                auto &row = (x == 0)
                    ? (lines.emplace_back() << nummap.at(y%10) << "┨" << (y % 2 == 0 ? " " : ""))
                    : lines.back();

                if (addspace)
                    row << " ";

                addspace = true;

                // not checkable
                // // true if any any of the following context attributes is available:
                // // * (7) neighbouringFriendlyStacks
                // // * (7) neighbouringEnemyStacks
                // // * (7) potentialEnemyAttackers
                // // Expect to be available for FREE_REACHABLE hexes only:
                // auto anycontext_free_reachable = std::any_of(
                //     stateu.begin()+ibase+3+EI(StackAttr::count)+14,
                //     stateu.begin()+ibase+3+EI(StackAttr::count)+14 + 21,
                //     [](Schema::NValue nv) { return nv.orig != 0; }
                // );

                switch(HexState(hex->getAttr(A::HEX_STATE))) {
                break; case HexState::FREE: {
                    if (!supdata->getIsBattleEnded() && amask.test(EI(HexAction::MOVE)) > 0) {
                        sym = "○";

                        auto emasks = masks.at(!EI(supdata->getSide()));
                        for (auto &emask : emasks) {
                            if (emask.test(EI(HexAction::MOVE))) {
                                sym = "◎";
                                break;
                            }
                        }
                    } else {
                        sym = "\033[90m◌\033[0m";
                    }
                }
                break; case HexState::OBSTACLE:
                    sym = "\033[90m▦\033[0m";
                break; case HexState::STACK_FRONT:
                       case HexState::STACK_BACK: {
                    auto slot = hex->getAttr(A::STACK_SLOT);
                    auto friendly = hex->getAttr(A::STACK_SIDE) == EI(supdata->getSide());
                    auto col = friendly ? ourcol : enemycol;

                    if (hex->getAttr(A::STACK_IS_ACTIVE) > 0 && !supdata->getIsBattleEnded())
                        col += activemod;

                    auto strslot = (slot == STACK_SPECIAL_SLOT) ? std::string(1, s_char) : std::to_string(slot);

                    if (hex->getAttr(A::STACK_IS_WIDE) > 0) {
                        if (hex->getAttr(A::STACK_SIDE) == 0 && hex->getAttr(A::HEX_STATE) == EI(HexState::STACK_BACK)) {
                            strslot += "↠";
                            addspace = false;
                        } else if(hex->getAttr(A::STACK_SIDE) == 1 && hex->getAttr(A::HEX_STATE) == EI(HexState::STACK_FRONT)) {
                            strslot += "↞";
                            addspace = false;
                        }
                    }

                    if (hex->getAttr(A::HEX_STATE) == EI(HexState::STACK_FRONT)) {
                        if (slot == STACK_SPECIAL_SLOT) {
                            s_stackhexes[s_char] = hex;
                            s_char += 1;
                        }
                        else {
                            stackhexes.at(hex->getAttr(A::STACK_SIDE) ? 7+slot : slot) = hex;
                        }
                    }

                    sym = col + strslot + nocol;
                }
                break; default:
                    THROW_FORMAT("unexpected HEX_STATE: %d", EI(hex->getAttr(A::HEX_STATE)));
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

            switch(i) {
            case 1:
                name = "Player";
                if (supdata->getIsBattleEnded())
                    value = "";
                else
                    value = ourcol == redcol ? redcol + "RED" + nocol : bluecol + "BLUE" + nocol;
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
                auto restext = supdata->getIsVictorious()
                    ? (ourcol == redcol ? redcol + "RED WINS" : bluecol + "BLUE WINS")
                    : (ourcol == redcol ? bluecol + "BLUE WINS" : redcol + "RED WINS");

                name = "Battle result"; value = supdata->getIsBattleEnded() ? (restext + nocol) : "";
                break;
            }
            // case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
            default:
                continue;
            }

            lines.at(tablestartrow + i) << PadLeft(name, 15, ' ') << ": " << value;
        }

        //
        // 5. Add stacks table:
        //
        //          Stack # |   1   2   3   4   5   6   7   1   2   3   4   5   6   7
        // -----------------+--------------------------------------------------------
        //              Qty |   0  34   0   0   0   0   0   0  17   0   0   0   0   0
        //           Attack |   0   8   0   0   0   0   0   0   6   0   0   0   0   0
        //    ...10 more... | ...
        // -----------------+--------------------------------------------------------
        //
        // table with 16 columns (14 stacks +1 header +1 divider)
        // Each row represents a separate attribute

        using RowDef = std::tuple<HexAttribute, std::string>;

        // All cell text is aligned right
        auto colwidths = std::array<int, 16>{};
        colwidths.fill(4); // default col width
        colwidths.at(0) = 16; // header col
        colwidths.at(1) = 2; // divider col

        // {Attribute, name, colwidth}
        const auto rowdefs = std::vector<RowDef> {
            RowDef{A::STACK_SLOT, "Stack #"},
            RowDef{A::HEX_X_COORD, ""},  // divider row
            RowDef{A::STACK_QUANTITY, "Qty"},
            RowDef{A::STACK_ATTACK, "Attack"},
            RowDef{A::STACK_DEFENSE, "Defense"},
            RowDef{A::STACK_SHOTS, "Shots"},
            RowDef{A::STACK_DMG_MIN, "Dmg (min)"},
            RowDef{A::STACK_DMG_MAX, "Dmg (max)"},
            RowDef{A::STACK_HP, "HP"},
            RowDef{A::STACK_HP_LEFT, "HP left"},
            RowDef{A::STACK_SPEED, "Speed"},
            RowDef{A::STACK_WAITED, "Waited"},
            RowDef{A::STACK_QUEUE_POS, "Queue"},
            RowDef{A::STACK_RETALIATIONS_LEFT, "Ret. left"},
            RowDef{A::HEX_X_COORD, ""},  // divider row
        };

        // Table with nrows and ncells, each cell a 3-element tuple
        // cell: color, width, txt
        using TableCell = std::tuple<std::string, int, std::string>;
        using TableRow = std::array<TableCell, colwidths.size()>;

        auto table = std::vector<TableRow> {};

        auto divrow = TableRow{};
        for (int i=0; i<colwidths.size(); i++)
            divrow[i] = TableCell{nocol, colwidths.at(i), std::string(colwidths.at(i), '-')};
        divrow.at(1) = TableCell{nocol, colwidths.at(1), std::string(colwidths.at(1)-1, '-') + "+"};

        // Attribute rows
        for (auto& [a, aname] : rowdefs) {
            if (a == A::HEX_X_COORD) { // divider row
                table.push_back(divrow);
                continue;
            }

            auto row = TableRow{};

            // Header col
            row.at(0) = {nocol, colwidths.at(0), aname};

            // Div col
            row.at(1) = {nocol, colwidths.at(1), "|"};

            // Stack cols
            for (int i=0; i<stackhexes.size(); i++) {
                auto hex = stackhexes.at(i);
                auto color = nocol;
                std::string value = "";

                if (hex) {
                    color = (hex->getAttr(A::STACK_SIDE) == EI(supdata->getSide())) ? ourcol : enemycol;
                    value = std::to_string(hex->getAttr(a));
                    if (hex->getAttr(A::STACK_IS_ACTIVE) > 0 && !supdata->getIsBattleEnded()) color += activemod;
                }

                row.at(2+i) = {color, colwidths.at(2+i), value};
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
        // 6. Append special stacks to stats table (optional)
        //
        //          Stack # |   1 ... 7 | *(y,x)   *(y,x)   ...
        // -----------------+-----------+-------------------------
        //              Qty |   0 ... 0 |      0        0
        //           Attack |   0 ... 0 |      0        0
        //    ...10 more... | ... ...   |    ...      ...
        // -----------------+-----------+-------------------------
        //                              ^^^^^^^^^^^^^^
        //                              appended part with special stacks
        //                              (summoned creatures / war machines)

        // Using "S" suffix for special
        auto s_nstacks = s_stackhexes.size();

        if (s_nstacks > 0) {
            auto s_colwidths = std::vector<int>{};
            s_colwidths.push_back(2); // divider col
            s_colwidths.insert(s_colwidths.end(), s_nstacks, 4); // default width

            // Table with nrows and ncells (n=table above), each cell a 3-element tuple
            // cell: color, width, txt
            // XXX: std::array requires size to be known at compile time
            // using S_TableRow = std::array<S_TableCell, s_nstacks>;
            using S_TableCell = TableCell;
            using S_TableRow = std::vector<S_TableCell>;
            auto s_table = std::vector<S_TableRow> {};

            auto s_divrow = S_TableRow{};
            s_divrow.push_back(S_TableCell{nocol, s_colwidths.at(0), std::string(s_colwidths.at(0)-1, '-') + "+"});
            s_divrow.insert(s_divrow.end(), s_nstacks, S_TableCell{});

            for (int i=0; i<s_colwidths.size(); i++)
                s_divrow[i] = TableCell{nocol, s_colwidths.at(i), std::string(s_colwidths.at(i), '-')};

            // Attribute rows
            for (auto& [a, aname] : rowdefs) {
                if (a == A::HEX_X_COORD) { // divider row
                    s_table.push_back(s_divrow);
                    continue;
                }

                auto s_row = S_TableRow{};

                // Div col
                s_row.push_back(TableCell{nocol, s_colwidths.at(0), "|"});

                // Stack cols
                int i = 1;
                for (auto &[stackchar, hex] : s_stackhexes) {
                    auto color = nocol;
                    std::string value = "";

                    color = (hex->getAttr(A::STACK_SIDE) == EI(supdata->getSide())) ? ourcol : enemycol;
                    value = (a == HexAttribute::STACK_SLOT)
                        ? std::string(1, stackchar)
                        : std::to_string(hex->getAttr(a));

                    if (hex->getAttr(A::STACK_IS_ACTIVE) > 0 && !supdata->getIsBattleEnded()) color += activemod;

                    s_row.push_back(TableCell{color, s_colwidths.at(i++), value});
                }

                expect(s_row.size() == s_nstacks + 1, "expected s_row size of %d, got: %d", s_nstacks+1, s_row.size());
                s_table.push_back(s_row);
            }

            expect(s_table.size() == table.size(), "expected %d rows in special table, got: %d", table.size(), s_table.size());

            for (int i=0; i<s_table.size(); ++i) {
                auto &s_row = s_table.at(i);
                expect(s_row.size() == 1 + s_nstacks, "expected %d cols in special row, got: %d", s_nstacks, s_row.size());

                // Line to modify
                auto &line = lines.at(lines.size() - table.size() + i);
                for (auto& [color, width, txt] : s_row)
                    line << color << PadLeft(txt, width, ' ') << nocol;
            }
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
