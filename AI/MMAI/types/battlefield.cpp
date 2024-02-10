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
#include "common.h"
#include "types/hexaction.h"
#include "types/hexactmask.h"
#include "types/stack.h"
#include <memory>
#include <stdexcept>

namespace MMAI {
    using NValue = Export::NValue;

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
    Hex Battlefield::InitHex(
        const int id,
        const std::vector<const CStack*> &allstacks,
        const CStack* astack,
        const bool canshoot,  // astack can shoot (not blocked)
        const Queue &queue,
        const AccessibilityInfo &ainfo, // accessibility info for active stack
        const ReachabilityInfos &rinfos
    ) {
        int x = id % BF_XMAX;
        int y = id / BF_XMAX;

        auto bh = BattleHex(x+1, y);
        ASSERT(bh.isAvailable(), "unavailable bh");
        expect(Hex::CalcId(bh) == id, "calcId mismatch: %d != %d", Hex::CalcId(bh), id);

        auto hex = Hex{};
        hex.bhex = bh;
        hex.x = x;
        hex.y = y;
        hex.hexactmask.fill(false);

        if (IsReachable(bh, astack, rinfos)) {
            hex.hexactmask.at(EI(HexAction::MOVE)) = true;
            hex.reachableByFriendlyStacks.insert(astack);

            // Report it as FREE_REACHABLE even if we are standing on it
            // Later, it will be "corrected" to OCCUPIED if needed
            hex.state = HexState::FREE_REACHABLE;

            // can also be OCCUPIED (by astack itself)
            // ASSERT(ainfo.at(bh.hex) == EAccessibility::ACCESSIBLE, "expected ACCESSIBLE hex");

            // assuming are at this hex, check each neighbouring hex for:
            //  - occupying stack (friendly or enemy)
            //  - potential attackers (enemy stacks that can reach those hexes)
            //
            // NOTE: proper neighbouring hexes are returned if we are 2-hex
            //
            for (const auto &nbh : astack->getSurroundingHexes(bh)) {
                auto obh = astack->occupiedHex(bh); // INVALID if astack is 1hex
                ASSERT(nbh != obh, "nbh == obh");

                for (const auto &cstack : allstacks) {
                    auto isEnemy = (cstack->getOwner() != astack->getOwner());
                    auto x = rinfos.at(cstack);

                    if (cstack->coversPos(nbh)) {
                        ASSERT(ainfo.at(nbh.hex) == EAccessibility::ALIVE_STACK, "expected ALIVE_STACK");

                        if (isEnemy) {
                            ASSERT(astack->isMeleeAttackPossible(astack, cstack, bh), "vcmi says melee attack is IMPOSSIBLE");
                            auto attack = AttackAction(nbh, bh, astack->occupiedHex(bh));
                            hex.hexactmask.at(EI(attack)) = true;
                            hex.neighbouringEnemyStacks.insert(cstack);
                            // potential attacker already here
                            hex.potentialEnemyAttackers.insert(cstack);
                        } else if (cstack != astack) {
                            // not sure if that info is useful
                            hex.neighbouringFriendlyStacks.insert(cstack);
                        }

                        break;
                    } else if (isEnemy && IsReachable(nbh, cstack, rinfos)) {
                        // potential attacker can come here
                        hex.potentialEnemyAttackers.insert(cstack);
                    }
                }
            }
        }

        for (const auto &cstack : allstacks) {
            auto isEnemy = cstack->getOwner() != astack->getOwner();

            // astack already inserted here
            if (cstack != astack && IsReachable(bh, cstack, rinfos)) {
                isEnemy
                    ? hex.reachableByEnemyStacks.insert(cstack)
                    : hex.reachableByFriendlyStacks.insert(cstack);
            }

            if (!cstack->coversPos(bh)) continue;
            hex.stack = std::make_shared<Stack>(InitStack(queue, cstack));

            if (canshoot && isEnemy) {
                hex.hexactmask.at(EI(HexAction::SHOOT)) = true;
                BattleHex::getDistance(bh, astack->position) > astack->getRangedFullDamageDistance()
                    ? hex.rangedDmgModifier = 0.5
                    : hex.rangedDmgModifier = 1;
            }
        }

        switch(ainfo.at(bh.hex)) {
        case EAccessibility::ACCESSIBLE: {
            // accessible, but not reachable => unreachable
            if (hex.state != HexState::FREE_REACHABLE)
                hex.state = HexState::FREE_UNREACHABLE;

            break;
        }
        case EAccessibility::OBSTACLE:
            hex.state = HexState::OBSTACLE;
            break;
        case EAccessibility::ALIVE_STACK: {
            hex.state = HexState::OCCUPIED;
            break;
        }
        // XXX: unhandled hex states
        // case EAccessibility::DESTRUCTIBLE_WALL:
        // case EAccessibility::GATE:
        // case EAccessibility::UNAVAILABLE:
        // case EAccessibility::SIDE_COLUMN:
        default:
          throw std::runtime_error(
            "Unexpected hex accessibility for hex "+ std::to_string(bh.hex) + ": "
              + std::to_string(static_cast<int>(ainfo.at(bh.hex)))
          );
        }

        return hex;
    }

