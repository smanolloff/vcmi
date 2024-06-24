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

#include "BAI/v3/encoder.h"
#include "Global.h"
#include "battle/ReachabilityInfo.h"
#include "common.h"
#include "schema/base.h"
#include "schema/v3/types.h"
#include "schema/v3/constants.h"
#include "./hexaction.h"
#include "./hexactmask.h"
#include "./battlefield.h"


namespace MMAI::BAI::V3 {
    using HA = HexAttribute;
    using SA = StackAttribute;

    Battlefield::Battlefield(
        GeneralInfo&& info_,
        Hexes&& hexes_,
        Stacks&& stacks_,
        Stack* astack_
    ) : info(std::move(info_))
      , hexes(std::move(hexes_))
      , stacks(std::move(stacks_))
      , astack(astack_) {};

    // static
    bool Battlefield::IsReachable(const BattleHex &bh, const StackInfo &stackinfo) {
        // XXX: don't rely on ReachabilityInfo's `isReachable` as it
        //      returns true even if speed is insufficient
        // NOTE: distances is 0 for the stack's main hex, 1 for its "back" hex
        //       (100000 if it can't fit there)
        return stackinfo.rinfo->distances.at(bh) <= stackinfo.speed;
    }

    //
    // Return bh's neighbouring hexes for setting action mask
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

        auto nbhR = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
        auto nbhL = bh.cloneInDirection(BattleHex::EDir::LEFT, false);

