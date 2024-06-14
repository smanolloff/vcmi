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

#include <bitset>
#include <memory>
#include <stdexcept>
#include <limits>

#include "common.h"
#include "schema/schema.h"
#include "schema/v1/types.h"
#include "schema/v1/constants.h"
#include "BAI/v1/hexaction.h"
#include "BAI/v1/hexactmask.h"
#include "BAI/v1/battlefield.h"


namespace MMAI::BAI::V1 {
    using D = BattleHex::EDir;

    // static
    BattleHex Battlefield::AMoveTarget(BattleHex &bh, HexAction &action) {
        if (action == HexAction::MOVE || action == HexAction::SHOOT)
            throw std::runtime_error("MOVE and SHOOT are not AMOVE actions");

        auto edir = AMOVE_TO_EDIR.at(action);
        auto nbh = bh.cloneInDirection(edir);

        switch (action) {
        case HexAction::AMOVE_TR:
        case HexAction::AMOVE_R:
        case HexAction::AMOVE_BR:
        case HexAction::AMOVE_BL:
        case HexAction::AMOVE_L:
        case HexAction::AMOVE_TL:
            break;
        case HexAction::AMOVE_2TR:
        case HexAction::AMOVE_2R:
        case HexAction::AMOVE_2BR:
            nbh = nbh.cloneInDirection(BattleHex::EDir::RIGHT);
            break;
        case HexAction::AMOVE_2BL:
        case HexAction::AMOVE_2L:
        case HexAction::AMOVE_2TL:
            nbh = nbh.cloneInDirection(BattleHex::EDir::LEFT);
            break;
        default:
            throw std::runtime_error("Unexpected action: " + std::to_string(EI(action)));
        }

        ASSERT(nbh.isAvailable(), "unavailable AMOVE target hex #" + std::to_string(nbh.hex));
        return nbh;
    }

    // static
    bool Battlefield::IsReachable(
        const BattleHex &bh,
        const StackInfo &stackinfo
    ) {
        //
        // XXX: don't rely on ReachabilityInfo's `isReachable` as it
        //      returns true even if speed is insufficient
        //
        // distances is 0 for the stack's main hex, 1 for its "back" hex
        // (100000 if it can't fit there)
        return stackinfo.rinfo->distances.at(bh) <= stackinfo.speed;
    }

