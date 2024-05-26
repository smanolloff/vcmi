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

#include "BAI.h"
#include "CStack.h"
#include "battle/BattleHex.h"
#include "battle/ReachabilityInfo.h"
#include "battle/Unit.h"
#include "common.h"
#include "export.h"
#include "types/battlefield.h"
#include "types/hexaction.h"
#include <stdexcept>
#include <string>

namespace MMAI {
    using A = Export::Attribute;

    // static
    // TODO: fix for terminal observation
    //       info after off-turn update is somewhat inconsistent
    //       (have not investigated properly though)
    std::tuple<Hexes, const CStack*> BAI::Reconstruct(
        const Export::Result &r,
        const std::shared_ptr<CBattleCallback> cb,
        const std::shared_ptr<CPlayerBattleCallback> battle
    ) {
        auto stateu = r.stateUnencoded;
        auto hexes = Hexes();
        const CStack* astack; // XXX: can remain nullptr (for terminal obs)

        auto l_CStacks = std::array<const CStack*, 7>{};
        auto r_CStacks = std::array<const CStack*, 7>{};
        auto rinfos = std::map<const CStack*, ReachabilityInfo>{};

        for (auto &cstack : battle->battleGetStacks()) {
            if (cstack->unitId() == battle->battleActiveUnit()->unitId())
                astack = cstack;

            cstack->unitSide()
                ? r_CStacks.at(cstack->unitSlot()) = cstack
                : l_CStacks.at(cstack->unitSlot()) = cstack;

            rinfos.insert({cstack, battle->getReachability(cstack)});
        }

        // Return (attr == N/A), but after performing some checks
        auto isNA = [](int v, const CStack* stack, const char* attrname) {
            if (v == Export::STATE_VALUE_NA) {
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
                throw std::runtime_error("Unexpected v: " + std::to_string(v));
        };

        auto ensureReachability = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            expect(checkReachable(bh, v, stack), "%s: (bhex=%d) reachability expected: %d", attrname, bh.hex, v);
        };

        auto ensureReachabilityOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            ensureReachability(bh, v, stack, attrname);
        };

