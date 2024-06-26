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
        std::shared_ptr<GeneralInfo> info_,
        std::shared_ptr<Hexes> hexes_,
        std::shared_ptr<Stacks> stacks_,
        const CStack* astack_
    ) : info(std::move(info_))
      , hexes(std::move(hexes_))
      , stacks(std::move(stacks_))
      , astack(astack_) {};

    // static
    bool Battlefield::IsReachable(const BattleHex &bh, const StackInfo* stackinfo) {
        // XXX: don't rely on ReachabilityInfo's `isReachable` as it
        //      returns true even if speed is insufficient
        // NOTE: distances is 0 for the stack's main hex, 1 for its "back" hex
        //       (100000 if it can't fit there)
        return stackinfo->rinfo->distances.at(bh) <= stackinfo->speed;
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
    std::shared_ptr<Hex> Battlefield::InitHex(
        const int id,
        const AccessibilityInfo &ainfo,
        const std::shared_ptr<StackInfo> astackinfo,
        const HexStacks &hexstacks,
        const HexObstacles &hexobstacles
    ) {
        int x = id % BF_XMAX;
        int y = id / BF_XMAX;

        auto bh = BattleHex(x+1, y);
        expect(Hex::CalcId(bh) == id, "calcId mismatch: %d != %d", Hex::CalcId(bh), id);

        auto hex = std::make_shared<Hex>();
        hex->bhex = bh;
        hex->setattr(HA::Y_COORD, y);
        hex->setattr(HA::X_COORD, x);

        std::shared_ptr<const Stack> h_stack = nullptr;
        auto it = hexstacks.find(hex->bhex);
        if (it != hexstacks.end())
            h_stack = it->second;

        switch(ainfo.at(bh.hex)) {
        break; case EAccessibility::ACCESSIBLE:
            ASSERT(!h_stack, "accessibility is ACCESSIBLE, but a stack was found on hex");
            hex->setattr(HA::STATE, EI(HexState::FREE));
        break; case EAccessibility::OBSTACLE: {
            ASSERT(!h_stack, "accessibility is ACCESSIBLE, but a stack was found on hex");
            auto it = hexobstacles.find(bh);
            ASSERT(it != hexobstacles.end(), "OBSTACLE but no obstacle on hex");
            auto &obstacle = it->second;
            switch(obstacle->obstacleType) {
            case CObstacleInstance::USUAL:
            case CObstacleInstance::ABSOLUTE_OBSTACLE:
            case CObstacleInstance::SPELL_CREATED:
                hex->setattr(HA::STATE, EI(HexState::OBSTACLE));
                break;
            case CObstacleInstance::MOAT:
                hex->setattr(HA::STATE, EI(HexState::MOAT));
                break;
            default:
                THROW_FORMAT("Unexpected obstacle type: %d", obstacle->obstacleType);
            }
        }
        break; case EAccessibility::ALIVE_STACK:
            ASSERT(h_stack, "accessibility is ALIVE_STACK, but no stack could be found on hex");
            hex->setattr(HA::STATE, EI(h_stack->cstack->getPosition() == bh ? HexState::STACK_FRONT : HexState::STACK_BACK));
            hex->setattr(HA::STACK_ID, h_stack->attr(SA::ID));
            hex->stack = h_stack;
        break; case EAccessibility::DESTRUCTIBLE_WALL:
            // XXX: Only for non-destroyed walls.
            // XXX: Destroyed walls become ACCESSIBLE.
            ASSERT(!h_stack, "accessibility is DESTRUCTIBLE_WALL, but a stack was found on hex");
            hex->setattr(HA::STATE, EI(HexState::DESTRUCTIBLE_WALL));
        break; case EAccessibility::GATE:
            // XXX: Only for closed, non-blocked gate.
            //      A blocked gate becomes UNAVAILABLE.
            //      In all other cases it is ACCESSIBLE.
            hex->setattr(HA::STATE, EI(HexState::GATE));
        break; default:
            THROW_FORMAT("Unexpected hex accessibility for hex %d: %d", bh.hex % static_cast<int>(ainfo.at(bh.hex)));
        }

        if (!astackinfo)
            return hex;

        auto astack = astackinfo->stack;

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
        // logAi->warn("TODO: handle special actions");
        // hex->permitAction(HexAction::SPECIAL)

        if (astackinfo->canshoot && h_stack && h_stack->cstack->unitSide() != astack->cstack->unitSide())
            hex->permitAction(HexAction::SHOOT);

        // XXX: false if astack is NULL
        if (IsReachable(bh, astackinfo.get()))
            hex->permitAction(HexAction::MOVE);
        else {
            hex->finalizeActionMask();
            return hex; // nothing else to do
        }

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
    std::shared_ptr<const Battlefield> Battlefield::Create(
        const CPlayerBattleCallback* battle,
        const CStack* astack,
        const ArmyValues av,
        bool isMorale
    ) {
        auto stacks = InitStacks(battle, astack, isMorale);
        auto hexes = InitHexes(battle, stacks, astack == nullptr);
        auto info = std::make_shared<GeneralInfo>(battle, av);

        return std::make_shared<const Battlefield>(info, hexes, stacks, astack);
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
                res.push_back(unit->unitId());

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
    std::shared_ptr<Hexes> Battlefield::InitHexes(const CPlayerBattleCallback* battle, const std::shared_ptr<Stacks> stacks, bool ended) {
        auto res = Hexes{};
        auto ainfo = battle->getAccesibility();
        auto hexstacks = HexStacks{};  // expensive check for blocked shooters => eager load once
        auto hexobstacles = HexObstacles{};
        std::shared_ptr<const Stack> astack;

        for (const auto &sidestacks : *stacks) {
            for (const auto &stack : sidestacks) {
                if (!stack)
                    continue;

                for (auto &bh : stack->cstack->getHexes())
                    hexstacks.insert({bh, stack});

                if (stack->attr(SA::QUEUE_POS) == 0 && !ended)
                    astack = stack;
            }
        }

        for (auto &obstacle : battle->battleGetAllObstacles()) {
            for (auto &bh : obstacle->getAffectedTiles()) {
                hexobstacles.insert({bh, obstacle});
            }
        }

        std::shared_ptr<StackInfo> astackinfo;

        if (astack) {
            astackinfo = std::make_shared<StackInfo>(
                astack,
                battle->battleCanShoot(astack->cstack),
                battle->getReachability(astack->cstack)
            );
        } else if (!ended) {
            throw std::runtime_error("No active stack found");
        }

        for (int i=0; i<BF_SIZE; i++) {
            auto hex = InitHex(i, ainfo, astackinfo, hexstacks, hexobstacles);
            res.at(hex->attr(HA::Y_COORD)).at(hex->attr(HA::X_COORD)) = std::move(hex);
        }

        return std::make_shared<Hexes>(std::move(res));
    };

    // static
    std::shared_ptr<Stacks> Battlefield::InitStacks(
        const CPlayerBattleCallback* battle,
        const CStack* astack,
        bool isMorale
    ) {
        auto stacks = Stacks{};
        auto cstacks = battle->battleGetStacks();

        // Sorting needed to ensure ordered insertion of summons/machines
        std::sort(cstacks.begin(), cstacks.end(), [](const CStack* a, const CStack* b) {
            return a->unitId() < b->unitId();
        });

        // Logic below assumes at least one side
        static_assert(MAX_STACKS_PER_SIDE > 7, "expected MAX_STACKS_PER_SIDE > 7");

        /*
         * Units for each side are indexed as follows:
         *
         *  1. The 7 "regular" army stacks use indexes 0..6 (index=slot)
         *  2. Up to N* summoned units will use indexes 7+ (ordered by unit ID)
         *  3. Up to N* war machines will use FREE indexes 7+, if any (ordered by unit ID).
         *  4. Remaining units from 2. and 3. will use FREE indexes from 1, if any (ordered by unit ID).
         *  5. Remaining units from 4. will be ignored.
         */
        auto queue = GetQueue(battle, astack, isMorale);
        auto summons = std::array<std::deque<const CStack*>, 2> {};
        auto machines = std::array<std::deque<const CStack*>, 2> {};

        std::array<std::bitset<7>, 2> used;

        for (auto& cstack : cstacks) {
            auto slot = cstack->unitSlot();
            auto side = cstack->unitSide();
            auto &sidestacks = stacks.at(cstack->unitSide());

            if (slot >= 0) {
                used.at(side).set(slot);
                sidestacks.at(slot) = std::make_shared<Stack>(cstack, slot, queue);
            } else if (slot == SlotID::SUMMONED_SLOT_PLACEHOLDER) {
                summons.at(side).push_back(cstack);
            } else if (slot == SlotID::WAR_MACHINES_SLOT) {
                machines.at(side).push_back(cstack);
            } // arrow towers ignored
        }

        auto freeslots = std::array<std::deque<int>, 2> {};
        auto ignored = 0;

        for (auto side : {0, 1}) {
            auto &sideslots = freeslots.at(side);
            auto &sideused = used.at(side);

            // First use "extra" slots 7, 8, 9...
            for (int i=7; i<MAX_STACKS_PER_SIDE; i++)
                sideslots.push_back(i);

            // Only then re-use "regular" slots
            for (int i=0; i<7; i++)
                if (!sideused.test(i))
                    sideslots.push_back(i);

            auto &sidestacks = stacks.at(side);
            auto &sidesummons = summons.at(side);
            auto &sidemachines = machines.at(side);

            // First insert the summons, only then add the war machines
            auto extras = std::deque<const CStack*> {};
            extras.insert(extras.end(), sidesummons.begin(), sidesummons.end());
            extras.insert(extras.end(), sidemachines.begin(), sidemachines.end());

            while (!sideslots.empty() && !extras.empty()) {
                auto cstack = extras.front();
                auto slot = sideslots.front();
                sidestacks.at(slot) = std::make_shared<Stack>(cstack, slot, queue);
                extras.pop_front();
                sideslots.pop_front();
            }

            ignored += extras.size();
        }

        for (auto &stack : stacks.at(1)) {
            if (stack) stack->attrs.at(EI(SA::ID)) += 10;
        }

        if (ignored > 0)
            logAi->warn("%d summoned and/or machine stacks were excluded from state");

        return std::make_shared<Stacks>(std::move(stacks));
    }

}