    //
    // Return bh's neighbouring hexes for setting HEX_RELPOS_TO_* attrs
    //
    // return nearby hexes for "X":
    //
    //  . . . . . . . . . .
    // . . .11 5 0 6 . . .
    //  . .10 4 X 1 7 . . .
    // . . . 9 3 2 8 . . .
    //  . . . . . . . . . .
    //
    // NOTE:
    // The index of each hex in the returned array corresponds to a
    // the respective AMOVE_* HexAction w.r.t. "X" (see hexaction.h)
    //
    HexActionHex Battlefield::NearbyHexes(BattleHex &bh) {
        static_assert(EI(HexAction::AMOVE_TR) == 0);
        static_assert(EI(HexAction::AMOVE_R) == 1);
        static_assert(EI(HexAction::AMOVE_BR) == 2);
        static_assert(EI(HexAction::AMOVE_BL) == 3);
        static_assert(EI(HexAction::AMOVE_L) == 4);
        static_assert(EI(HexAction::AMOVE_TL) == 5);
        static_assert(EI(HexAction::AMOVE_2TR) == 6);
        static_assert(EI(HexAction::AMOVE_2R) == 7);
        static_assert(EI(HexAction::AMOVE_2BR) == 8);
        static_assert(EI(HexAction::AMOVE_2BL) == 9);
        static_assert(EI(HexAction::AMOVE_2L) == 10);
        static_assert(EI(HexAction::AMOVE_2TL) == 11);

        auto nbhR = bh.cloneInDirection(D::RIGHT, false);
        auto nbhL = bh.cloneInDirection(D::LEFT, false);

        return HexActionHex{
            // The 6 basic directions
            bh.cloneInDirection(D::TOP_RIGHT, false),
            nbhR,
            bh.cloneInDirection(D::BOTTOM_RIGHT, false),
            bh.cloneInDirection(D::BOTTOM_LEFT, false),
            nbhL,
            bh.cloneInDirection(D::TOP_LEFT, false),

            // Extended directions for R-side wide creatures
            nbhR.cloneInDirection(D::TOP_RIGHT, false),
            nbhR.cloneInDirection(D::RIGHT, false),
            nbhR.cloneInDirection(D::BOTTOM_RIGHT, false),

            // Extended directions for L-side wide creatures
            nbhL.cloneInDirection(D::BOTTOM_LEFT, false),
            nbhL.cloneInDirection(D::LEFT, false),
            nbhL.cloneInDirection(D::TOP_LEFT, false)
        };
    }

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    std::unique_ptr<Hex> Battlefield::InitHex(
        const int id,
        const CStack* astack,
        const int percentValue,
        const Queue &queue,
        const AccessibilityInfo &ainfo, // accessibility info for active stack
        const StackInfos &stackinfos,
        const HexStacks &hexstacks
    ) {
        int x = id % BF_XMAX;
        int y = id / BF_XMAX;

        auto bh = BattleHex(x+1, y);
        expect(Hex::CalcId(bh) == id, "calcId mismatch: %d != %d", Hex::CalcId(bh), id);

        auto hex = std::make_unique<Hex>();
        hex->bhex = bh;
        hex->setPercentCurToStartTotalValue(percentValue);
        hex->setX(x);
        hex->setY(y);

        switch(ainfo.at(bh.hex)) {
        break; case EAccessibility::ACCESSIBLE: hex->setState(HexState::FREE);
        break; case EAccessibility::OBSTACLE: hex->setState(HexState::OBSTACLE);
        break; case EAccessibility::ALIVE_STACK: hex->setState(HexState::OCCUPIED);
        // XXX: unhandled hex states
        // case EAccessibility::DESTRUCTIBLE_WALL:
        // case EAccessibility::GATE:
        // case EAccessibility::UNAVAILABLE:
        // case EAccessibility::SIDE_COLUMN:
        break; default:
          throw std::runtime_error(
            "Unexpected hex accessibility for hex "+ std::to_string(bh.hex) + ": "
              + std::to_string(static_cast<int>(ainfo.at(bh.hex)))
          );
        }

        //
        //
        //

        const CStack* h_cstack = nullptr;
        auto it = hexstacks.find(hex->bhex);
        if (it != hexstacks.end()) {
            h_cstack = it->second;
            auto qit = std::find(queue.begin(), queue.end(), h_cstack->unitId());
            auto qpos = (qit == queue.end()) ? QSIZE-1 : qit - queue.begin();
            hex->setCStackAndAttrs(h_cstack, qpos);
        }

        auto meleeDistanceFromStackResets = std::array<std::bitset<7>, 2> {};
        auto maybeResetDistanceFromStack = [&hex, &meleeDistanceFromStackResets](bool isActive, bool isRight, int slot) {
            if (meleeDistanceFromStackResets.at(isRight).test(slot))
                return;

            hex->setMeleeDistanceFromStack(isActive, isRight, slot, MeleeDistance::NA);
            meleeDistanceFromStackResets.at(isRight).set(slot);
        };

        const auto& nbhexes = NearbyHexes(hex->bhex);

        for (const auto& [cstack, stackinfo] : stackinfos) {
            auto isActive = cstack == astack;
            auto isRight = bool(cstack->unitSide());  // 1 = right = true
            auto slot = cstack->unitSlot();

            // Stack exists => set default values to 0 instead of N/A
            // Some of those values them will be updated to true later
            hex->setMeleeableByStack(isActive, isRight, slot, DmgMod::ZERO);
            hex->setShootDistanceFromStack(isActive, isRight, slot, ShootDistance::NA);

            // XXX: MELEE_DISTANCE_FROM for THIS cstack may have been set in
            //      a previous iteration (previous cstack) for an n_cstack
            maybeResetDistanceFromStack(isActive, isRight, slot);

            if (stackinfo.canshoot) {
                // stack can shoot (not blocked & has ammo)
                // => calculate the dmg mod if the stack were to shoot at Hex
                //
                // XXX: If Hex is at distance 11, but is the "2nd" hex an enemy,
                //      shooting at it would do FULL dmg. This can be ignored
                //      as the shooter can simply shoot at the "1st" hex (which
                //      would be at range 10) with the same result. The
                //      The exceptions here are Magogs and Liches, where AoE
                //      plays a role, but that is an edge case.
                //      => when calculating, pretend that the enemy is 1-hex.
                //
                // XXX: If shooter is wide, has walked to the enemy side of the
                //      battlefield and is at distance=11 from Hex (i.e. its)
                //      "2nd" is at distance=10 from Hex), shooting at Hex
                //      would do FULL dmg. But that's also an edge case.
                //      => when calculating, pretend that the shooter is 1-hex.
                //
                auto dist = (stackinfo.noDistancePenalty || BattleHex::getDistance(cstack->getPosition(), bh) <= 10)
                    ? ShootDistance::NEAR
                    : ShootDistance::FAR;

                hex->setShootDistanceFromStack(isActive, isRight, slot, dist);

                if (h_cstack && h_cstack->unitSide() != cstack->unitSide())
                    hex->setActionForStack(isActive, isRight, slot, HexAction::SHOOT);
            }

            auto isReachable = IsReachable(hex->bhex, stackinfo);
            if (isReachable)
                hex->setActionForStack(isActive, isRight, slot, HexAction::MOVE);

            // once we've identified that cstack can attack hex from n_bhex,
            // there's no need to check the remaining n_bhexes
            bool meleeableAlreadySetForStack = false;

            // Special case: a stack is covering both hexes at "2" and "1"
            // We must set "2" here (NEAR):
            // . . 2 1 . .
            //  . X ~ . .
            // => iterate hexactions in reverse, so "FAR" are processed first

            for (int i=nbhexes.size()-1; i>=0; i--) {
                auto &n_bhex = nbhexes.at(i);
                if (!n_bhex.isAvailable())
                    continue;

                auto hexaction = HexAction(i);

                if (!meleeableAlreadySetForStack && IsReachable(n_bhex, stackinfo)) {
                    if (hexaction <= HexAction::AMOVE_TL) {
                        if (cstack->doubleWide() && ((hexaction == HexAction::AMOVE_L && isRight) || (hexaction == HexAction::AMOVE_R && !isRight))) {
                            continue;
                        } else {
                            hex->setMeleeableByStack(isActive, isRight, slot, stackinfo.meleemod);
                            meleeableAlreadySetForStack = true;
                        }
                    } else if (hexaction <= HexAction::AMOVE_2BR) { // hexaction is 2TR/2R/2BR
                        // XXX: don't combine the nested ifs
                        // Perspective is mirrored: attack is n_bhex->bhex
                        // => actual attack action is 2BL/2L/2TL
                        // only wide L stacks can perform such an action
                        if (!isRight && cstack->doubleWide()) {
                            hex->setMeleeableByStack(isActive, false, slot, stackinfo.meleemod);
                            meleeableAlreadySetForStack = true;
                        }
                    } else { // hexaction is 2BL/2L/2TL
                        // actual attack action is mirrored: 2TR/2R/2BR
                        // only wide R stacks can perform such an action
                        if (isRight && cstack->doubleWide()) {
                            hex->setMeleeableByStack(isActive, true, slot, stackinfo.meleemod);
                            meleeableAlreadySetForStack = true;
                        }
                    }
                }

                auto it = hexstacks.find(n_bhex);
                if (it == hexstacks.end())
                    continue;

                auto &n_cstack = it->second;
                auto n_isActive = n_cstack == astack;
                auto n_isRight = bool(n_cstack->unitSide());  // 1 = right = true
                auto n_slot = n_cstack->unitSlot();

                if (isReachable && cstack->unitSide() != n_cstack->unitSide()) {
                    maybeResetDistanceFromStack(n_isActive, n_isRight, n_slot);

                    if (hexaction <= HexAction::AMOVE_TL) {
                        hex->setMeleeDistanceFromStack(n_isActive, n_isRight, n_slot, MeleeDistance::NEAR);
                        //ASSERT(cstack->isMeleeAttackPossible(cstack, n_cstack, hex->bhex), "vcmi says melee attack is IMPOSSIBLE");
                        hex->setActionForStack(isActive, isRight, slot, hexaction);
                    } else if (hexaction <= HexAction::AMOVE_2BR) {
                        // only wide R stacks can perform 2TR/2R/2BR attacks
                        if (isRight && cstack->doubleWide()) {
                            hex->setMeleeDistanceFromStack(n_isActive, false, n_slot, MeleeDistance::FAR);
                            //ASSERT(cstack->isMeleeAttackPossible(cstack, n_cstack, hex->bhex), "vcmi says melee attack is IMPOSSIBLE");
                            hex->setActionForStack(isActive, isRight, slot, hexaction);
                        }
                    } else {
                        // only wide L stacks can perform 2TL/2L/2BL attacks
                        if (!isRight && cstack->doubleWide()) {
                            hex->setMeleeDistanceFromStack(n_isActive, true, n_slot, MeleeDistance::FAR);
                            //ASSERT(cstack->isMeleeAttackPossible(cstack, n_cstack, hex->bhex), "vcmi says melee attack is IMPOSSIBLE");
                            hex->setActionForStack(isActive, isRight, slot, hexaction);
                        }
                    }
                }
            }

            expect(meleeDistanceFromStackResets.at(isRight).test(slot), "uninitialized MELEE_DISTANCE_FROM_* attributes");
            hex->finalizeActionMaskForStack(isActive, isRight, slot);
        }

        return hex;
    }

