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

#include "types/battlefield.h"
#include "AI/VCAI/AIUtility.h"
#include "CCallback.h"
#include "CStack.h"
#include "battle/AccessibilityInfo.h"
#include "battle/ReachabilityInfo.h"
#include "battle/Unit.h"
#include "bonuses/BonusEnum.h"
#include "common.h"
#include "export.h"
#include "types/hexaction.h"
#include "types/hexactmask.h"
#include <memory>
#include <stdexcept>

namespace MMAI {
    using Attr = Export::Attribute;

    // static
    BattleHex Battlefield::AMoveTarget(BattleHex &bh, HexAction &action) {
        if (action == HexAction::MOVE || action == HexAction::SHOOT)
            throw std::runtime_error("MOVE and SHOOT are not AMOVE actions");

        auto edir = AMOVE_TO_EDIR.at(action);
        auto nbh = bh.cloneInDirection(edir);

        switch (action) {
        case HexAction::AMOVE_TL:
        case HexAction::AMOVE_TR:
        case HexAction::AMOVE_R:
        case HexAction::AMOVE_BR:
        case HexAction::AMOVE_BL:
        case HexAction::AMOVE_L:
            break;
        case HexAction::AMOVE_2BL:
        case HexAction::AMOVE_2L:
        case HexAction::AMOVE_2TL:
            nbh = nbh.cloneInDirection(BattleHex::EDir::LEFT);
            break;
        case HexAction::AMOVE_2TR:
        case HexAction::AMOVE_2R:
        case HexAction::AMOVE_2BR:
            nbh = nbh.cloneInDirection(BattleHex::EDir::RIGHT);
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
        const CStack* cstack,
        const ReachabilityInfos &rinfos
    ) {
        // isReachable may return true even if speed is insufficient
        // distances is 0 for the stack's main hex, 1 for its "back" hex
        // (100000 if it can't fit there)
        return rinfos.at(cstack)->distances.at(bh) <= cstack->speed();
    }

    // static
    HexAction Battlefield::AttackAction(
        const BattleHex &nbh, // hex to attack
        const BattleHex &bh,  // hex attacking from (pos)
        const BattleHex &bh2  // 2nd hex attacking from (2-hex stacks only)
    ) {
        auto mutualpos = BattleHex::mutualPosition(bh, nbh);

        if (mutualpos != BattleHex::EDir::NONE)
            return EDIR_TO_AMOVE.at(mutualpos);

        // "indirect" neighbouring hex (astack is double-wide)
        ASSERT(bh2 != BattleHex::INVALID, "bh2 is INVALID");
        mutualpos = BattleHex::mutualPosition(bh2, nbh);
        return EDIR_TO_AMOVE_2.at(mutualpos);
    }

    // static
    // If we were to move to this hex, check each neighbouring hex for:
    //  - occupying stack (friendly or enemy)
    //  - potential attackers (enemy stacks that can reach and attack from those hexes)
    //
    // NOTE: If we are a 2-hex stack, if we were to move to "hex", the
    //       proper (i.e. 8 instead of 6) hexes are considered "neighbouring"
    void Battlefield::ProcessNeighbouringHexes(
        Hex &hex,
        const CStack* astack,
        const std::vector<const CStack*> &allstacks,
        const AccessibilityInfo &ainfo, // accessibility info for active stack
        const ReachabilityInfos &rinfos
    ) {
        for (const auto &nbh : astack->getSurroundingHexes(hex.bhex)) {
            auto obh = astack->occupiedHex(hex.bhex); // =INVALID if astack is 1hex
            ASSERT(nbh != obh, "nbh == obh");

            for (const auto &cstack : allstacks) {
                auto isEnemy = (cstack->getOwner() != astack->getOwner());
                auto isActive = (cstack != astack);
                auto x = rinfos.at(cstack);
                auto alreadyOnNBH = cstack->coversPos(nbh);

                // XXX: can be already there, but unreachable (see note in IsReachable)
                if (IsReachable(nbh, cstack, rinfos) || alreadyOnNBH) {
                    // cstack is already on the neighbouring hex

                    if (isEnemy) {
                        // cstack is a potential attacker
                        auto mod = cstack->isShooter() && !cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY)
                            ? Export::DmgMod::HALF
                            : Export::DmgMod::FULL;

                        hex.setMeleeableByEnemyStack(cstack->unitSlot(), mod);

                        if (alreadyOnNBH) {
                            ASSERT(ainfo.at(nbh.hex) == EAccessibility::ALIVE_STACK, "expected ALIVE_STACK");
                            ASSERT(astack->isMeleeAttackPossible(astack, cstack, hex.bhex), "vcmi says melee attack is IMPOSSIBLE");
                            auto attack = AttackAction(nbh, hex.bhex, astack->occupiedHex(hex.bhex));
                            hex.hexactmask.at(EI(attack)) = true;
                            hex.setNextToEnemyStack(cstack->unitSlot(), true);
                        }

                        // cstack covers this hex => no other stack can cover it nor move there
                        break;
                    } else if (!isActive) { // friendly
                        if (alreadyOnNBH) {
                            hex.setNextToFriendlyStack(cstack->unitSlot(), true);
                        }
                    }
                }
            }
        }
    }

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    Hex Battlefield::InitHex(
        const int id,
        const std::vector<const CStack*> &allstacks,
        const CStack* astack,
        const Queue &queue,
        const AccessibilityInfo &ainfo, // accessibility info for active stack
        const ReachabilityInfos &rinfos,
        const ShooterInfos &sinfos
    ) {
        int x = id % BF_XMAX;
        int y = id / BF_XMAX;

        auto bh = BattleHex(x+1, y);
        expect(Hex::CalcId(bh) == id, "calcId mismatch: %d != %d", Hex::CalcId(bh), id);

        auto hex = Hex{};
        hex.bhex = bh;
        hex.setX(x);
        hex.setY(y);

        switch(ainfo.at(bh.hex)) {
        break; case EAccessibility::ACCESSIBLE: hex.setState(HexState::FREE);
        break; case EAccessibility::OBSTACLE: hex.setState(HexState::OBSTACLE);
        break; case EAccessibility::ALIVE_STACK: hex.setState(HexState::OCCUPIED);
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

        for (const auto &cstack : allstacks) {
            auto isActive = cstack == astack;
            auto isFriendly = cstack->getOwner() != astack->getOwner();
            auto isEnemy = !isFriendly;
            auto slot = cstack->unitSlot();

            // Stack exists => set default values to 0 instead of N/A
            // Some of those values them will be updated to true later
            if (isFriendly) {
                hex.setReachableByFriendlyStack(slot, false);
                hex.setReachableByEnemyStack(slot, false);
                if (isActive) {
                    hex.setMeleeableByActiveStack(Export::DmgMod::ZERO);
                    hex.setShootableByActiveStack(Export::DmgMod::ZERO);
                }
            } else {
                hex.setNextToFriendlyStack(slot, false);
                hex.setNextToEnemyStack(slot, false);
                hex.setMeleeableByEnemyStack(slot, Export::DmgMod::ZERO);
                hex.setShootableByEnemyStack(slot, Export::DmgMod::ZERO);
            }

            if ((isActive || isEnemy) && sinfos.at(cstack)) {
                // stack can shoot (not blocked & has ammo)
                auto mod = (cstack->hasBonusOfType(BonusType::NO_DISTANCE_PENALTY) || BattleHex::getDistance(cstack->getPosition(), bh) <= 10)
                    ? Export::DmgMod::FULL
                    : Export::DmgMod::HALF;

                isActive
                    ? hex.setShootableByActiveStack(mod)
                    : hex.setShootableByEnemyStack(slot, mod);
            }

            if (IsReachable(bh, cstack, rinfos)) {
                if (isEnemy) {
                    hex.setReachableByEnemyStack(slot, true);
                } else {
                    if (isActive) {
                        hex.hexactmask.at(EI(HexAction::MOVE)) = true;
                        hex.setReachableByActiveStack(true);
                        ProcessNeighbouringHexes(hex, astack, allstacks, ainfo, rinfos);
                    }
                    hex.setReachableByFriendlyStack(slot, true);
                }
            }

            if (!cstack->coversPos(bh)) continue;

            auto it = std::find(queue.begin(), queue.end(), cstack->unitId());
            auto qpos = (it == queue.end()) ? QSIZE-1 : it - queue.begin();
            if (isActive) ASSERT(qpos == 0, "expected qpos=0 for active stack");
            hex.setCStackAndAttrs(cstack, qpos);
        }

        return hex;
    }

    // static
    // result is a vector<UnitID>
    // XXX: there is a bug in VCMI when high morale occurs:
    //      - the stack acts as if it's already the next unit's turn
    //      - as a result, QueuePos for the ACTIVE stack is non-0
    //        while the QueuePos for the next (non-active) stack is 0
    // XXX: possibly similar issues occur when low morale occurs
    // As a workaround, morale is diabled (all battles are on cursed ground)
    // Possible solution is to set some flag via BAI->battleTriggerEffect (TODO)
    Queue Battlefield::GetQueue(CBattleCallback* cb) {
        auto res = Queue{};

        auto tmp = std::vector<battle::Units>{};
        cb->battleGetTurnOrder(tmp, QSIZE, 0);
        for (auto &units : tmp)
            for (auto &unit : units)
                res.insert(res.end(), unit->unitId());

        return res;
    }

    // static
    Hexes Battlefield::InitHexes(CBattleCallback* cb, const CStack* astack) {
        auto res = Hexes{};
        auto ainfo = cb->getAccesibility();
        auto rinfos = ReachabilityInfos{};
        auto allstacks = cb->battleGetStacks();
        auto sinfos = ShooterInfos{};  // expensive check for blocked shooters => eager load once

        for (const auto& cstack : allstacks) {
            rinfos.insert({cstack, std::make_shared<ReachabilityInfo>(cb->getReachability(cstack))});
            sinfos.insert({cstack, cb->battleCanShoot(astack)});
        }

        auto queue = GetQueue(cb);

        // state: set OBSTACLE/OCCUPIED/FREE_*
        // hexactmask: set MOVE
        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, allstacks, astack, queue, ainfo, rinfos, sinfos);
            expect(hex.getX() + hex.getY()*BF_XMAX == i, "hex.x + hex.y*BF_XMAX != i: %d + %d*%d != %d", hex.getX(), hex.getY(), BF_XMAX, i);
            res.at(hex.getY()).at(hex.getX()) = hex;
        }