        return HexActionHex{
            // The 6 basic directions
            bh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
            nbhR,
            bh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),
            bh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
            nbhL,
            bh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false),

            // Extended directions for R-side wide creatures
            nbhR.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
            nbhR.cloneInDirection(BattleHex::EDir::RIGHT, false),
            nbhR.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),

            // Extended directions for L-side wide creatures
            nbhL.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
            nbhL.cloneInDirection(BattleHex::EDir::LEFT, false),
            nbhL.cloneInDirection(BattleHex::EDir::TOP_LEFT, false)
        };
    }

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    std::unique_ptr<Hex> Battlefield::InitHex(
        const int id,
        const AccessibilityInfo &ainfo,
        const StackInfo &astackinfo,
        const HexStacks &hexstacks,
        const HexObstacles &hexobstacles
    ) {
        int x = id % BF_XMAX;
        int y = id / BF_XMAX;

        auto bh = BattleHex(x+1, y);
        expect(Hex::CalcId(bh) == id, "calcId mismatch: %d != %d", Hex::CalcId(bh), id);

        auto hex = std::make_unique<Hex>();
        hex->bhex = bh;
        hex->setattr(HexAttribute::Y_COORD, y);
        hex->setattr(HexAttribute::X_COORD, x);

        auto astack = astackinfo.stack;

        const Stack* h_stack = nullptr;
        auto it = hexstacks.find(hex->bhex);
        if (it != hexstacks.end())
            h_stack = it->second;

        switch(ainfo.at(bh.hex)) {
        break; case EAccessibility::ACCESSIBLE:
            hex->setState(HexState::FREE);
        break; case EAccessibility::OBSTACLE: {
            auto it = hexobstacles.find(bh);
            ASSERT(it != hexobstacles.end(), "OBSTACLE but no obstacle on hex");
            auto &obstacle = it->second;
            switch(obstacle->obstacleType) {
            case CObstacleInstance::USUAL:
            case CObstacleInstance::ABSOLUTE_OBSTACLE:
            case CObstacleInstance::SPELL_CREATED:
                hex->setState(HexState::OBSTACLE);
                break;
            case CObstacleInstance::MOAT:
                hex->setState(HexState::MOAT);
                break;
            default:
                THROW_FORMAT("Unexpected obstacle type: %d", obstacle->obstacleType);
            }
        }
        break; case EAccessibility::ALIVE_STACK:
            ASSERT(h_stack, "accessibility is ALIVE_STACK, but no stack could be found on hex");
            hex->setState(h_stack->cstack->getPosition() == bh ? HexState::STACK_FRONT : HexState::STACK_BACK);
        break; case EAccessibility::DESTRUCTIBLE_WALL:
            hex->setState(HexState::DESTRUCTIBLE_WALL);
        break; case EAccessibility::GATE:
            hex->setState(HexState::GATE);
        break; default:
            THROW_FORMAT("Unexpected hex accessibility for hex %d: %d", bh.hex % static_cast<int>(ainfo.at(bh.hex)));
        }

        // It seems spellcaster spells are selected client-side
        // (with a TODO for server-side selection)
        //
        // * In UI interface:
        //      BattleActionsController::tryActivateStackSpellcasting
        //      ^ populates `creatureSpells`
        //      > BattleActionsController::getPossibleActionsForStack
        //        ^ populates `data.creatureSpellsToCast`
        //        > CBattleInfoCallback::getClientActionsForStack
        //
        // * In BattleAI:
        //      BattleEvaluator::findBestCreatureSpell
        //
        // * ?
        //      CBattleInfoCallback::getClientActionsForStack
        //
        // It seems all cast bonuses are "valid" actions
        // (see "fairieDragon" (with the typo) in creatures/neutral.json)
        // So it's whichever the client chooses atm.
        logAi->warn("TODO: handle special actions");
        // hex->permitAction(HexAction::SPECIAL)

        if (astackinfo.canshoot)
            hex->permitAction(HexAction::SHOOT);

        if (IsReachable(bh, astackinfo))
            hex->permitAction(HexAction::MOVE);
        else
            return hex; // nothing else to do

        const auto &nbhexes = NearbyHexes(hex->bhex);
        const auto a_cstack = astack->cstack;

        for (int i=0; i<nbhexes.size(); ++i) {
            auto &n_bhex = nbhexes.at(i);
            if (!n_bhex.isAvailable())
                continue;

            auto it = hexstacks.find(n_bhex);
            if (it == hexstacks.end())
                continue;

            auto &n_cstack = it->second->cstack;
            auto hexaction = HexAction(i);

            if (n_cstack->unitSide() != a_cstack->unitSide()) {
                if (hexaction <= HexAction::AMOVE_TL) {
                    ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bh), "vcmi says melee attack is IMPOSSIBLE [1]");
                    hex->permitAction(hexaction);
                } else if (hexaction <= HexAction::AMOVE_2BR) {
                    // only wide R stacks can perform 2TR/2R/2BR attacks
                    if (a_cstack->unitSide() == 1 && a_cstack->doubleWide()) {
                        ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bh), "vcmi says melee attack is IMPOSSIBLE [2]");
                        hex->permitAction(hexaction);
                    }
                } else {
                    // only wide L stacks can perform 2TL/2L/2BL attacks
                    if (a_cstack->unitSide() == 0 && a_cstack->doubleWide()) {
                        ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bh), "vcmi says melee attack is IMPOSSIBLE");
                        hex->permitAction(hexaction);
                    }
                }
            }
        }

        hex->finalizeActionMask();
        return hex;
    }

    // static
    std::unique_ptr<const Battlefield> Battlefield::Create(
        const CPlayerBattleCallback* battle,
        const CStack* astack_,
        const ArmyValues av,
        bool isMorale
    ) {
        auto [stacks, astack] = InitStacks(battle, astack_, isMorale);
        auto hexes = InitHexes(battle, stacks);
        auto info = GeneralInfo(battle, av);

        return std::make_unique<const Battlefield>(
            std::move(info),
            std::move(hexes),
            std::move(stacks),
            std::move(astack)
        );
    }

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
    Hexes Battlefield::InitHexes(const CPlayerBattleCallback* battle, const Stacks &stacks) {
        auto res = Hexes{};
        auto ainfo = battle->getAccesibility();
        auto hexstacks = HexStacks{};  // expensive check for blocked shooters => eager load once
        auto hexobstacles = HexObstacles{};
        const Stack* astack;

        for (const auto &stack : stacks) {
            for (auto &bh : stack->cstack->getHexes())
                hexstacks.insert({bh, stack.get()});

            if (stack->isActive)
                astack = stack.get();
        }

        for (auto &obstacle : battle->battleGetAllObstacles()) {
            for (auto &bh : obstacle->getAffectedTiles()) {
                hexobstacles.insert({bh, obstacle});
            }
        }

        ASSERT(astack, "failed to find active stack");

        auto astackinfo = StackInfo(
            astack,
            battle->battleCanShoot(astack->cstack),
            battle->getReachability(astack->cstack)
        );

        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, ainfo, astackinfo, hexstacks, hexobstacles);
            res.at(hex->attr(HA::Y_COORD)).at(hex->attr(HA::X_COORD)) = std::move(hex);
        }

        return res;
    };

    // static
    std::pair<Stacks, Stack*> Battlefield::InitStacks(
        const CPlayerBattleCallback* battle,
        const CStack* astack,
        bool isMorale
    ) {
        logAi->warn("TODO: implement Battlefield::InitStacks(...)");
        return {};
    }

}