    Battlefield::Battlefield(const CPlayerBattleCallback* battle, const CStack* astack_, int percentValue, bool isMorale)
    : astack(astack_),
      hexes(InitHexes(battle, astack_, percentValue, isMorale))
      {};

    // static
    // result is a vector<UnitID>
    // XXX: there is a bug in VCMI when high morale occurs:
    //      - the stack acts as if it's already the next unit's turn
    //      - as a result, QueuePos for the ACTIVE stack is non-0
    //        while the QueuePos for the next (non-active) stack is 0
    // (this applies only to good morale; bad morale simply skips turn)
    // As a workaround, a "isMorale" flag is passed whenever the astack is
    // acting because of high morale and queue is "shifted" accordingly.
    Queue Battlefield::GetQueue(const CPlayerBattleCallback* battle, const CStack* astack, bool isMorale) {
        auto res = Queue{};

        auto tmp = std::vector<battle::Units>{};
        battle->battleGetTurnOrder(tmp, QSIZE, 0);
        for (auto &units : tmp)
            for (auto &unit : units)
                res.insert(res.end(), unit->unitId());

        if (isMorale) {
            assert(astack);
            std::rotate(res.rbegin(), res.rbegin() + 1, res.rend());
            res.at(0) = astack->unitId();
        } else {
            assert(astack == nullptr || res.at(0) == astack->unitId());
        }

        return res;
    }