        ASSERT(queue.size() == QSIZE, "queue size: " + std::to_string(queue.size()));

        return res;
    };

    const Export::State Battlefield::exportState() {
        auto res = Export::State{};

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                for (int i=0; i<EI(Export::Attribute::_count); i++) {
                    auto a = Export::Attribute(i);
                    auto v = hex.attrs.at(EI(a));
                    auto oh = Export::OneHot(a, v);
                    res.emplace_back(a, v);
                }
            }
        }

        return res;
    }

    const Export::ActMask Battlefield::exportActMask() {
        auto res = Export::ActMask{};
        int i = 0;

        for (int j=0; j<EI(NonHexAction::count); j++) {
            switch (NonHexAction(j)) {
            break; case NonHexAction::RETREAT: res.at(i++) = true;
            break; case NonHexAction::WAIT: res.at(i++) = !astack->waitedThisTurn;
            break; default:
                throw std::runtime_error("Unexpected NonHexAction: " + std::to_string(j));
            }
        }

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                for (auto &m : hex.hexactmask) {
                    res.at(i++) = m;
                }
            }
        }

        // static_assert in action_enums.h makes this redundant?
        ASSERT(i == Export::N_ACTIONS, "unexpected i: " + std::to_string(i));

        return res;
    }

    /**
     * Update state between turns:
     * - used if expecting a battle end (for terminal state/render)
     * - no "reachable" hexes (off-turn means no active stack)
     */
    void Battlefield::offTurnUpdate(CBattleCallback* cb) {
        auto ainfo = cb->getAccesibility();
        auto queue = GetQueue(cb);

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                switch(ainfo.at(hex.bhex.hex)) {
                break; case EAccessibility::ACCESSIBLE:
                    hex.setState(HexState::FREE);
                break; case EAccessibility::OBSTACLE:
                    hex.setState(HexState::OBSTACLE);
                break; case EAccessibility::ALIVE_STACK: {
                    hex.setState(HexState::OCCUPIED);

                    // nothing else needed here: no active stack
                    // => no special move case
                    // => no action mask
                }
                break; default:
                  throw std::runtime_error(
                    "Unexpected hex accessibility for hex "+ std::to_string(hex.bhex.hex) + ": "
                      + std::to_string(static_cast<int>(ainfo.at(hex.bhex.hex)))
                  );
                }
            }
        }
    }
}
