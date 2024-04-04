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
#include "battle/BattleHex.h"
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
        // isReachable may return true even if speed is insufficient
        // distances is 0 for the stack's main hex, 1 for its "back" hex
        // (100000 if it can't fit there)
        return stackinfo.rinfo->distances.at(bh) <= stackinfo.speed;
    }

    // static
    HexAction Battlefield::HexActionFromHexes(
        const BattleHex &nbh, // hex to attack
        const BattleHex &bh,  // hex attacking from (pos)
        const BattleHex &bh2  // 2nd hex attacking from (2-hex stacks only)
    ) {
        auto dir = BattleHex::mutualPosition(bh, nbh);

        if (dir != BattleHex::EDir::NONE)
            return EDIR_TO_AMOVE.at(dir);

        // "indirect" neighbouring hex (astack is double-wide)
        ASSERT(bh2 != BattleHex::INVALID, "bh2 is INVALID");
        dir = BattleHex::mutualPosition(bh2, nbh);
        return EDIR_TO_AMOVE_2.at(dir);
    }

    // static
    // If we were to move to this hex, check each neighbouring hex for:
    //  - occupying stack (friendly or enemy)
    //  - potential attackers (enemy stacks that can reach and attack from those hexes)
    //
    // NOTE: If the active stack is a 2-hex stack, if it were to move to "hex",
    //       then 8 (insted of 6) hexes should be considered "neighbouring".
    //       HOWEVER, we do the calculations as if the active stack is 1-hex as
    //       (6 neuighbouring hexes) or there would be too much confusion:
    //       E.g. if astack is wide, the back hex could turn out onto an
    //       obstacle unless we call ProcessNeighbouringHexes only on hexes
    //       which are reachable - this leads to two other problems:
    //          1/ incomplete/wrong information for the rest of the battlefield
    //          2/ wrong information for hexes where it could only stand
    //             onto with its "back" hex (e.g. right next to obstacles), as
    //             those are considered unreachable, hence with no potential
    //             attackers.
    //
    // => just pretend the active stack is a 1-hex stack here
    void Battlefield::ProcessNeighbouringHexes(
        Hex &hex,
        const CStack* astack,
        const StackInfos &stackinfos,
        const HexStacks &hexstacks
    ) {
        for (const auto& [cstack, stackinfo] : stackinfos) {
            auto isActive = (cstack == astack);
            auto isFriendly = (cstack->getOwner() == astack->getOwner());
            auto slot = cstack->unitSlot();

            // For each stack, set hex attributes:
            //
            // (1) HEX_NEXT_TO_*          if stack stands on a directly
            //                              neighbouring hex
            //
            // (2) HEX_MELEEABLE_BY_*     if stack (stands on OR can reach)
            //                              a (direct OR special) nbhex
            //
            for (const auto& [dir, nbh] : NearbyHexesForAttributes(hex.bhex, cstack)) {
                if (cstack->coversPos(nbh)) {
                    if (dir != D::NONE) // direct neighbour
                        hex.setNextToStack(isActive, isFriendly, cstack->unitSlot(), true);

                    hex.setMeleeableByStack(isActive, isFriendly, slot, stackinfo.meleemod);
                    break; // no other stack can cover this hex (nor move here)
                } else if (IsReachable(nbh, stackinfo)) {
                    hex.setMeleeableByStack(isActive, isFriendly, slot, stackinfo.meleemod);
                }
            }
        }

        // For active stack, set actmask:
        //
        // (1) hexactmask for attack  if stack stands on `hex`'s nearby hex (nbh)
        //                              AND stack is enemy
        //                              AND astack (stands on OR can reach) `hex`
        //
        for (const auto& [hexaction, nbh] : NearbyHexesForActmask(hex.bhex, astack)) {
            auto it = hexstacks.find(nbh);
            if (it == hexstacks.end())
                continue; // no unit on nbh

            auto cstack = it->second;
            if (cstack->getOwner() == astack->getOwner())
                continue; // friendly unit on nbh

            if (!IsReachable(hex.bhex, stackinfos.at(astack)))
                continue; // can't reach the hex to attack from

            ASSERT(astack->isMeleeAttackPossible(astack, cstack, hex.bhex), "vcmi says melee attack is IMPOSSIBLE");
            hex.hexactmask.at(EI(hexaction)) = true;
        }
        return;
    }

    // static
    //
    // Return bh's neighbouring hexes for setting the AMOVE_* heaxctmasks
    // i.e. assume astack's position is fixed at "@", return nearby hexes "X"
    // at which astack could perform a melee attack:
    //
    //  1-hex:        2-hex (attacker):     2-hex (defender):
    //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    // . . X X . . . . . . . X X X . . . . . . . . X X X . . .
    //  . X @ X . . . . . . X - @ X . . . . . . . X @ - X . . .
    // . . X X . . . . . . . X X X . . . . . . . . X X X . . .
    //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    // Legend:
    // @ is astack's front hex
    // - is astack's rear hex (if astack is a 2-hex creature)
    // * are the nearby hexes to return
    // attacker/defender is astack's perspective (attacker=left/defender=right)
    //
    // Available "X" hexes are returned according to the unit size.
    //
    // Each of the "X" has a HexAction associated with it (see hexaction.h),
    // which is returned along with the corresponding hex.
    //
    HexActionHex Battlefield::NearbyHexesForActmask(BattleHex &bh, const CStack* astack) {
        // The 6 basic directions
        auto res = HexActionHex{};
        res.reserve(8);
        BattleHex nbh;

        nbh = bh.cloneInDirection(D::TOP_RIGHT, false);
        if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_TR, nbh);

        nbh = bh.cloneInDirection(D::BOTTOM_RIGHT, false);
        if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_BR, nbh);

        nbh = bh.cloneInDirection(D::BOTTOM_LEFT, false);
        if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_BL, nbh);

        nbh = bh.cloneInDirection(D::TOP_LEFT, false);
        if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_TL, nbh);

        if (!astack->doubleWide()) {
            nbh = bh.cloneInDirection(D::LEFT, false);
            if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_L, nbh);

            nbh = bh.cloneInDirection(D::RIGHT, false);
            if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_R, nbh);

            return res;
        }

        // The 6 "special" directions (3 for each side)
        if (astack->unitSide() == BattleSide::ATTACKER) {
            // astack's rear hex is to-the-left
            nbh = bh.cloneInDirection(D::RIGHT, false);
            if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_R, nbh);

            auto offset = bh.cloneInDirection(D::LEFT, false);

            nbh = offset.cloneInDirection(D::BOTTOM_LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2BL, nbh);

            nbh = offset.cloneInDirection(D::LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2L, nbh);

            nbh = offset.cloneInDirection(D::TOP_LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2TL, nbh);
        } else {
            // astack's rear hex is to-the-right
            nbh = bh.cloneInDirection(D::LEFT, false);
            if(nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_L, nbh);

            auto offset = bh.cloneInDirection(D::RIGHT, false);

            nbh = offset.cloneInDirection(D::TOP_RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2TR, nbh);

            nbh = offset.cloneInDirection(D::RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2R, nbh);

            nbh = offset.cloneInDirection(D::BOTTOM_RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(HexAction::AMOVE_2BR, nbh);
        }

        return res;
    }

    // static
    //
    // Return bh's neighbouring hexes for setting HEX_ATTACKABLE_BY_* attrs
    // i.e. assume the target hex is fixed at "X", return nearby hexes "@"
    // from which cstack could perform a melee attack:
    //
    //  1-hex:        2-hex (attacker):     2-hex (defender):
    //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    // . . @ @ . . . . . . - @ @ @ . . . . . . . . @ @ @ - . .
    //  . @ X @ . . . . . - @ X - @ . . . . . . . @ - X @ - . .
    // . . @ @ . . . . . . - @ @ @ . . . . . . . . @ @ @ - . .
    //  . . . . . . . . . . . . . . . . . . . . . . . . . . . .
    // Legend:
    // X is the target hex
    // @ is cstack's front hex
    // - is cstack's rear hex (if cstack is a 2-hex creature)
    // attacker/defender is astack's perspective (attacker=left/defender=right)
    //
    // Hexes outside the map are not returned.
    // If cstack is 1-hex stack, a maximum of 6 hexes are returned.
    // If cstack is a 2-hex stack, a maximum of 8 hexes are returned.
    //
    // Each of the "@" has a EDir associated with it, this is relevant only
    // for the "special" cases where the @ is not a direct neighbour (EDir::NONE).
    //
    DirHex Battlefield::NearbyHexesForAttributes(BattleHex &bh, const CStack* cstack) {
        // The 6 basic directions
        auto res = DirHex{};
        res.reserve(8);
        BattleHex nbh;

        nbh = bh.cloneInDirection(D::TOP_RIGHT, false);
        if (nbh.isAvailable()) res.emplace_back(D::TOP_RIGHT, nbh);

        nbh = bh.cloneInDirection(D::BOTTOM_RIGHT, false);
        if (nbh.isAvailable()) res.emplace_back(D::BOTTOM_RIGHT, nbh);

        nbh = bh.cloneInDirection(D::BOTTOM_LEFT, false);
        if (nbh.isAvailable()) res.emplace_back(D::BOTTOM_LEFT, nbh);

        nbh = bh.cloneInDirection(D::TOP_LEFT, false);
        if (nbh.isAvailable()) res.emplace_back(D::TOP_LEFT, nbh);

        if (!cstack->doubleWide()) {
            nbh = bh.cloneInDirection(D::LEFT, false);
            if(nbh.isAvailable()) res.emplace_back(D::LEFT, nbh);

            nbh = bh.cloneInDirection(D::RIGHT, false);
            if(nbh.isAvailable()) res.emplace_back(D::RIGHT, nbh);

            return res;
        }

        // The 6 "special" directions (3 for each side)
        if (cstack->unitSide() == BattleSide::ATTACKER) {
            // enemy's rear hex is to-the-right
            nbh = bh.cloneInDirection(D::LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(D::LEFT, nbh);

            auto offset = bh.cloneInDirection(D::RIGHT, false);

            nbh = offset.cloneInDirection(D::TOP_RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);

            nbh = offset.cloneInDirection(D::RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);

            nbh = offset.cloneInDirection(D::BOTTOM_RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);

        } else {
            // enemy's rear hex is to-the-left
            nbh = bh.cloneInDirection(D::RIGHT, false);
            if (nbh.isAvailable()) res.emplace_back(D::RIGHT, nbh);

            auto offset = bh.cloneInDirection(D::LEFT, false);

            nbh = offset.cloneInDirection(D::BOTTOM_LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);

            nbh = offset.cloneInDirection(D::LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);

            nbh = offset.cloneInDirection(D::TOP_LEFT, false);
            if (nbh.isAvailable()) res.emplace_back(D::NONE, nbh);
        }

        return res;
    }

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    Hex Battlefield::InitHex(
        const int id,
        const CStack* astack,
        const Queue &queue,
        const AccessibilityInfo &ainfo, // accessibility info for active stack
        const StackInfos &stackinfos,
        const HexStacks &hexstacks
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

        for (const auto& [cstack, stackinfo] : stackinfos) {
            auto isActive = cstack == astack;
            auto isFriendly = cstack->getOwner() == astack->getOwner();
            auto slot = cstack->unitSlot();

            // Stack exists => set default values to 0 instead of N/A
            // Some of those values them will be updated to true later
            hex.setReachableByStack(isActive, isFriendly, slot, false);
            hex.setMeleeableByStack(isActive, isFriendly, slot, Export::DmgMod::ZERO);
            hex.setShootableByStack(isActive, isFriendly, slot, Export::DmgMod::ZERO);
            hex.setNextToStack(isActive, isFriendly, slot, false);

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
                auto mod = (stackinfo.noDistancePenalty || BattleHex::getDistance(cstack->getPosition(), bh) <= 10)
                    ? Export::DmgMod::FULL
                    : Export::DmgMod::HALF;

                hex.setShootableByStack(isActive, isFriendly, slot, mod);
            }

            auto isReachable = IsReachable(bh, stackinfo);

            if (isReachable || cstack->coversPos(bh)) {
                hex.setReachableByStack(isActive, isFriendly, slot, true);

                // although hex may already stand on hex, it be unable to
                // move on it (e.g. that is its rear hex and there's no space)
                if (isActive && isReachable) hex.hexactmask.at(EI(HexAction::MOVE)) = true;
            }

            ProcessNeighbouringHexes(hex, astack, stackinfos, hexstacks);

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
        auto hexstacks = HexStacks{};  // expensive check for blocked shooters => eager load once
        auto stackinfos = StackInfos{}; // expensive check for stack->speed, isblocked and getReachability

        for (const auto& cstack : cb->battleGetStacks()) {
            stackinfos.insert({cstack, StackInfo(
                cstack->speed(),
                // cstack->canShoot() && (cstack->hasBonusOfType(BonusType::FREE_SHOOTING) || !cb->battleIsUnitBlocked(cstack))
                cb->battleCanShoot(cstack),
                (cstack->isShooter() && !cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY) ? Export::DmgMod::HALF : Export::DmgMod::FULL),
                cstack->hasBonusOfType(BonusType::NO_DISTANCE_PENALTY),
                std::make_shared<ReachabilityInfo>(cb->getReachability(cstack))
            )});

            for (auto bh : cstack->getHexes())
                hexstacks.insert({bh, cstack});
        }

        auto queue = GetQueue(cb);

        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, astack, queue, ainfo, stackinfos, hexstacks);
            expect(hex.getX() + hex.getY()*BF_XMAX == i, "hex.x + hex.y*BF_XMAX != i: %d + %d*%d != %d", hex.getX(), hex.getY(), BF_XMAX, i);
            res.at(hex.getY()).at(hex.getX()) = hex;
        }

        ASSERT(queue.size() == QSIZE, "queue size: " + std::to_string(queue.size()));

        return res;
    };

    const std::pair<Export::State, Export::EncodedState> Battlefield::exportState() {
        auto state = Export::State{};
        auto encstate = Export::EncodedState{};
        state.reserve(Export::STATE_SIZE);
        encstate.reserve(Export::ENCODED_STATE_SIZE);

        for (auto &hexrow : hexes) {
            for (auto &hex : hexrow) {
                for (int i=0; i<EI(Export::Attribute::_count); i++) {
                    auto a = Export::Attribute(i);
                    auto v = hex.attrs.at(EI(a));
                    auto onehot = Export::OneHot(a, v);

                    onehot.encode(encstate);
                    state.push_back(std::move(onehot));
                }
            }
        }

        return {state, encstate};
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
