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
#include "BAI/v3/hex.h"
#include "Global.h"
#include "battle/AccessibilityInfo.h"
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
        StackMapping stackmapping_,
        const Stack* astack_
    ) : info(info_)
      , hexes(hexes_)
      , stacks(stacks_)
      , stackmapping(stackmapping_)
      , astack(astack_) {};

    // static
    std::shared_ptr<const Battlefield> Battlefield::Create(
        const CPlayerBattleCallback* battle,
        const CStack* acstack,
        const ArmyValues av,
        bool isMorale
    ) {
        auto [stacks, mapping] = InitStacks(battle, acstack, isMorale);
        auto [hexes, astack] = InitHexes(battle, acstack, stacks);
        auto info = std::make_shared<GeneralInfo>(battle, av);

        return std::make_shared<const Battlefield>(info, hexes, stacks, mapping, astack);
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
    std::tuple<std::shared_ptr<Hexes>, Stack*> Battlefield::InitHexes(
        const CPlayerBattleCallback* battle,
        const CStack* acstack,
        const std::shared_ptr<Stacks> stacks
    ) {
        auto res = std::make_shared<Hexes>();
        auto ainfo = battle->getAccesibility();
        auto hexstacks = std::map<BattleHex, std::shared_ptr<Stack>> {};
        auto hexobstacles = std::array<std::vector<std::shared_ptr<const CObstacleInstance>>, 165> {};

        std::shared_ptr<ActiveStackInfo> astackinfo = nullptr;
        Stack* astack = nullptr;

        for (auto &sidestacks : *stacks) {
            for (auto &stack : sidestacks) {
                if (!stack)
                    continue;

                for (auto &bh : stack->cstack->getHexes())
                    if (bh.isAvailable())
                        hexstacks.insert({bh, stack});

                // XXX: at battle_end, stack->cstack != acstack even if qpos=0
                if (stack->attr(SA::QUEUE_POS) == 0 && acstack)
                    astack = stack.get();
            }
        }

        for (auto &obstacle : battle->battleGetAllObstacles())
            for (auto &bh : obstacle->getAffectedTiles())
                if (bh.isAvailable())
                    hexobstacles.at(Hex::CalcId(bh)).push_back(obstacle);;

        std::shared_ptr<ReachabilityInfo> arinfo = nullptr;

        if (astack) {
            astackinfo = std::make_shared<ActiveStackInfo>(
                astack,
                battle->battleCanShoot(astack->cstack),
                std::make_shared<ReachabilityInfo>(battle->getReachability(astack->cstack))
            );
        } else if (acstack) {
            // The only situation astack is NULL, but acstack is NOT NULL,
            // is when army stacks exceeds MAX_STACKS_PER_SIDE and the active
            // stack is one of the ignored stacks (i.e. excluded from state)
            auto mystacks = stacks->at(battle->battleGetMySide());
            ASSERT(mystacks.size() == MAX_STACKS_PER_SIDE, "Active stack not found");
            // In this case the AI will fallback to a wait/defend action.
        }

        auto gatestate = battle->battleGetGateState();

        for (int y=0; y<11; ++y) {
            for (int x=0; x<15; ++x) {
                auto i = y*15 + x;
                auto bh = BattleHex(x+1, y);
                res->at(y).at(x) = std::make_unique<Hex>(
                    bh, ainfo.at(bh), gatestate, hexobstacles.at(i),
                    hexstacks, astackinfo
                );
            }
        }

        // XXX: astack can be nullptr (even if acstack is not) -- see above
        return {res, astack};
    };

    // static
    std::tuple<std::shared_ptr<Stacks>, StackMapping> Battlefield::InitStacks(
        const CPlayerBattleCallback* battle,
        const CStack* astack,
        bool isMorale
    ) {
        auto stacks = std::make_shared<Stacks>();
        auto mapping = StackMapping{};
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
            auto &sidestacks = stacks->at(cstack->unitSide());

            if (slot >= 0) {
                used.at(side).set(slot);
                auto stack = std::make_shared<Stack>(cstack, slot, queue);
                sidestacks.at(slot) = stack;
                mapping.insert({cstack, stack});
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

            auto &sidestacks = stacks->at(side);
            auto &sidesummons = summons.at(side);
            auto &sidemachines = machines.at(side);

            // First insert the summons, only then add the war machines
            auto extras = std::deque<const CStack*> {};
            extras.insert(extras.end(), sidesummons.begin(), sidesummons.end());
            extras.insert(extras.end(), sidemachines.begin(), sidemachines.end());

            while (!sideslots.empty() && !extras.empty()) {
                auto cstack = extras.front();
                auto slot = sideslots.front();
                auto stack = std::make_shared<Stack>(cstack, slot, queue);
                sidestacks.at(slot) = stack;
                mapping.insert({cstack, stack});
                extras.pop_front();
                sideslots.pop_front();
            }

            ignored += extras.size();
        }

        for (auto &stack : stacks->at(1)) {
            if (stack) stack->attrs.at(EI(SA::ID)) += 10;
        }

        if (ignored > 0) // "stack overflow" :)
            logAi->warn("%d war machines and/or summoned stacks were excluded from state", ignored);

        return {stacks, mapping};
    }

}