        // as opposed to ensureHexShootableOrNA, this hex works with a mask
        // values are 0 or 1 (not 0..2) and this check requires a valid target
        auto ensureShootability = [=](BattleHex bh, int v, const CStack* cstack, const char* attrname) {
            auto canshoot = battle->battleCanShoot(cstack);
            auto estacks = (cstack->unitSide() == BattleSide::DEFENDER) ? l_CStacks : r_CStacks;
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
                static_assert(Export::ShootDistance(0) == Export::ShootDistance::NA);
                expect(!canshoot, "%s: =0 but the stack is able to shoot", attrname);
            } else if (v == 1) {
                static_assert(Export::ShootDistance(1) == Export::ShootDistance::FAR);
                expect(canshoot, "%s: =1, but the stack is unable to shoot", attrname);
                expect(distance > 10, "%s: =1, but distance=%d (<=10)", attrname, distance);
            } else if (v == 2) {
                static_assert(Export::ShootDistance(2) == Export::ShootDistance::NEAR);
                expect(canshoot, "%s: =2, but the stack is unable to shoot", attrname);
                expect(norangepen || distance <= 10, "%s: =2, but norangepen=0 and distance=%d (>10)", attrname, distance, norangepen);
            } else {
                throw std::runtime_error("Unexpected v: " + std::to_string(v));
            }
        };

        auto ensureHexMeleeDistanceOrNA = [=](BattleHex bh, int v, const CStack* cstack, const char* attrname) {
            if (isNA(v, cstack, attrname)) return;
            auto estacks = (cstack->unitSide() == BattleSide::DEFENDER) ? l_CStacks : r_CStacks;

            static_assert(Export::MeleeDistance(0) == Export::MeleeDistance::NA);
            static_assert(Export::MeleeDistance(1) == Export::MeleeDistance::FAR);
            static_assert(Export::MeleeDistance(2) == Export::MeleeDistance::NEAR);

            if (v == 0) {
                // for each enemy stack
                //   ensure bh unreachable OR melee attack at cstack is impossible
                for (auto estack : estacks) {
                    if (!estack) continue;
                    if (!checkReachable(bh, 1, estack)) continue;
                    expect(!estack->isMeleeAttackPossible(estack, cstack, bh), "%s: =%d (bhex %d), but isAttackPossible=1", attrname, v, bh.hex);
                }
            } else {
                // find at least 1 enemy stack
                //   where bh is reachable AND melee attack at cstack is possible
                auto it = std::find_if(estacks.begin(), estacks.end(), [=](auto &estack) {
                    return estack \
                        && checkReachable(bh, 1, estack) \
                        && estack->isMeleeAttackPossible(estack, cstack, bh);
                });

                expect(it != estacks.end(), "%s: =%d (bhex %d), stack is not attackable by any enemy stack", attrname, bh.hex);

                int mindist = -1;
                for (auto &cbh : cstack->getHexes()) {
                    auto dist = int(BattleHex::getDistance(bh, cbh));
                    mindist = (mindist == -1) ? dist : std::min(mindist, dist);
                }

                if (v == 1) { // =FAR
                    expect(mindist == 2, "%s: =1=FAR (bhex %d), but real distance is %d", attrname, bh.hex, mindist);
                } else { // =NEAR
                    expect(mindist == 1, "%s: =2=NEAR (bhex %d), but real distance is %d", attrname, bh.hex, mindist);
                }
            }
        };

        auto ensureValueMatch = [=](const CStack* stack, int v, int vreal, const char* attrname) {
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
                throw std::runtime_error("Unexpected HexAction: " + std::to_string(EI(ha)));
              break;
            }

            auto estacks = (cstack->unitSide() == BattleSide::DEFENDER) ? l_CStacks : r_CStacks;
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

        // ihex = 0, 1, 2, .. 164
        // ibase = 0, 15, 30, ... 2460
        for (int ihex=0; ihex < BF_SIZE; ihex++) {
            int ibase = ihex * EI(A::_count);
            auto hex = Hex{};
            int x = ihex%15;
            int y = ihex/15;

            ASSERT(y == stateu.at(ibase).v, "hex.y mismatch");
            ASSERT(x == stateu.at(ibase+1).v, "hex.x mismatch");
            auto bh = BattleHex(x+1, y);
            hex.bhex = bh;

            for (int i=0; i < EI(A::_count); i++) {
                hex.attrs.at(i) = stateu.at(ibase + i).v;
            }

            const CStack *cstack = nullptr;
            bool isActive = false;
            bool isRight = false;

            if (hex.attr(A::STACK_QUANTITY) != Export::STATE_VALUE_NA) {
                // there's a stack on this hex
                isRight = hex.attr(A::STACK_SIDE);
                cstack = isRight ? r_CStacks.at(hex.attr(A::STACK_SLOT)) : l_CStacks.at(hex.attr(A::STACK_SLOT));
                hex.cstack = cstack;
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
                auto v = hex.attrs.at(i);

                switch(attr) {
                break; case A::HEX_Y_COORD:
                    expect(v == y, "HEX_Y_COORD: %d != %d", v, y);
                break; case A::HEX_X_COORD:
                    expect(v == x, "HEX_X_COORD: %d != %d", v, x);
                break; case A::HEX_STATE:
                    switch (Export::HexState(v)) {
                    break; case Export::HexState::OBSTACLE:
                        expect(aa == EAccessibility::OBSTACLE, "HEX_STATE: OBSTACLE -> %d", aa);
                    break; case Export::HexState::OCCUPIED:
                        expect(aa == EAccessibility::ALIVE_STACK, "HEX_STATE: OCCUPIED -> %d", aa);
                    break; case Export::HexState::FREE:
                        expect(aa == EAccessibility::ACCESSIBLE, "HEX_STATE: FREE -> %d", aa);
                    break; default:
                        throw std::runtime_error("HEX_STATE: Unexpected HexState: " + std::to_string(v));
                    }
                break; case A::HEX_ACTION_MASK_FOR_ACT_STACK: {
                    // Check mask is the same for the corresponding FRIENDLY_STACK_ attribute
                    auto baseattr = astack->unitSide()
                        ? A::HEX_ACTION_MASK_FOR_R_STACK_0
                        : A::HEX_ACTION_MASK_FOR_L_STACK_0;

                    expect(
                        v == hex.attrs.at(EI(baseattr) + astack->unitSlot()),
                        "HEX_ACTION_MASK_FOR_ACT_STACK: =%d but different corresponding R/L v=%d",
                        v, hex.attrs.at(EI(baseattr) + astack->unitSlot())
                    );
                }
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
                break; case A::HEX_MELEEABLE_BY_ACT_STACK: {
                    // Check meleeability is the same for the corresponding FRIENDLY_STACK_ attribute
                    auto baseattr = astack->unitSide()
                        ? A::HEX_MELEEABLE_BY_R_STACK_0
                        : A::HEX_MELEEABLE_BY_L_STACK_0;

                    expect(
                        v == hex.attrs.at(EI(baseattr) + astack->unitSlot()),
                        "HEX_MELEEABLE_BY_ACT_STACK: =%d but different corresponding R/L v=%d",
                        v, hex.attrs.at(EI(baseattr) + astack->unitSlot())
                    );
                }
                break; case A::HEX_MELEEABLE_BY_L_STACK_0:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(0), "HEX_MELEEABLE_BY_L_STACK_0");
                break; case A::HEX_MELEEABLE_BY_L_STACK_1:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(1), "HEX_MELEEABLE_BY_L_STACK_1");
                break; case A::HEX_MELEEABLE_BY_L_STACK_2:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(2), "HEX_MELEEABLE_BY_L_STACK_2");
                break; case A::HEX_MELEEABLE_BY_L_STACK_3:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(3), "HEX_MELEEABLE_BY_L_STACK_3");
                break; case A::HEX_MELEEABLE_BY_L_STACK_4:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(4), "HEX_MELEEABLE_BY_L_STACK_4");
                break; case A::HEX_MELEEABLE_BY_L_STACK_5:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(5), "HEX_MELEEABLE_BY_L_STACK_5");
                break; case A::HEX_MELEEABLE_BY_L_STACK_6:
                    ensureMeleeableOrNA(hex.bhex, v, l_CStacks.at(6), "HEX_MELEEABLE_BY_L_STACK_6");
                break; case A::HEX_MELEEABLE_BY_R_STACK_0:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(0), "HEX_MELEEABLE_BY_R_STACK_0");
                break; case A::HEX_MELEEABLE_BY_R_STACK_1:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(1), "HEX_MELEEABLE_BY_R_STACK_1");
                break; case A::HEX_MELEEABLE_BY_R_STACK_2:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(2), "HEX_MELEEABLE_BY_R_STACK_2");
                break; case A::HEX_MELEEABLE_BY_R_STACK_3:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(3), "HEX_MELEEABLE_BY_R_STACK_3");
                break; case A::HEX_MELEEABLE_BY_R_STACK_4:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(4), "HEX_MELEEABLE_BY_R_STACK_4");
                break; case A::HEX_MELEEABLE_BY_R_STACK_5:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(5), "HEX_MELEEABLE_BY_R_STACK_5");
                break; case A::HEX_MELEEABLE_BY_R_STACK_6:
                    ensureMeleeableOrNA(hex.bhex, v, r_CStacks.at(6), "HEX_MELEEABLE_BY_R_STACK_6");
                break; case A::HEX_SHOOT_DISTANCE_FROM_ACT_STACK: {
                    // Check meleeability is the same for the corresponding FRIENDLY_STACK_ attribute
                    auto baseattr = astack->unitSide()
                        ? A::HEX_SHOOT_DISTANCE_FROM_R_STACK_0
                        : A::HEX_SHOOT_DISTANCE_FROM_L_STACK_0;

                    expect(
                        v == hex.attrs.at(EI(baseattr) + astack->unitSlot()),
                        "HEX_SHOOT_DISTANCE_FROM_ACT_STACK: =%d but different corresponding R/L v=%d",
                        v, hex.attrs.at(EI(baseattr) + astack->unitSlot())
                    );
                }
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_0:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(0), "HEX_SHOOT_DISTANCE_FROM_L_STACK_0");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_1:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(1), "HEX_SHOOT_DISTANCE_FROM_L_STACK_1");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_2:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(2), "HEX_SHOOT_DISTANCE_FROM_L_STACK_2");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_3:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(3), "HEX_SHOOT_DISTANCE_FROM_L_STACK_3");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_4:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(4), "HEX_SHOOT_DISTANCE_FROM_L_STACK_4");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_5:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(5), "HEX_SHOOT_DISTANCE_FROM_L_STACK_5");
                break; case A::HEX_SHOOT_DISTANCE_FROM_L_STACK_6:
                    ensureHexShootableOrNA(hex.bhex, v, l_CStacks.at(6), "HEX_SHOOT_DISTANCE_FROM_L_STACK_6");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_0:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(0), "HEX_SHOOT_DISTANCE_FROM_R_STACK_0");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_1:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(1), "HEX_SHOOT_DISTANCE_FROM_R_STACK_1");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_2:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(2), "HEX_SHOOT_DISTANCE_FROM_R_STACK_2");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_3:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(3), "HEX_SHOOT_DISTANCE_FROM_R_STACK_3");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_4:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(4), "HEX_SHOOT_DISTANCE_FROM_R_STACK_4");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_5:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(5), "HEX_SHOOT_DISTANCE_FROM_R_STACK_5");
                break; case A::HEX_SHOOT_DISTANCE_FROM_R_STACK_6:
                    ensureHexShootableOrNA(hex.bhex, v, r_CStacks.at(6), "HEX_SHOOT_DISTANCE_FROM_R_STACK_6");
                break; case A::HEX_MELEE_DISTANCE_FROM_ACT_STACK: {
                    // Check meleeability is the same for the corresponding FRIENDLY_STACK_ attribute
                    auto baseattr = astack->unitSide()
                        ? A::HEX_MELEE_DISTANCE_FROM_R_STACK_0
                        : A::HEX_MELEE_DISTANCE_FROM_L_STACK_0;

                    expect(
                        v == hex.attrs.at(EI(baseattr) + astack->unitSlot()),
                        "HEX_MELEE_DISTANCE_FROM_ACT_STACK: =%d but different corresponding R/L v=%d",
                        v, hex.attrs.at(EI(baseattr) + astack->unitSlot())
                    );
                }
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_0:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(0), "HEX_MELEE_DISTANCE_FROM_L_STACK_0");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_1:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(1), "HEX_MELEE_DISTANCE_FROM_L_STACK_1");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_2:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(2), "HEX_MELEE_DISTANCE_FROM_L_STACK_2");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_3:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(3), "HEX_MELEE_DISTANCE_FROM_L_STACK_3");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_4:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(4), "HEX_MELEE_DISTANCE_FROM_L_STACK_4");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_5:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(5), "HEX_MELEE_DISTANCE_FROM_L_STACK_5");
                break; case A::HEX_MELEE_DISTANCE_FROM_L_STACK_6:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, l_CStacks.at(6), "HEX_MELEE_DISTANCE_FROM_L_STACK_6");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_0:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(0), "HEX_MELEE_DISTANCE_FROM_R_STACK_0");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_1:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(1), "HEX_MELEE_DISTANCE_FROM_R_STACK_1");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_2:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(2), "HEX_MELEE_DISTANCE_FROM_R_STACK_2");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_3:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(3), "HEX_MELEE_DISTANCE_FROM_R_STACK_3");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_4:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(4), "HEX_MELEE_DISTANCE_FROM_R_STACK_4");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_5:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(5), "HEX_MELEE_DISTANCE_FROM_R_STACK_5");
                break; case A::HEX_MELEE_DISTANCE_FROM_R_STACK_6:
                    ensureHexMeleeDistanceOrNA(hex.bhex, v, r_CStacks.at(6), "HEX_MELEE_DISTANCE_FROM_R_STACK_6");
                break; case A::STACK_QUANTITY:
                    // need separate N/A check (cstack may be nullptr)
                    if (isNA(v, cstack, "STACK_QUANTITY")) break;
                    ensureValueMatch(cstack, v, std::min(cstack->getCount(), 1023), "STACK_QUANTITY");
                break; case A::STACK_ATTACK:
                    if (isNA(v, cstack, "STACK_ATTACK")) break;
                    ensureValueMatch(cstack, v, cstack->getAttack(false), "STACK_ATTACK");
                break; case A::STACK_DEFENSE:
                    if (isNA(v, cstack, "STACK_DEFENSE")) break;
                    ensureValueMatch(cstack, v, cstack->getDefense(false), "STACK_DEFENSE");
                break; case A::STACK_SHOTS:
                    if (isNA(v, cstack, "STACK_SHOTS")) break;
                    ensureValueMatch(cstack, v, cstack->shots.available(), "STACK_SHOTS");
                break; case A::STACK_DMG_MIN:
                    if (isNA(v, cstack, "STACK_DMG_MIN")) break;
                    ensureValueMatch(cstack, v, cstack->getMinDamage(false), "STACK_DMG_MIN");
                break; case A::STACK_DMG_MAX:
                    if (isNA(v, cstack, "STACK_DMG_MAX")) break;
                    ensureValueMatch(cstack, v, cstack->getMaxDamage(false), "STACK_DMG_MAX");
                break; case A::STACK_HP:
                    if (isNA(v, cstack, "STACK_HP")) break;
                    ensureValueMatch(cstack, v, cstack->getMaxHealth(), "STACK_HP");
                break; case A::STACK_HP_LEFT:
                    if (isNA(v, cstack, "STACK_HP_LEFT")) break;
                    ensureValueMatch(cstack, v, cstack->getFirstHPleft(), "STACK_HP_LEFT");
                break; case A::STACK_SPEED:
                    if (isNA(v, cstack, "STACK_SPEED")) break;
                    ensureValueMatch(cstack, v, cstack->getMovementRange(), "STACK_SPEED");
                break; case A::STACK_WAITED:
                    if (isNA(v, cstack, "STACK_WAITED")) break;
                    ensureValueMatch(cstack, v, cstack->waitedThisTurn, "STACK_WAITED");
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
                    ensureValueMatch(cstack, v, cstack->unitSide(), "STACK_SIDE");
                break; case A::STACK_SLOT:
                    if (isNA(v, cstack, "STACK_SLOT")) break;
                    ensureValueMatch(cstack, v, cstack->unitSlot(), "STACK_SLOT");
                break; case A::STACK_CREATURE_TYPE:
                    if (isNA(v, cstack, "STACK_CREATURE_TYPE")) break;
                    ensureValueMatch(cstack, v, cstack->creatureId(), "STACK_CREATURE_TYPE");
                break; case A::STACK_AI_VALUE_TENTH:
                    if (isNA(v, cstack, "STACK_AI_VALUE_TENTH")) break;
                    ensureValueMatch(cstack, v, cstack->creatureId().toCreature()->getAIValue() / 10, "STACK_AI_VALUE_TENTH");
                break; case A::STACK_IS_ACTIVE:
                    if (isNA(v, cstack, "STACK_IS_ACTIVE")) break;
                    ensureValueMatch(cstack, v, isActive, "STACK_IS_ACTIVE");
                break; case A::STACK_IS_WIDE:
                    if (isNA(v, cstack, "STACK_IS_WIDE")) break;
                    ensureValueMatch(cstack, v, cstack->doubleWide(), "STACK_IS_WIDE");
                break; case A::STACK_FLYING:
                    if (isNA(v, cstack, "STACK_FLYING")) break;
                    ensureValueMatch(cstack, v, cstack->hasBonusOfType(BonusType::FLYING), "STACK_FLYING");
                break; case A::STACK_NO_MELEE_PENALTY:
                    if (isNA(v, cstack, "STACK_NO_MELEE_PENALTY")) break;
                    ensureValueMatch(cstack, v, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "STACK_NO_MELEE_PENALTY");
                break; case A::STACK_TWO_HEX_ATTACK_BREATH:
                    if (isNA(v, cstack, "STACK_TWO_HEX_ATTACK_BREATH")) break;
                    ensureValueMatch(cstack, v, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "STACK_TWO_HEX_ATTACK_BREATH");
                break; case A::STACK_BLOCKS_RETALIATION:
                    if (isNA(v, cstack, "STACK_BLOCKS_RETALIATION")) break;
                    ensureValueMatch(cstack, v, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "STACK_BLOCKS_RETALIATION");
                break; case A::STACK_DEFENSIVE_STANCE:
                    if (isNA(v, cstack, "STACK_DEFENSIVE_STANCE")) break;
                    ensureValueMatch(cstack, v, cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE), "STACK_DEFENSIVE_STANCE");
                break; default:
                    throw std::runtime_error("Unexpected attr: " + std::to_string(EI(attr)));
                }
            }

            hexes.at(y).at(x) = hex;
        }


        return std::tuple{hexes, astack};
    }

    void BAI::Verify(const Battlefield &bf, Hexes &hexes, const CStack* astack) {
        for (int y=0; y<BF_YMAX; y++) {
            for (int x=0; x<BF_XMAX; x++) {
                auto hex0 = bf.hexes.at(y).at(x);
                auto hex = hexes.at(y).at(x);

                expect(hex0.bhex == hex.bhex, "mismatch: hex.bhex");
                expect(hex0.cstack == hex.cstack, "mismatch: hex.cstack");

                for (int i=0; i < EI(A::_count); i++)
                    expect(hex0.attrs.at(i) == hex.attrs.at(i), "mismatch: hex.attrs.at(%d)", i);
            }
        }

        expect(bf.astack == astack, "mismatch: astack");
    }

    // static
    std::string BAI::Render(
        const Export::Result &r,
        const std::shared_ptr<CBattleCallback> cb,
        const std::shared_ptr<CPlayerBattleCallback> battle,
        const Battlefield &bf,  // verification only
        const std::string color,
        const Action *action,  // for displaying "last action"
        const std::vector<AttackLog> attackLogs // for displaying log
    ) {
        expect(r.stateUnencoded.size() == 165*EI(A::_count), "r.stateu.size %d != 165*%d", r.stateUnencoded.size(), EI(A::_count));

        auto reconstructed = Reconstruct(r, cb, battle);
        auto hexes = std::get<0>(reconstructed);
        auto astack = std::get<1>(reconstructed);

        Verify(bf, hexes, astack);

        auto stateu = r.stateUnencoded;

        std::string nocol = "\033[0m";
        std::string redcol = "\033[31m"; // red
        std::string bluecol = "\033[34m"; // blue
        std::string activemod = "\033[107m\033[7m"; // bold+reversed

        std::vector<std::stringstream> rows;

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
        for (auto &alog : attackLogs) {
            auto row = std::stringstream();
            auto col1 = ourcol;
            auto col2 = enemycol;

            if (alog.isOurStackAttacked) {
                col1 = enemycol;
                col2 = ourcol;
            }

            if (alog.attslot >= 0)
                row << col1 << "#" << alog.attslot << nocol;
            else
                row << "\033[7m" << "FX" << nocol;

            row << " attacks ";
            row << col2 << "#" << alog.defslot << nocol;
            row << " for " << alog.dmg << " dmg";
            row << " (kills: " << alog.units << ", value: " << alog.value << ")";

            rows.push_back(std::move(row));
        }

        //
        // 2. Add errors
        //
        // Error: target hex is unreachable
        // ...

        for (const auto& pair : Export::ERRORS) {
            auto errtuple = pair.second;
            auto errflag = std::get<0>(errtuple);
            auto errname = std::get<1>(errtuple);
            auto errmsg = std::get<2>(errtuple);
            if (r.errmask & errflag)
                rows.emplace_back("[" + errname + "] Error: " + errmsg);
        }

        //
        // 3. Build ASCII table
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

        auto stackhexes = std::array<std::shared_ptr<Hex>, 14>{};

        auto tablestartrow = rows.size();

        rows.emplace_back() << "  ▕₀▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕";
        rows.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

        for (int y=0; y < BF_YMAX; y++) {
            for (int x=0; x < BF_XMAX; x++) {
                auto sym = std::string("?");
                auto &hex = hexes.at(y).at(x);

                auto amask = HexActMask(hex.attr(A::HEX_ACTION_MASK_FOR_ACT_STACK));
                auto masks = std::array<std::array<HexActMask, 7>, 2> {};

                for (int side=0; side<2; side++) {
                    auto baseattr = side ? A::HEX_ACTION_MASK_FOR_R_STACK_0 : A::HEX_ACTION_MASK_FOR_L_STACK_0;
                    for (int i=0; i<7; i++) {
                        auto attr = A(EI(baseattr) + i);
                        auto v = hex.attr(attr);
                        if (v != ATTR_UNSET) masks.at(side).at(i) = HexActMask(v);
                    }
                }

                auto &row = (x == 0)
                    ? (rows.emplace_back() << nummap.at(y%10) << "┨" << (y % 2 == 0 ? " " : ""))
                    : rows.back();

                row << " ";

                // not checkable
                // // true if any any of the following context attributes is available:
                // // * (7) neighbouringFriendlyStacks
                // // * (7) neighbouringEnemyStacks
                // // * (7) potentialEnemyAttackers
                // // Expect to be available for FREE_REACHABLE hexes only:
                // auto anycontext_free_reachable = std::any_of(
                //     stateu.begin()+ibase+3+EI(StackAttr::count)+14,
                //     stateu.begin()+ibase+3+EI(StackAttr::count)+14 + 21,
                //     [](Export::NValue nv) { return nv.orig != 0; }
                // );

                switch(hex.getState()) {
                break; case Export::HexState::FREE: {
                    if (amask.test(EI(HexAction::MOVE)) > 0) {
                        sym = "○";

                        if (astack) {
                            auto emasks = masks.at(!astack->unitSide());
                            for (auto &emask : emasks) {
                                if (emask.test(EI(HexAction::MOVE))) {
                                    sym = "◎";
                                    break;
                                }
                            }
                        }
                    } else {
                        sym = "\033[90m◌\033[0m";
                    }
                }
                break; case Export::HexState::OBSTACLE:
                    sym = "\033[90m▦\033[0m";
                break; case Export::HexState::OCCUPIED: {
                    auto slot = hex.attr(A::STACK_SLOT);
                    auto friendly = hex.attr(A::STACK_SIDE) == battle->battleGetMySide();
                    auto col = friendly ? ourcol : enemycol;

                    if (hex.attr(A::STACK_IS_ACTIVE) > 0)
                        col += activemod;

                    stackhexes.at(hex.attr(A::STACK_SIDE) ? 7+slot : slot) = std::make_shared<Hex>(hex);
                    sym = col + std::to_string(slot) + nocol;
                }
                break; default:
                    throw std::runtime_error("unexpected hex.stateu: " + std::to_string(EI(hex.getState())));
                }

                row << sym;

                if (x == BF_XMAX-1) {
                    row << (y % 2 == 0 ? " " : "  ") << "┠" << nummap.at(y % 10);
                }
            }
        }

        rows.emplace_back() << " ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃";
        rows.emplace_back() << "  ▕⁰▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕";

        //
        // 4. Add side table stuff
        //
        //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
        //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃    Last action:
        // ₁┨  ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠₁         Errors: 0
        // ₂┨ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠₂      DMG dealt: 0
        // ₃┨  1 ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌ ◌ ◌ 1 ┠₃   Units killed: 0
        // ...

        auto n_errors = 0;
        auto errmask = r.errmask;
        while (errmask) {
                n_errors += errmask & 1;
                errmask >>= 1;
        }

        for (int i=0; i<=rows.size(); i++) {
            std::string name;
            std::string value;

            switch(i) {
            case 1:
                name = "Player";
                value = ourcol == redcol ? redcol + "RED" + nocol : bluecol + "BLUE" + nocol;
                break;
            case 2:
                name = "Last action";
                value = action ? action->name() + " [" + std::to_string(action->action) + "]" : "";
                break;
            case 3: name = "Errors"; value = std::to_string(n_errors); break;
            case 4: name = "DMG dealt"; value = std::to_string(r.dmgDealt); break;
            case 5: name = "Units killed"; value = std::to_string(r.unitsKilled); break;
            case 6: name = "Value killed"; value = std::to_string(r.valueKilled); break;
            case 7: name = "DMG received"; value = std::to_string(r.dmgReceived); break;
            case 8: name = "Units lost"; value = std::to_string(r.unitsLost); break;
            case 9: name = "Value lost"; value = std::to_string(r.valueLost); break;
            case 10: {
                auto restext = r.victory
                    ? (ourcol == redcol ? redcol + "RED WINS" : bluecol + "BLUE WINS")
                    : (ourcol == redcol ? bluecol + "BLUE WINS" : redcol + "RED WINS");

                name = "Battle result"; value = r.ended ? (restext + nocol) : "";
                break;
            }
            // case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
            default:
                continue;
            }

            rows.at(tablestartrow + i) << PadLeft(name, 15, ' ') << ": " << value;
        }

        //
        // 5. Add stats table:
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

        using RowDef = std::tuple<Export::Attribute, std::string>;

        // All cell text is aligned right
        auto colwidths = std::array<int, 16>{};
        colwidths.fill(4); // default col width
        colwidths.at(0) = 16; // header col
        colwidths.at(1) = 2; // header col

        // {Attribute, name, colwidth}
        const auto rowdefs = std::vector<RowDef> {
            RowDef{A::STACK_SLOT, "Stack #"},
            RowDef{A::HEX_X_COORD, ""},
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
            RowDef{A::HEX_X_COORD, ""},
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

        // Header row
        // table.emplace_back(...) // TODO

        // Divider row
        // table.emplace_back(...) // TODO

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
                    color = (hex->attr(A::STACK_SIDE) == astack->unitSide()) ? ourcol : enemycol;
                    value = std::to_string(hex->attr(a));
                    if (hex->attr(A::STACK_IS_ACTIVE) > 0) color += activemod;
                }

                row.at(2+i) = {color, colwidths.at(2+i), value};
            }

            table.push_back(row);
        }

        // XXX: table is rotated here
        for (auto &r : table) {
            auto row = std::stringstream();
            for (auto& [color, width, txt] : r)
                row << color << PadLeft(txt, width, ' ') << nocol;

            rows.push_back(std::move(row));
        }

        //
        // 6. Join rows into a single string
        //
        std::string res = rows[0].str();
        for (int i=1; i<rows.size(); i++)
            res += "\n" + rows[i].str();

        return res;
    }
}