    // static
    Hexes Battlefield::InitHexes(const CPlayerBattleCallback* battle, const CStack* astack, int percentValue, bool isMorale) {
        auto res = Hexes{};
        auto ainfo = battle->getAccesibility();
        auto hexstacks = HexStacks{};  // expensive check for blocked shooters => eager load once
        auto stackinfos = StackInfos{}; // expensive check for stack->speed, isblocked and getReachability

        for (const auto& cstack : battle->battleGetStacks()) {
            stackinfos.insert({cstack, StackInfo(
                cstack->getMovementRange(),
                // cstack->canShoot() && (cstack->hasBonusOfType(BonusType::FREE_SHOOTING) || !cb->battleIsUnitBlocked(cstack))
                battle->battleCanShoot(cstack),
                (cstack->isShooter() && !cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY) ? DmgMod::HALF : DmgMod::FULL),
                cstack->hasBonusOfType(BonusType::NO_DISTANCE_PENALTY),
                std::make_shared<ReachabilityInfo>(battle->getReachability(cstack))
            )});

            for (auto bh : cstack->getHexes())
                hexstacks.insert({bh, cstack});
        }

        auto queue = GetQueue(battle, astack, isMorale);

        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, astack, percentValue, queue, ainfo, stackinfos, hexstacks);
            expect(hex->getX() + hex->getY()*BF_XMAX == i, "hex->x + hex->y*BF_XMAX != i: %d + %d*%d != %d", hex->getX(), hex->getY(), BF_XMAX, i);
            res.at(hex->getY()).at(hex->getX()) = std::move(hex);
        }

        ASSERT(queue.size() == QSIZE, "queue size: " + std::to_string(queue.size()));

        return res;
    };

    /**
     * Update state between turns:
     * - used if expecting a battle end (for terminal state/render)
     * - no "reachable" hexes (off-turn means no active stack)
     */
    void Battlefield::offTurnUpdate(CPlayerBattleCallback* battle) {
        auto ainfo = battle->getAccesibility();
        auto queue = GetQueue(battle, nullptr, false);

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                switch(ainfo.at(hex->bhex.hex)) {
                break; case EAccessibility::ACCESSIBLE:
                    hex->setState(HexState::FREE);
                break; case EAccessibility::OBSTACLE:
                    hex->setState(HexState::OBSTACLE);
                break; case EAccessibility::ALIVE_STACK: {
                    hex->setState(HexState::OCCUPIED);

                    // nothing else needed here: no active stack
                    // => no special move case
                    // => no action mask
                }
                break; default:
                  throw std::runtime_error(
                    "Unexpected hex accessibility for hex "+ std::to_string(hex->bhex.hex) + ": "
                      + std::to_string(static_cast<int>(ainfo.at(hex->bhex.hex)))
                  );
                }
            }
        }
    }
}
