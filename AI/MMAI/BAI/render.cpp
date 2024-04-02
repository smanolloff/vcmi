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
        auto state = r.state;
        auto hexes = Hexes();
        const CStack* astack; // XXX: can remain nullptr (for terminal obs)

        auto friendlyCStacks = std::array<const CStack*, 7>{};
        auto enemyCStacks = std::array<const CStack*, 7>{};
        auto rinfos = std::map<const CStack*, ReachabilityInfo>{};

        for (auto &cstack : cb->battleGetAllStacks()) {
            if (cstack->unitId() == cb->battleActiveUnit()->unitId())
                astack = cstack;

            cstack->unitSide() == cb->battleGetMySide()
                ? friendlyCStacks.at(cstack->unitSlot()) = cstack
                : enemyCStacks.at(cstack->unitSlot()) = cstack;

            rinfos.at(cstack) = cb->getReachability(cstack);
        }

        // Return (attr == N/A), but after performing some checks
        auto isNA = [](int v, const CStack* stack, const char* attrname) {
            if (v == Export::VALUE_NA) {
                expect(!stack, "%s: N/A but stack != nullptr", attrname);
                return true;
            }
            expect(stack, "%s: != N/A but stack = nullptr", attrname);
            return false;
        };

        auto ensureReachableOrNA = [=](BattleHex bh, int v, const CStack* stack, const char* attrname) {
            if (isNA(v, stack, attrname)) return;
            auto distance = rinfos.at(stack).distances.at(bh);
            expect(stack->speed() >= distance, "%s: =1 but speed=%d < distance=%d", attrname, distance);
        };

        // ihex = 0, 1, 2, .. 164
        // ibase = 0, 15, 30, ... 2460
        for (int ihex=0; ihex < BF_SIZE; ihex++) {
            int ibase = ihex * EI(A::_count);
            auto hex = Hex{};
            int x = ihex%15;
            int y = ihex/15;

            ASSERT(y == state.at(ibase).v, "hex.y mismatch");
            ASSERT(x == state.at(ibase+1).v, "hex.x mismatch");
            auto bh = BattleHex(x+1, y);
            hex.bhex = bh;
            hex.hexactmask.fill(false);

            for (int i=0; i < EI(A::_count); i++)
                hex.attrs.at(i) = state.at(ibase + i).v;

            const CStack *cstack;
            bool isActive = false;
            bool isFriendly = false;

            // Hex offsets for attacking at direction 0..12
            // Depends if the row is odd or even
            // XXX: order MUST correspond to the directions in hexaction.h
            auto getAttackDirs = [rinfos](Hex hex, const CStack* stack){
                auto bhid = Hex::CalcId(hex.bhex);
                auto offsets = (hex.getY() % 2 == 0)
                    ? std::array<int, 12>{-15, -14, 1, 16, 15, -1, 14, -2, -16, -13, 2, 17}
                    : std::array<int, 12>{-16, -15, 1, 15, 14, -1, 13, -2, -17, -14, 2, 16};
                auto ndirs = stack->doubleWide() ? 12 : 6;
                auto res = std::vector<int>{};
                for (int dir=0; dir<ndirs; dir++) {
                    auto nbhid = bhid + offsets.at(dir);
                    if (nbhid < 0 || nbhid > 164) continue;
                    auto nbhY = nbhid / 15;
                    auto nbhX = nbhid % 15;
                    auto nbh = BattleHex(nbhX+1, nbhY);
                    expect(nbh.isAvailable(), "nbh is not available");
                    if (rinfos.at(stack).distances.at(nbh) <= stack->speed())
                        res.push_back(dir);
                }
                return res;
            };

            auto isNeighbouring = [](BattleHex bh, const CStack* stack) {
                for (auto &nbh : stack->getSurroundingHexes(bh, false, 0))
                    if (stack->coversPos(nbh))
                        return true;
                return false;
            };

            if (hex.attr(A::STACK_QUANTITY) != Export::VALUE_NA) {
                // there's a stack on this hex
                isFriendly = hex.attr(A::STACK_SIDE) == cb->battleGetMySide();
                cstack = isFriendly ? friendlyCStacks.at(EI(A::STACK_SLOT)) : enemyCStacks.at(EI(A::STACK_SLOT));
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
            for (const auto &a : hex.attrs) {
                auto attr = A(a);
                auto v = hex.attrs.at(a);

                switch(attr) {
                break; case A::HEX_Y_COORD:
                    expect(a == y, "HEX_Y_COORD: %d != %d", a, y);
                break; case A::HEX_X_COORD:
                    expect(a == x, "HEX_X_COORD: %d != %d", a, x);
                break; case A::HEX_STATE:
                    switch (HexState(a)) {
                    break; case HexState::OBSTACLE:
                        expect(aa == EAccessibility::OBSTACLE, "HEX_STATE: OBSTACLE -> %d", aa);
                    break; case HexState::OCCUPIED:
                        expect(aa == EAccessibility::ALIVE_STACK, "HEX_STATE: OCCUPIED -> %d", aa);
                    break; case HexState::FREE:
                        expect(aa == EAccessibility::ACCESSIBLE, "HEX_STATE: FREE -> %d", aa);
                    break; default:
                        throw std::runtime_error("HEX_STATE: Unexpected HexState: " + std::to_string(a));
                    }
                break; case A::HEX_REACHABLE_BY_ACTIVE_STACK:
                    if (!astack) {
                        expect(v == Export::VALUE_NA, "HEX_REACHABLE_BY_ACTIVE_STACK: != N/A, but astack=nullptr");
                        break;
                    }

                    if (cstack && cstack->coversPos(bh))
                        expect(isActive, "HEX_REACHABLE_BY_ACTIVE_STACK: coversPos=true but isActive=false");
                    else
                        expect(hex.isFree(), "HEX_REACHABLE_BY_ACTIVE_STACK: hex.isFree()=false");
                    hex.hexactmask.at(EI(HexAction::MOVE)) = true;
                    expect(
                        hex.attrs.at(EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0) + astack->speed()) == 1,
                        "HEX_REACHABLE_BY_ACTIVE_STACK: corresponding FRIENDLY reachability != 1"
                    );
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
                    if (!astack) {
                        expect(v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ACTIVE_STACK: != N/A, but astack=nullptr");
                        break;
                    }

                    expect(getAttackDirs(hex, astack).size() > 0, "HEX_MELEEABLE_BY_ACTIVE_STACK: no attack dirs");
                    for (auto &dir : getAttackDirs(hex, astack))
                        hex.hexactmask.at(1+dir) = true;
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_0:
                    expect(enemyCStacks.at(0) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_0: != N/A, but enemyCStacks.at(0)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(0)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_0: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_1:
                    expect(enemyCStacks.at(1) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_1: != N/A, but enemyCStacks.at(1)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(1)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_1: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_2:
                    expect(enemyCStacks.at(2) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_2: != N/A, but enemyCStacks.at(2)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(2)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_2: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_3:
                    expect(enemyCStacks.at(3) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_3: != N/A, but enemyCStacks.at(3)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(3)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_3: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_4:
                    expect(enemyCStacks.at(4) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_4: != N/A, but enemyCStacks.at(4)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(4)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_4: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_5:
                    expect(enemyCStacks.at(5) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_5: != N/A, but enemyCStacks.at(5)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(5)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_5: no attack dirs");
                break; case A::HEX_MELEEABLE_BY_ENEMY_STACK_6:
                    expect(enemyCStacks.at(6) || v == Export::VALUE_NA, "HEX_MELEEABLE_BY_ENEMY_STACK_6: != N/A, but enemyCStacks.at(6)=nullptr");
                    expect(getAttackDirs(hex, enemyCStacks.at(6)).size() > 0, "HEX_MELEEABLE_BY_ENEMY_STACK_6: no attack dirs");
                break; case A::HEX_SHOOTABLE_BY_ACTIVE_STACK:
                    if (!astack) {
                        expect(v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ACTIVE_STACK: != N/A, but astack=nullptr");
                        break;
                    }
                    expect(cb->battleCanShoot(astack), "HEX_SHOOTABLE_BY_ACTIVE_STACK: astack can't shoot");
                    hex.hexactmask.at(EI(HexAction::SHOOT)) = true;
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_0:
                    expect(enemyCStacks.at(0) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_0: != N/A, but enemyCStacks.at(0)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(0)), "HEX_SHOOTABLE_BY_ENEMY_STACK_0: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_1:
                    expect(enemyCStacks.at(1) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_1: != N/A, but enemyCStacks.at(1)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(1)), "HEX_SHOOTABLE_BY_ENEMY_STACK_1: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_2:
                    expect(enemyCStacks.at(2) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_2: != N/A, but enemyCStacks.at(2)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(2)), "HEX_SHOOTABLE_BY_ENEMY_STACK_2: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_3:
                    expect(enemyCStacks.at(3) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_3: != N/A, but enemyCStacks.at(3)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(3)), "HEX_SHOOTABLE_BY_ENEMY_STACK_3: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_4:
                    expect(enemyCStacks.at(4) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_4: != N/A, but enemyCStacks.at(4)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(4)), "HEX_SHOOTABLE_BY_ENEMY_STACK_4: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_5:
                    expect(enemyCStacks.at(5) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_5: != N/A, but enemyCStacks.at(5)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(5)), "HEX_SHOOTABLE_BY_ENEMY_STACK_5: stack can't shoot");
                break; case A::HEX_SHOOTABLE_BY_ENEMY_STACK_6:
                    expect(enemyCStacks.at(6) || v == Export::VALUE_NA, "HEX_SHOOTABLE_BY_ENEMY_STACK_6: != N/A, but enemyCStacks.at(6)=nullptr");
                    expect(cb->battleCanShoot(enemyCStacks.at(6)), "HEX_SHOOTABLE_BY_ENEMY_STACK_6: stack can't shoot");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_0:
                    expect(friendlyCStacks.at(0) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_0: != N/A, but friendlyCStacks.at(0)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(0)), "HEX_NEXT_TO_FRIENDLY_STACK_0: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_1:
                    expect(friendlyCStacks.at(1) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_1: != N/A, but friendlyCStacks.at(1)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(1)), "HEX_NEXT_TO_FRIENDLY_STACK_1: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_2:
                    expect(friendlyCStacks.at(2) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_2: != N/A, but friendlyCStacks.at(2)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(2)), "HEX_NEXT_TO_FRIENDLY_STACK_2: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_3:
                    expect(friendlyCStacks.at(3) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_3: != N/A, but friendlyCStacks.at(3)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(3)), "HEX_NEXT_TO_FRIENDLY_STACK_3: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_4:
                    expect(friendlyCStacks.at(4) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_4: != N/A, but friendlyCStacks.at(4)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(4)), "HEX_NEXT_TO_FRIENDLY_STACK_4: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_5:
                    expect(friendlyCStacks.at(5) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_5: != N/A, but friendlyCStacks.at(5)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(5)), "HEX_NEXT_TO_FRIENDLY_STACK_5: not neighbouring");
                break; case A::HEX_NEXT_TO_FRIENDLY_STACK_6:
                    expect(friendlyCStacks.at(6) || v == Export::VALUE_NA, "HEX_NEXT_TO_FRIENDLY_STACK_6: != N/A, but friendlyCStacks.at(6)=nullptr");
                    expect(isNeighbouring(hex.bhex, friendlyCStacks.at(6)), "HEX_NEXT_TO_FRIENDLY_STACK_6: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_0:
                    expect(enemyCStacks.at(0) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_0: != N/A, but enemyCStacks.at(0)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(0)), "HEX_NEXT_TO_ENEMY_STACK_0: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_1:
                    expect(enemyCStacks.at(1) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_1: != N/A, but enemyCStacks.at(1)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(1)), "HEX_NEXT_TO_ENEMY_STACK_1: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_2:
                    expect(enemyCStacks.at(2) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_2: != N/A, but enemyCStacks.at(2)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(2)), "HEX_NEXT_TO_ENEMY_STACK_2: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_3:
                    expect(enemyCStacks.at(3) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_3: != N/A, but enemyCStacks.at(3)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(3)), "HEX_NEXT_TO_ENEMY_STACK_3: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_4:
                    expect(enemyCStacks.at(4) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_4: != N/A, but enemyCStacks.at(4)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(4)), "HEX_NEXT_TO_ENEMY_STACK_4: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_5:
                    expect(enemyCStacks.at(5) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_5: != N/A, but enemyCStacks.at(5)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(5)), "HEX_NEXT_TO_ENEMY_STACK_5: not neighbouring");
                break; case A::HEX_NEXT_TO_ENEMY_STACK_6:
                    expect(enemyCStacks.at(6) || v == Export::VALUE_NA, "HEX_NEXT_TO_ENEMY_STACK_6: != N/A, but enemyCStacks.at(6)=nullptr");
                    expect(isNeighbouring(hex.bhex, enemyCStacks.at(6)), "HEX_NEXT_TO_ENEMY_STACK_6: not neighbouring");
                break; case A::STACK_QUANTITY:
                    if (isNA(v, cstack, "STACK_QUANTITY")) break;
                    expect(v == cstack->getCount(), "STACK_QUANTITY: %d != %d", v, cstack->getCount());
                break; case A::STACK_ATTACK:
                    if (isNA(v, cstack, "STACK_ATTACK")) break;
                    expect(v == cstack->getAttack(false), "STACK_ATTACK: %d != %d", v, cstack->getAttack(false));
                break; case A::STACK_DEFENSE:
                    if (isNA(v, cstack, "STACK_DEFENSE")) break;
                    expect(v == cstack->getDefense(false), "STACK_DEFENSE: %d != %d", v, cstack->getDefense(false));
                break; case A::STACK_SHOTS:
                    if (isNA(v, cstack, "STACK_SHOTS")) break;
                    expect(v == cstack->shots.available(), "STACK_SHOTS: %d != %d", v, cstack->shots.available());
                break; case A::STACK_DMG_MIN:
                    if (isNA(v, cstack, "STACK_DMG_MIN")) break;
                    expect(v == cstack->getMinDamage(false), "STACK_DMG_MIN: %d != %d", v, cstack->getMinDamage(false));
                break; case A::STACK_DMG_MAX:
                    if (isNA(v, cstack, "STACK_DMG_MAX")) break;
                    expect(v == cstack->getMaxDamage(false), "STACK_DMG_MAX: %d != %d", v, cstack->getMaxDamage(false));
                break; case A::STACK_HP:
                    if (isNA(v, cstack, "STACK_HP")) break;
                    expect(v == cstack->getMaxHealth(), "STACK_HP: %d != %d", v, cstack->getMaxHealth());
                break; case A::STACK_HP_LEFT:
                    if (isNA(v, cstack, "STACK_HP_LEFT")) break;
                    expect(v == cstack->getFirstHPleft(), "STACK_HP_LEFT: %d != %d", v, cstack->getFirstHPleft());
                break; case A::STACK_SPEED:
                    if (isNA(v, cstack, "STACK_SPEED")) break;
                    expect(v == cstack->speed(), "STACK_SPEED: %d != %d", v, cstack->speed());
                break; case A::STACK_WAITED:
                    if (isNA(v, cstack, "STACK_WAITED")) break;
                    expect(v == cstack->waitedThisTurn, "STACK_WAITED: %d != %d", v, cstack->waitedThisTurn);
                break; case A::STACK_QUEUE_POS:
                    if (isNA(v, cstack, "STACK_QUEUE_POS")) break;
                    if (v == 0)
                        expect(isActive, "STACK_QUEUE_POS: =0 but isActive=false");
                    else
                        expect(!isActive, "STACK_QUEUE_POS: !=0 but isActive=true");
                break; case A::STACK_RETALIATIONS_LEFT:
                    if (isNA(v, cstack, "STACK_RETALIATIONS_LEFT")) break;
                    // not verifying unlimited retals, just check 0
                    if (v > 0)
                        expect(cstack->ableToRetaliate(), "STACK_RETALIATIONS_LEFT: >0 but ableToRetaliate=false");
                    else
                        expect(!cstack->ableToRetaliate(), "STACK_RETALIATIONS_LEFT: =0 but ableToRetaliate=true");
                break; case A::STACK_SIDE:
                    if (isNA(v, cstack, "STACK_SIDE")) break;
                    expect(v == cstack->unitSide(), "STACK_SIDE: %d != %d", v, cstack->unitSide());
                break; case A::STACK_SLOT:
                    if (isNA(v, cstack, "STACK_SLOT")) break;
                    expect(v == cstack->unitSlot(), "STACK_SLOT: %d != %d", v, int(cstack->unitSlot()));
                break; case A::STACK_CREATURE_TYPE:
                    if (isNA(v, cstack, "STACK_CREATURE_TYPE")) break;
                    expect(v == cstack->creatureId(), "STACK_CREATURE_TYPE: %d != %d", v, cstack->creatureId());
                break; case A::STACK_AI_VALUE_TENTH:
                    if (isNA(v, cstack, "STACK_AI_VALUE_TENTH")) break;
                    expect(v == cstack->creatureId().toCreature()->getAIValue(), "STACK_AI_VALUE_TENTH: %d != %d", v, cstack->creatureId().toCreature()->getAIValue());
                break; case A::STACK_IS_ACTIVE:
                    if (isNA(v, cstack, "STACK_IS_ACTIVE")) break;
                    expect(v == isActive, "STACK_IS_ACTIVE: %d != %d", v, isActive);
                break; case A::STACK_FLYING:
                    if (isNA(v, cstack, "STACK_FLYING")) break;
                    expect(v == cstack->hasBonusOfType(BonusType::FLYING), "STACK_FLYING: %d != %d", v, cstack->hasBonusOfType(BonusType::FLYING));
                break; case A::STACK_NO_MELEE_PENALTY:
                    if (isNA(v, cstack, "STACK_NO_MELEE_PENALTY")) break;
                    expect(v == cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "STACK_NO_MELEE_PENALTY: %d != %d", v, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY));
                break; case A::STACK_TWO_HEX_ATTACK_BREATH:
                    if (isNA(v, cstack, "STACK_TWO_HEX_ATTACK_BREATH")) break;
                    expect(v == cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "STACK_TWO_HEX_ATTACK_BREATH: %d != %d", v, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH));
                break; case A::STACK_BLOCKS_RETALIATION:
                    if (isNA(v, cstack, "STACK_BLOCKS_RETALIATION")) break;
                    expect(v == cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "STACK_BLOCKS_RETALIATION: %d != %d", v, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION));
                break; case A::STACK_DEFENSIVE_STANCE:
                    if (isNA(v, cstack, "STACK_DEFENSIVE_STANCE")) break;
                    expect(v == cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE), "STACK_DEFENSIVE_STANCE: %d != %d", v, cstack->hasBonusOfType(BonusType::DEFENSIVE_STANCE));
                break; default:
                    throw std::runtime_error("Unexpected attr: " + std::to_string(a));
                }
            }

            hex.cstack = cstack;
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
        const Battlefield &bf,  // verification only
        const Action *action,  // for displaying "last action"
        const std::vector<AttackLog> attackLogs // for displaying log
    ) {
        expect(r.state.size() == 165*EI(A::_count), "r.state.size %d != 165*%d", r.state.size(), EI(A::_count));

        int encsize = 0;
        for (auto &oh : r.state)
            encsize += oh.n;

        expect(encsize == Export::STATE_SIZE, "encsize: %d != %d", encsize, Export::STATE_SIZE);

        auto reconstructed = Reconstruct(r, cb);
        auto hexes = std::get<0>(reconstructed);
        auto astack = std::get<1>(reconstructed);

        Verify(bf, hexes, astack);

        auto state = r.state;

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
                //     state.begin()+ibase+3+EI(StackAttr::count)+14,
                //     state.begin()+ibase+3+EI(StackAttr::count)+14 + 21,
                //     [](Export::NValue nv) { return nv.orig != 0; }
                // );

                switch(hex.getState()) {
                break; case HexState::FREE: {
                    if (hex.attr(A::HEX_REACHABLE_BY_ACTIVE_STACK)) {
                        sym = "○";
                        for (int i=0; i<7; i++) {
                            if (hex.attrs.at(i+EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_0))) {
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

                    if (hex.attr(A::STACK_IS_ACTIVE))
                        col += activemod;

                    stackhexes.at(friendly ? slot : slot*2) = std::make_shared<Hex>(hex);
                    sym = col + std::to_string(slot+1) + nocol;
                }
                break; default:
                    throw std::runtime_error("unexpected hex.state: " + std::to_string(EI(hex.getState())));
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

                if (hex->cstack) {
                    color = (hex->attr(A::STACK_SIDE) == BattleSide::ATTACKER) ? redcol : bluecol;
                    value = std::to_string(hex->attr(a));
                    if (hex->attr(A::STACK_IS_ACTIVE)) color += activemod;
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
