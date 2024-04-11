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
        const std::shared_ptr<CBattleCallback> cb
    ) {
        auto stateu = r.stateUnencoded;
        auto hexes = Hexes();
        const CStack* astack; // XXX: can remain nullptr (for terminal obs)

        auto friendlyCStacks = std::array<const CStack*, 7>{};
        auto enemyCStacks = std::array<const CStack*, 7>{};
        auto rinfos = std::map<const CStack*, ReachabilityInfo>{};

        for (auto &cstack : cb->battleGetStacks()) {
            if (cstack->unitId() == cb->battleActiveUnit()->unitId())
                astack = cstack;

            cstack->unitSide() == cb->battleGetMySide()
                ? friendlyCStacks.at(cstack->unitSlot()) = cstack
                : enemyCStacks.at(cstack->unitSlot()) = cstack;

            rinfos.insert({cstack, cb->getReachability(cstack)});
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

        auto ensureReachableOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto distance = rinfos.at(stack).distances.at(bh);
            auto canreach = (stack->speed() >= distance);
            if (v == 0)
                expect(!canreach, "%s: =0 but actually reachable (speed=%d, distance=%d, coversPos=%d)", attrname, stack->speed(), distance, stack->coversPos(bh));
            else if (v == 1)
                expect(canreach, "%s: =1 but actually unreachable (speed=%d, distance=%d, coversPos=%d)", attrname, stack->speed(), distance, stack->coversPos(bh));
            else
                throw std::runtime_error("Unexpected v: " + std::to_string(v));
        };

        // Two functions for neughbouring hexes:
        //
        // getHexesForFixedAttacker():
        // Attacks are evaluated relative to a FIXED ATTACKER "@":
        //
        //  1-hex:        2-hex (attacker):     2-hex (defender):
        //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
        // . . X X . . . . . . . X X X . . . . . . . . X X X . . .
        //  . X @ X . . . . . . X - @ X . . . . . . . X @ - X . . .
        // . . X X . . . . . . . X X X . . . . . . . . X X X . . .
        //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
        //
        // getHexActionsFixedAttacker():
        // Attacks are evaluated relative to a FIXED TARGET "X":
        //
        //  1-hex:        2-hex (attacker):     2-hex (defender):
        //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
        // . . @ @ . . . . . . - @ @ @ . . . . . . . . @ @ @ - . .
        //  . @ X @ . . . . . - @ X - @ . . . . . . . @ - X @ - . .
        // . . @ @ . . . . . . - @ @ @ . . . . . . . . @ @ @ - . .
        //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .

        auto getHexActionsFixedAttacker = [enemyCStacks](BattleHex bh, const CStack* astack){
            auto res = std::vector<HexAction>{};

            auto nbhs = std::vector<BattleHex>{
                bh.cloneInDirection(BattleHex::BOTTOM_LEFT, false),
                bh.cloneInDirection(BattleHex::TOP_LEFT, false),
                bh.cloneInDirection(BattleHex::TOP_RIGHT, false),
                bh.cloneInDirection(BattleHex::BOTTOM_RIGHT, false),
            };

            if (astack->doubleWide()) {
                if (astack->unitSide() == BattleSide::ATTACKER) {
                    // attacker's "back" hex is to-the-left
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::BOTTOM_LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false).cloneInDirection(BattleHex::TOP_LEFT, false));
                } else {
                    // attacker's "back" hex is to-the-right
                    nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::TOP_RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::RIGHT, false));
                    nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false).cloneInDirection(BattleHex::BOTTOM_RIGHT, false));
                }
            } else {
                nbhs.push_back(bh.cloneInDirection(BattleHex::LEFT, false));
                nbhs.push_back(bh.cloneInDirection(BattleHex::RIGHT, false));
            }

            for (auto &nbh : nbhs) {
                for (auto &cstack : enemyCStacks) {
                    if (!(cstack && cstack->coversPos(nbh)))
                        continue;

                    HexAction ha;

                    switch(bh.mutualPosition(bh, nbh)) {;
                    break; case BattleHex::TOP_LEFT: ha = HexAction::AMOVE_TL;
                    break; case BattleHex::TOP_RIGHT: ha = HexAction::AMOVE_TR;
                    break; case BattleHex::RIGHT:  ha = HexAction::AMOVE_R;
                    break; case BattleHex::BOTTOM_RIGHT: ha = HexAction::AMOVE_BR;
                    break; case BattleHex::BOTTOM_LEFT: ha = HexAction::AMOVE_BL;
                    break; case BattleHex::LEFT: ha = HexAction::AMOVE_L;
                    break; case BattleHex::NONE:
                        // nbh is a special hex (remote) => we can attack
                        // with our other hex (obh)
                        expect(astack->doubleWide(), "special hex attack for single-hex astack???");

                        if (astack->unitSide() == BattleSide::ATTACKER) {
                            auto obh = bh.cloneInDirection(BattleHex::LEFT);
                            switch(obh.mutualPosition(obh, nbh)) {
                            break; case BattleHex::TOP_LEFT: ha = HexAction::AMOVE_2TL;
                            break; case BattleHex::BOTTOM_LEFT: ha = HexAction::AMOVE_2BL;
                            break; case BattleHex::LEFT: ha = HexAction::AMOVE_2L;
                            break; default:
                                throw std::runtime_error("Unexpected mutualpos (ATTACKER): " + std::to_string(obh.mutualPosition(obh, nbh)));
                            }
                        } else {
                            auto obh = bh.cloneInDirection(BattleHex::RIGHT);
                            switch(obh.mutualPosition(obh, nbh)) {
                            break; case BattleHex::TOP_RIGHT: ha = HexAction::AMOVE_2TR;
                            break; case BattleHex::BOTTOM_RIGHT: ha = HexAction::AMOVE_2BR;
                            break; case BattleHex::RIGHT:  ha = HexAction::AMOVE_2R;
                            break; default:
                                throw std::runtime_error("Unexpected mutualpos (DEFENDER): " + std::to_string(obh.mutualPosition(obh, nbh)));
                            }
                        }
                    break; default:
                        throw std::runtime_error("Unexpected mutualpos" + std::to_string(bh.mutualPosition(bh, nbh)));
                    }

                    res.push_back(ha);
                    // no other stack can cover this hex
                    // break // not breaking (sanity check)
                }
            }

            return res;
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
                if (nbh.isAvailable() && rinfos.at(stack).distances.at(nbh) <= stack->speed())
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

        auto ensureShootableOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto canshoot = cb->battleCanShoot(stack);
            auto distance = bh.getDistance(bh, stack->getPosition());
            auto norangepen = stack->hasBonusOfType(BonusType::NO_DISTANCE_PENALTY);

            if (v == 0) {
                static_assert(Export::DmgMod(0) == Export::DmgMod::ZERO);
                expect(!canshoot, "%s: =0 but the stack is able to shoot", attrname);
            } else if (v == 1) {
                static_assert(Export::DmgMod(1) == Export::DmgMod::HALF);
                expect(canshoot, "%s: =1, but the stack is unable to shoot", attrname);
                expect(distance > 10, "%s: =1, but the distance <= 10 (%d)", attrname, distance);
            } else if (v == 2) {
                static_assert(Export::DmgMod(2) == Export::DmgMod::FULL);
                expect(canshoot, "%s: =2, but the stack is unable to shoot", attrname);
                expect(norangepen || distance <= 10, "%s: =2, but the distance > 10 (%d)", attrname, distance);
            } else {
                throw std::runtime_error("Unexpected v: " + std::to_string(v));
            }
        };

        auto ensureNextToOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto res = BattleHex{};

            for (auto &nbh : stack->getSurroundingHexes(bh, false, 0)) {
                if (stack->coversPos(nbh)) {
                    res = nbh;
                    break;
                }
            }

            if (v == 0)
                expect(!res.isAvailable(), "%s: =0 (bhex %d), but there's a stack on a neighbouring hex (bhex %d)", attrname, bh.hex, res.hex);
            else if (v == 1)
                expect(res, "%s: =1 (bhex %d) but the stack does not cover any neighbouring hex", attrname, bh.hex);
            else
                throw std::runtime_error("Unexpected v: " + std::to_string(v));
        };

        auto ensureValueMatch = [=](const CStack* stack, int v, int vreal, const char* attrname) {
            expect(v == vreal, "%s: =%d, but is %d", attrname, v, vreal);
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
            hex.hexactmask.fill(false);

            for (int i=0; i < EI(A::_count); i++) {
                hex.attrs.at(i) = stateu.at(ibase + i).v;
            }

            const CStack *cstack = nullptr;
            bool isActive = false;
            bool isFriendly = false;

            if (hex.attr(A::STACK_QUANTITY) != Export::STATE_VALUE_NA) {
                // there's a stack on this hex
                isFriendly = hex.attr(A::STACK_SIDE) == cb->battleGetMySide();
                cstack = isFriendly ? friendlyCStacks.at(hex.attr(A::STACK_SLOT)) : enemyCStacks.at(hex.attr(A::STACK_SLOT));
                hex.cstack = cstack;
                isActive = (cstack == astack);
                expect(cstack, "could not find cstack");

                if (cstack->doubleWide())
                    expect(cstack->coversPos(bh), "Hex's stack info about side+slot is incorrect (two-hex stack)");
                else
                    expect(cstack->getPosition() == bh, "Hex's stack info about side+slot is incorrect");
            }

            // auto rinfo = cb->getReachability(cstack);
            auto ainfo = cb->getAccesibility();
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
                    switch (HexState(v)) {
                    break; case HexState::OBSTACLE:
                        expect(aa == EAccessibility::OBSTACLE, "HEX_STATE: OBSTACLE -> %d", aa);
                    break; case HexState::OCCUPIED:
                        expect(aa == EAccessibility::ALIVE_STACK, "HEX_STATE: OCCUPIED -> %d", aa);
                    break; case HexState::FREE:
                        expect(aa == EAccessibility::ACCESSIBLE, "HEX_STATE: FREE -> %d", aa);
                    break; default:
                        throw std::runtime_error("HEX_STATE: Unexpected HexState: " + std::to_string(v));
                    }
                break; case A::HEX_REACHABLE_BY_ACTIVE_STACK:
                    ensureReachableOrNA(bh, v, astack, "HEX_REACHABLE_BY_FRIENDLY_STACK_0");

                    // Check reachability is the same for the corresponding FRIENDLY_STACK_ attribute
                    expect(
                        v == hex.attrs.at(EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot()),
                        "HEX_REACHABLE_BY_ACTIVE_STACK: =%d but different corresponding FRIENDLY v=%d",
                        v, hex.attrs.at(EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot())
                    );
                    if (v > 0) {
                        hex.hexactmask.at(EI(HexAction::MOVE)) = true;

                        // For each neighouring hex (incl. special if we are 2-stack)
                        // if there's a creature we could attack => set hexactmask
                        for (auto &hexaction : getHexActionsFixedAttacker(hex.bhex, astack))
                            hex.hexactmask.at(EI(hexaction)) = true;
                    }
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_0:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(0), "HEX_REACHABLE_BY_FRIENDLY_STACK_0");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_1:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(1), "HEX_REACHABLE_BY_FRIENDLY_STACK_1");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_2:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(2), "HEX_REACHABLE_BY_FRIENDLY_STACK_2");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_3:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(3), "HEX_REACHABLE_BY_FRIENDLY_STACK_3");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_4:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(4), "HEX_REACHABLE_BY_FRIENDLY_STACK_4");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_5:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(5), "HEX_REACHABLE_BY_FRIENDLY_STACK_5");
                break; case A::HEX_REACHABLE_BY_FRIENDLY_STACK_6:
                    ensureReachableOrNA(bh, v, friendlyCStacks.at(6), "HEX_REACHABLE_BY_FRIENDLY_STACK_6");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_0:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(0), "HEX_REACHABLE_BY_ENEMY_STACK_0");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_1:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(1), "HEX_REACHABLE_BY_ENEMY_STACK_1");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_2:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(2), "HEX_REACHABLE_BY_ENEMY_STACK_2");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_3:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(3), "HEX_REACHABLE_BY_ENEMY_STACK_3");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_4:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(4), "HEX_REACHABLE_BY_ENEMY_STACK_4");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_5:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(5), "HEX_REACHABLE_BY_ENEMY_STACK_5");
                break; case A::HEX_REACHABLE_BY_ENEMY_STACK_6:
                    ensureReachableOrNA(bh, v, enemyCStacks.at(6), "HEX_REACHABLE_BY_ENEMY_STACK_6");
                break; case A::HEX_MELEEABLE_BY_ACTIVE_STACK:
                    ensureMeleeableOrNA(hex.bhex, v, astack, "HEX_MELEEABLE_BY_ACTIVE_STACK");

                    // Check meleeability is the same for the corresponding FRIENDLY_STACK_ attribute
                    expect(
                        v == hex.attrs.at(EI(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot()),
                        "HEX_MELEEABLE_BY_ACTIVE_STACK: =%d but different corresponding FRIENDLY v=%d",
                        v, hex.attrs.at(EI(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot())
                    );
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_0:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(0), "HEX_MELEEABLE_BY_FRIENDLY_STACK_0");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_1:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(1), "HEX_MELEEABLE_BY_FRIENDLY_STACK_1");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_2:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(2), "HEX_MELEEABLE_BY_FRIENDLY_STACK_2");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_3:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(3), "HEX_MELEEABLE_BY_FRIENDLY_STACK_3");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_4:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(4), "HEX_MELEEABLE_BY_FRIENDLY_STACK_4");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_5:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(5), "HEX_MELEEABLE_BY_FRIENDLY_STACK_5");
                break; case A::HEX_MELEEABLE_BY_FRIENDLY_STACK_6:
                    ensureMeleeableOrNA(hex.bhex, v, friendlyCStacks.at(6), "HEX_MELEEABLE_BY_FRIENDLY_STACK_6");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_0:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(0), "HEX_MELEEABLE_BY_ENEMY_STACK_0");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_1:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(1), "HEX_MELEEABLE_BY_ENEMY_STACK_1");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_2:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(2), "HEX_MELEEABLE_BY_ENEMY_STACK_2");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_3:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(3), "HEX_MELEEABLE_BY_ENEMY_STACK_3");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_4:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(4), "HEX_MELEEABLE_BY_ENEMY_STACK_4");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_5:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(5), "HEX_MELEEABLE_BY_ENEMY_STACK_5");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_6:
                    ensureMeleeableOrNA(hex.bhex, v, enemyCStacks.at(6), "HEX_MELEEABLE_BY_ENEMY_STACK_6");
                break; case A::HEX_SHOOTABLE_BY_ACTIVE_STACK:
                    ensureShootableOrNA(hex.bhex, v, astack, "HEX_SHOOTABLE_BY_ACTIVE_STACK");

                    // Check shootability is the same for the corresponding FRIENDLY_STACK_ attribute
                    expect(
                        v == hex.attrs.at(EI(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot()),
                        "HEX_SHOOTABLE_BY_ACTIVE_STACK: =%d but different corresponding FRIENDLY v=%d",
                        v, hex.attrs.at(EI(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_0) + astack->unitSlot())
                    );

                    if (cstack && !isFriendly && v > 0)
                        hex.hexactmask.at(EI(HexAction::SHOOT)) = true;
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_0:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(0), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_0");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_1:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(1), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_1");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_2:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(2), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_2");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_3:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(3), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_3");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_4:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(4), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_4");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_5:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(5), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_5");
                break; case A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_6:
                    ensureShootableOrNA(hex.bhex, v, friendlyCStacks.at(6), "HEX_SHOOTABLE_BY_FRIENDLY_STACK_6");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_0:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(0), "HEX_SHOOTABLE_BY_ENEMY_STACK_0");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_1:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(1), "HEX_SHOOTABLE_BY_ENEMY_STACK_1");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_2:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(2), "HEX_SHOOTABLE_BY_ENEMY_STACK_2");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_3:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(3), "HEX_SHOOTABLE_BY_ENEMY_STACK_3");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_4:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(4), "HEX_SHOOTABLE_BY_ENEMY_STACK_4");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_5:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(5), "HEX_SHOOTABLE_BY_ENEMY_STACK_5");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_6:
                    ensureShootableOrNA(hex.bhex, v, enemyCStacks.at(6), "HEX_SHOOTABLE_BY_ENEMY_STACK_6");
                break; case A::HEX_NEXT_TO_ACTIVE_STACK:
                    ensureNextToOrNA(hex.bhex, v, astack, "HEX_NEXT_TO_ACTIVE_STACK");
                    // Check nextto is the same for the corresponding FRIENDLY_STACK_ attribute
                    expect(
                        v == hex.attrs.at(EI(A::HEX_NEXT_TO_FRIENDLY_STACK_0) + astack->unitSlot()),
                        "HEX_NEXT_TO_ACTIVE_STACK: =%d but different corresponding FRIENDLY v=%d",
                        v, hex.attrs.at(EI(A::HEX_NEXT_TO_FRIENDLY_STACK_0) + astack->unitSlot())
                    );
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_0:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(0), "HEX_NEXT_TO_FRIENDLY_STACK_0");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_1:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(1), "HEX_NEXT_TO_FRIENDLY_STACK_1");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_2:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(2), "HEX_NEXT_TO_FRIENDLY_STACK_2");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_3:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(3), "HEX_NEXT_TO_FRIENDLY_STACK_3");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_4:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(4), "HEX_NEXT_TO_FRIENDLY_STACK_4");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_5:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(5), "HEX_NEXT_TO_FRIENDLY_STACK_5");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_6:
                    ensureNextToOrNA(hex.bhex, v, friendlyCStacks.at(6), "HEX_NEXT_TO_FRIENDLY_STACK_6");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_0:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(0), "HEX_NEXT_TO_ENEMY_STACK_0");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_1:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(1), "HEX_NEXT_TO_ENEMY_STACK_1");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_2:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(2), "HEX_NEXT_TO_ENEMY_STACK_2");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_3:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(3), "HEX_NEXT_TO_ENEMY_STACK_3");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_4:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(4), "HEX_NEXT_TO_ENEMY_STACK_4");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_5:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(5), "HEX_NEXT_TO_ENEMY_STACK_5");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_6:
                    ensureNextToOrNA(hex.bhex, v, enemyCStacks.at(6), "HEX_NEXT_TO_ENEMY_STACK_6");
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
                    ensureValueMatch(cstack, v, cstack->speed(), "STACK_SPEED");
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

                for (int i=0; i < EI(HexAction::count); i++)
                    expect(hex0.hexactmask.at(i) == hex.hexactmask.at(i), "mismatch: hex.hexactmask.at(%d)", i);
            }
        }

        expect(bf.astack == astack, "mismatch: astack");
    }

    // static
    std::string BAI::Render(
        const Export::Result &r,
        const std::shared_ptr<CBattleCallback> cb,
        const Battlefield &bf,  // verification only
        const Action *action,  // for displaying "last action"
        const std::vector<AttackLog> attackLogs // for displaying log
    ) {
        expect(r.stateUnencoded.size() == 165*EI(A::_count), "r.stateu.size %d != 165*%d", r.stateUnencoded.size(), EI(A::_count));

        auto reconstructed = Reconstruct(r, cb);
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
        if (astack->unitSide() == LIB_CLIENT::BattleSide::DEFENDER) {
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
                row << col1 << "#" << alog.attslot + 1 << nocol;
            else
                row << "\033[7m" << "FX" << nocol;

            row << " attacks ";
            row << col2 << "#" << alog.defslot + 1 << nocol;
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

        rows.emplace_back() << "  ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕";
        rows.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉", "₀"};

        for (int y=0; y < BF_YMAX; y++) {
            for (int x=0; x < BF_XMAX; x++) {
                auto sym = std::string("?");
                auto &hex = hexes.at(y).at(x);

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
                break; case HexState::FREE: {
                    if (hex.attr(A::HEX_REACHABLE_BY_ACTIVE_STACK) > 0) {
                        sym = "○";
                        for (int i=0; i<7; i++) {
                            if (hex.attrs.at(i+EI(A::HEX_REACHABLE_BY_ENEMY_STACK_0)) > 0) {
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
                break; case HexState::OCCUPIED: {
                    auto slot = hex.attr(A::STACK_SLOT);
                    auto friendly = hex.attr(A::STACK_SIDE) == cb->battleGetMySide();
                    auto col = friendly ? ourcol : enemycol;

                    if (hex.attr(A::STACK_IS_ACTIVE) > 0)
                        col += activemod;

                    stackhexes.at(friendly ? slot : 7+slot) = std::make_shared<Hex>(hex);
                    sym = col + std::to_string(slot+1) + nocol;
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
        rows.emplace_back() << "  ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕";

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
                    color = (hex->attr(A::STACK_SIDE) == BattleSide::ATTACKER) ? redcol : bluecol;
                    value = std::to_string(a == A::STACK_SLOT ? 1 + hex->attr(a) : hex->attr(a));
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