    // static
    // result is a vector<UnitID>
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

        for (int i=0; i<allstacks.size(); i++) {
            auto cstack = allstacks.at(i);
            rinfos.insert({cstack, std::make_shared<ReachabilityInfo>(cb->getReachability(cstack))});
        }

        auto canshoot = cb->battleCanShoot(astack);
        auto queue = GetQueue(cb);

        // state: set OBSTACLE/OCCUPIED/FREE_*
        // hexactmask: set MOVE
        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, allstacks, astack, canshoot, queue, ainfo, rinfos);
            expect(hex.x + hex.y*BF_XMAX == i, "hex.x + hex.y*BF_XMAX != i: %d + %d*%d != %d", hex.x, hex.y, BF_XMAX, i);
            res.at(hex.y).at(hex.x) = hex;
        }

        ASSERT(queue.size() == QSIZE, "queue size: " + std::to_string(queue.size()));

        return res;
    };

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    Stack Battlefield::InitStack(const Queue &q, const CStack* cstack) {
        auto attrs = StackAttrs{};

        auto it = std::find(q.begin(), q.end(), cstack->unitId());
        auto qpos = (it == q.end()) ? QSIZE-1 : it - q.begin();

        for (int i=0; i<EI(StackAttr::count); i++) {
            int a;

            switch(StackAttr(i)) {
            break; case StackAttr::Quantity: a = cstack->getCount();
            break; case StackAttr::Attack: a = cstack->getAttack(false);
            break; case StackAttr::Defense: a = cstack->getDefense(false);
            break; case StackAttr::Shots: a = cstack->shots.available();
            break; case StackAttr::DmgMin: a = cstack->getMinDamage(false);
            break; case StackAttr::DmgMax: a = cstack->getMaxDamage(false);
            break; case StackAttr::HP: a = cstack->getMaxHealth();
            break; case StackAttr::HPLeft: a = cstack->getFirstHPleft();
            break; case StackAttr::Speed: a = cstack->speed();
            break; case StackAttr::Waited: a = cstack->waitedThisTurn;
            break; case StackAttr::QueuePos:
                a = qpos;
                ASSERT(a >= 0 && a < QSIZE, "unexpected qpos: " + std::to_string(a));
            break; case StackAttr::RetaliationsLeft: a = cstack->counterAttacks.available();
            break; case StackAttr::Side: a = cstack->unitSide();
            break; case StackAttr::Slot:
                a = cstack->unitSlot();
                ASSERT(a >= 0 && a < 7, "unexpected slot: " + std::to_string(a));
            break; case StackAttr::CreatureType:
                a = cstack->creatureId();
            break; case StackAttr::AIValue:
                a = cstack->creatureId().toCreature()->getAIValue();
            break; case StackAttr::IsActive:
                a = (qpos == 0) ? 1 : 0;
            break; default:
                throw std::runtime_error("Unexpected StackAttr: " + std::to_string(i));
            }

            attrs.at(i) = a;
        }

        return Stack(cstack, attrs);
    }

    // static
    void Battlefield::AddToExportState(Export::State &state, std::set<const CStack*> &stacks, int i, int max) {
        // this is because we use "slot" for the position
        ASSERT(max == 7, "expected max=7");

        int added = 0;
        for (const auto &cstack : stacks) {
            if (added >= max) break;
            auto slot = static_cast<int>(cstack->unitSlot());
            expect(slot <= max, "slot > max (%d > %d)", slot, max);

            // the position encodes the slot => value is binary
            state.at(i+slot) = NValue(1, 0, 1);
            added += 1;
        }
    }

    const Export::State Battlefield::exportState() {
        auto res = Export::State{};
        int i = 0;

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                static_assert(EI(HexState::INVALID) == 0);
                ASSERT(EI(hex.state) > 0, "INVALID hex during export");

                res.at(i++) = NValue(hex.y, 0, BF_YMAX - 1);
                res.at(i++) = NValue(hex.x, 0, BF_XMAX - 1);
                res.at(i++) = NValue(EI(hex.state), 1, EI(HexState::count) - 1);
                auto &stack = hex.stack;

                for (int j=0; j<EI(StackAttr::count); j++) {
                    int max;

                    switch (StackAttr(j)) {
                    break; case StackAttr::Quantity:         max = 5000;
                    break; case StackAttr::Attack:           max = 100;
                    break; case StackAttr::Defense:          max = 100;
                    break; case StackAttr::Shots:            max = 32;
                    break; case StackAttr::DmgMin:           max = 100;
                    break; case StackAttr::DmgMax:           max = 100;
                    break; case StackAttr::HP:               max = 1500;
                    break; case StackAttr::HPLeft:           max = 1500;
                    break; case StackAttr::Speed:            max = 30;
                    break; case StackAttr::Waited:           max = 1;
                    break; case StackAttr::QueuePos:         max = QSIZE-1;
                    break; case StackAttr::RetaliationsLeft: max = 3;
                    break; case StackAttr::Side:             max = 1;
                    break; case StackAttr::Slot:             max = 6;
                    break; case StackAttr::CreatureType:     max = 150;
                    break; case StackAttr::AIValue:          max = 40000; // azure dragon is ~80k!
                    break; case StackAttr::IsActive:         max = 1;
                    break; default:
                        throw std::runtime_error("Unexpected StackAttr: " + std::to_string(EI(j)));
                    }

                    res.at(i++) = NValue(stack->attrs.at(j), ATTR_NA, max);
                }

                res.at(i++) = NValue(hex.rangedDmgModifier, 0, 1);
                AddToExportState(res, hex.reachableByFriendlyStacks, i, 7);
                i += 7;
                AddToExportState(res, hex.reachableByEnemyStacks, i, 7);
                i += 7;
                AddToExportState(res, hex.neighbouringFriendlyStacks, i, 7);
                i += 7;
                AddToExportState(res, hex.neighbouringEnemyStacks, i, 7);
                i += 7;
                AddToExportState(res, hex.potentialEnemyAttackers, i, 7);
                i += 7;

                expect(i % Export::N_HEX_ATTRS == 0, "i %% Export::N_HEX_ATTRS != 0 (%d %% %d != 0)", i, Export::N_HEX_ATTRS);
            }
        }

        ASSERT(i == Export::STATE_SIZE, "unexpected i: " + std::to_string(i));

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
                    hex.state = HexState::FREE_UNREACHABLE;
                break; case EAccessibility::OBSTACLE:
                    hex.state = HexState::OBSTACLE;
                break; case EAccessibility::ALIVE_STACK: {
                    hex.state = HexState::OCCUPIED;

                    auto cstack = cb->battleGetStackByPos(hex.bhex, true);
                    ASSERT(cstack, "null cstack on ALIVE_STACK hex");
                    hex.stack = std::make_unique<Stack>(InitStack(queue, cstack));

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
