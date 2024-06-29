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

#include "./hex.h"
#include "./hexactmask.h"
#include "Global.h"
#include "schema/v3/constants.h"
#include <mach/vm_region.h>

namespace MMAI::BAI::V3 {
    using A = Schema::V3::HexAttribute;
    using S = Schema::V3::HexState;
    using SA = Schema::V3::StackAttribute;

    constexpr HexStateMask S_PASSABLE = 1<<EI(HexState::PASSABLE);
    constexpr HexStateMask S_STOPPING = 1<<EI(HexState::STOPPING);
    constexpr HexStateMask S_DAMAGING = 1<<EI(HexState::DAMAGING);
    // constexpr HexStateMask S_GATE = 1<<EI(HexState::GATE);

    // static
    int Hex::CalcId(const BattleHex &bh) {
        ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.hex));
        return bh.getX()-1 + bh.getY()*BF_XMAX;
    }

    // static
    std::pair<int, int> Hex::CalcXY(const BattleHex &bh) {
        return {bh.getX() - 1, bh.getY()};
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
    // static
    HexActionHex Hex::NearbyBattleHexes(const BattleHex &bh) {
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

    Hex::Hex(
        const BattleHex &bhex_,
        const EAccessibility accessibility,
        const EGateState gatestate,
        const std::vector<std::shared_ptr<const CObstacleInstance>> &obstacles,
        const std::map<BattleHex, std::shared_ptr<Stack>> &hexstacks,
        const std::shared_ptr<ActiveStackInfo> &astackinfo
    ) : bhex(bhex_) {
        attrs.fill(NULL_VALUE_UNENCODED);

        auto [x, y] = CalcXY(bhex);
        auto it = hexstacks.find(bhex);
        stack = it == hexstacks.end() ? nullptr : it->second;

        setattr(A::Y_COORD, y);
        setattr(A::X_COORD, x);

        if (stack)
            setattr(A::STACK_ID, stack->attr(SA::ID));

        if (astackinfo) {
            setStateMask(accessibility, obstacles, astackinfo->stack->attr(SA::SIDE));
            setActionMask(astackinfo, hexstacks);
        } else {
            setStateMask(accessibility, obstacles, 0);
        }

        finalize();
    }

    const HexAttrs& Hex::getAttrs() const {
        return attrs;
    }

    int Hex::getAttr(HexAttribute a) const {
        return attr(a);
    }

    int Hex::attr(HexAttribute a) const { return attrs.at(EI(a)); };
    void Hex::setattr(HexAttribute a, int value) {
        attrs.at(EI(a)) = std::min(value, std::get<3>(HEX_ENCODING.at(EI(a))));
    };

    std::string Hex::name() const {
        // return boost::str(boost::format("(%d,%d)") % attr(A::Y_COORD) % attr(A::X_COORD));
        return "(" + std::to_string(attr(A::Y_COORD)) + "," + std::to_string(attr(A::X_COORD)) + ")";
    }

    void Hex::finalize() {
        attrs.at(EI(A::ACTION_MASK)) = actmask.to_ulong();
        attrs.at(EI(A::STATE_MASK)) = statemask.to_ulong();
    }

    void Hex::addAction(HexAction action) {
        actmask.set(EI(action));
    }

    void Hex::addState(HexState state) {
        statemask.set(EI(state));
    }

    // private

    void Hex::setStateMask(
        const EAccessibility accessibility,
        const std::vector<std::shared_ptr<const CObstacleInstance>> &obstacles,
        bool side
    ) {
        // First process obstacles
        // XXX: set only non-PASSABLE flags
        // (e.g. there may be a stack standing on the obstacle (firewall, moat))
        // so the PASSABLE mask bit will set later
        // XXX: moats are a weird obstacle:
        //      * if dispellable (Tower mines?) => type=SPELL_CREATED
        //      * otherwise => type=MOAT
        //      * their trigger ability is a spell, as it seems
        //        (which is not found in spells.json, neither is available as a SpellID constant)
        //
        //      Ref: Moat::placeObstacles()
        //           BattleEvaluator::goTowardsNearest() // var triggerAbility
        //
        for (auto &obstacle : obstacles) {
            switch (obstacle->obstacleType) {
            break; case CObstacleInstance::USUAL:               statemask &= ~S_PASSABLE; // impassable
            break; case CObstacleInstance::ABSOLUTE_OBSTACLE:   statemask &= ~S_PASSABLE;
            break; case CObstacleInstance::MOAT:                statemask |= S_STOPPING | S_DAMAGING;
            break; case CObstacleInstance::SPELL_CREATED:
                switch(obstacle->getTrigger()) {
                break; case SpellID::FORCE_FIELD:   statemask &= ~S_PASSABLE;
                break; case SpellID::QUICKSAND:     statemask |= S_STOPPING;
                break; case SpellID::FIRE_WALL:     statemask |= S_DAMAGING;
                break; case SpellID::LAND_MINE:     statemask |= S_DAMAGING;
                break; default:
                    logAi->warn("Unexpected obstacle trigger: %d", EI(obstacle->getTrigger()));
                }
            break; default:
                THROW_FORMAT("Unexpected obstacle type: %d", EI(obstacle->obstacleType));
            }
        }

        switch(accessibility) {
        break; case EAccessibility::ACCESSIBLE:
            ASSERT(!stack, "accessibility is ACCESSIBLE, but a stack was found on hex");
            statemask |= S_PASSABLE;
        break; case EAccessibility::OBSTACLE:
            ASSERT(!stack, "accessibility is OBSTACLE, but a stack was found on hex");
            statemask &= ~S_PASSABLE;
        break; case EAccessibility::ALIVE_STACK:
            ASSERT(stack, "accessibility is ALIVE_STACK, but no stack was found on hex");
            statemask &= ~S_PASSABLE;
        break; case EAccessibility::DESTRUCTIBLE_WALL:
            // XXX: Destroyed walls become ACCESSIBLE.
            ASSERT(!stack, "accessibility is DESTRUCTIBLE_WALL, but a stack was found on hex");
            statemask &= ~S_PASSABLE;
        break; case EAccessibility::GATE:
            // See BattleProcessor::updateGateState() for gate states
            // See CBattleInfoCallback::getAccesibility() for accessibility on gate
            //
            // TL; DR:
            // -> GATE means closed, non-blocked gate
            // -> UNAVAILABLE means blocked
            // -> ACCESSIBLE otherwise (open, destroyed)
            //
            // Regardless of the gate state, we always set the GATE flag
            // purely based on the hex coordinates and not on the accessibility
            // => not setting GATE flag here
            //
            // However, in case of GATE accessibility, we still need
            // to set the PASSABLE flag accordingly.
            side
                ? statemask.set(EI(S::PASSABLE))
                : statemask.reset(EI(S::PASSABLE));
        break; case EAccessibility::UNAVAILABLE:
            statemask &= ~S_PASSABLE;
        break; default:
            THROW_FORMAT("Unexpected hex accessibility for bhex %d: %d", bhex.hex % EI(accessibility));
        }

        // if (bhex == BattleHex::GATE_INNER || bhex == BattleHex::GATE_OUTER)
        //     statemask |= S_GATE;
    }

    void Hex::setActionMask(
        const std::shared_ptr<ActiveStackInfo> &astackinfo,
        const std::map<BattleHex, std::shared_ptr<Stack>> &hexstacks
    ) {
        auto astack = astackinfo->stack;

        // XXX: ReachabilityInfo::isReachable() must not be used as it
        //      returns true even if speed is insufficient => use distances.
        // NOTE: distances is 0 for the stack's main hex and 1 for its rear hex
        //       (100000 if it can't fit there)
        if (astackinfo->rinfo->distances.at(bhex) <= astack->attr(SA::SPEED))
            actmask.set(EI(HexAction::MOVE));

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
        if (astack->attr(SA::SHOOTER) && stack && stack->attr(SA::SIDE) != astack->attr(SA::SIDE))
            actmask.set(EI(HexAction::SHOOT));


        const auto &nbhexes = NearbyBattleHexes(bhex);
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
                    ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bhex), "vcmi says melee attack is IMPOSSIBLE [1]");
                    actmask.set(i);
                } else if (hexaction <= HexAction::AMOVE_2BR) {
                    // only wide R stacks can perform 2TR/2R/2BR attacks
                    if (a_cstack->unitSide() == 1 && a_cstack->doubleWide()) {
                        ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bhex), "vcmi says melee attack is IMPOSSIBLE [2]");
                        actmask.set(i);
                    }
                } else {
                    // only wide L stacks can perform 2TL/2L/2BL attacks
                    if (a_cstack->unitSide() == 0 && a_cstack->doubleWide()) {
                        ASSERT(CStack::isMeleeAttackPossible(a_cstack, n_cstack, bhex), "vcmi says melee attack is IMPOSSIBLE");
                        actmask.set(i);
                    }
                }
            }
        }
    }

}
