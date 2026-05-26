/*
 * generic.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/global.h"
#include "BAI/v15/graph/nodes/hex.h"
#include "BAI/v15/graph/nodes/player.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "BAI/v15/graph/nodes/action.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

/*
 * The macro is useful for generic edges which have no attributes.
 *
 * GENERIC_EDGE_ELEMENT(NodeA, Edge, NodeB) expands to:
 *
 *     using NodeA_Adjacent_NodeB_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::NodeA_Edge_NodeB>;
 *     using NodeA_Edge_NodeB_Base = Base<Nodes::NodeA, Nodes::NodeB, NodeA_Edge_NodeB_Traits>;
 *     class NodeA_Edge_NodeA : public S15::EncodingTraits<
 *         NodeA,
 *         NodeB,
 *         S15::EncodingTraits<S15::EdgeEncoding_NodeA_Edge_NodeB>
 *     > {};
 */
#define GENERIC_EDGE_ELEMENT(NodeA, Edge, NodeB) \
namespace detail \
{ \
    using NodeA##_##Edge##_##NodeB##_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::NodeA##_##Edge##_##NodeB>; \
    using NodeA##_##Edge##_##NodeB##_Base = Base<Nodes::NodeA, Nodes::NodeB, NodeA##_##Edge##_##NodeB##_Traits>; \
} \
class NodeA##_##Edge##_##NodeB : public detail::NodeA##_##Edge##_##NodeB##_Base \
{ \
public: \
    using detail::NodeA##_##Edge##_##NodeB##_Base::NodeA##_##Edge##_##NodeB##_Base; \
    static_assert(EU(A::_count) == 0, "generic edges cannot have attributes"); \
    static std::shared_ptr<const NodeA##_##Edge##_##NodeB> Create( \
        const std::shared_ptr<const Nodes::NodeA> & srcNode, \
        const std::shared_ptr<const Nodes::NodeB> & dstNode) \
    { \
        return std::make_shared<const NodeA##_##Edge##_##NodeB>(srcNode, dstNode); \
    } \
}

GENERIC_EDGE_ELEMENT(Global, Has, Player);
GENERIC_EDGE_ELEMENT(Global, Has, Unit);
GENERIC_EDGE_ELEMENT(Global, Has, Hex);
GENERIC_EDGE_ELEMENT(Global, Has, Action);

GENERIC_EDGE_ELEMENT(Player, Owns, Unit);

GENERIC_EDGE_ELEMENT(Unit, Blocks, Unit);
GENERIC_EDGE_ELEMENT(Unit, Occupies, Hex);

GENERIC_EDGE_ELEMENT(Action, By, Unit);
GENERIC_EDGE_ELEMENT(Action, Blocks, Unit);
GENERIC_EDGE_ELEMENT(Unit, BecomesMeleeThreatAfter, Action);
GENERIC_EDGE_ELEMENT(Unit, BecomesMeleeTargetAfter, Action);
GENERIC_EDGE_ELEMENT(Hex, BecomesMeleeTargetAfter, Action);

}

/*
    auto cstacks = battle.battleGetStacks();

    // Sorting needed to ensure ordered insertion of summons/machines
    std::ranges::sort(
        cstacks,
        [](const CStack * a, const CStack * b)
        {
            return a->unitId() < b->unitId();
        }
    );

    auto blocking = std::map<const CStack *, bool>{};
    auto blocked = std::map<const CStack *, bool>{};

    auto setBlockedBlocking = [&battle, &blocked, &blocking](const CStack * cstack)
    {
        blocked.emplace(cstack, false);
        blocking.emplace(cstack, false);

        for(const auto * adjacent : battle.battleAdjacentUnits(cstack))
        {
            if(adjacent->unitOwner() == cstack->unitOwner())
                continue;

            // XXX: battleIsUnitBlocked can return true for ballista => don't use
            //      (though properly detects blocked by frenzied ally)
            if(!blocked[cstack] && cstack->canShoot() && !cstack->hasBonusOfType(BonusType::FREE_SHOOTING) && !cstack->hasBonusOfType(BonusType::SIEGE_WEAPON))
            {
                blocked[cstack] = true;
            }
            if(!blocking[cstack] && adjacent->canShoot() && !adjacent->hasBonusOfType(BonusType::FREE_SHOOTING)
               && !adjacent->hasBonusOfType(BonusType::SIEGE_WEAPON))
            {
                blocking[cstack] = true;
            }
        }
    };

    // estimated dmg by active stack
    // values are for ranged attack if unit is an unblocked shooter
    // otherwise for melee attack
    auto estdmg = std::map<const CStack *, DamageEstimation>{};

    auto estimateDamage = [&battle, &astack, &estdmg, &blocked](const CStack * cstack)
    {
        if(!astack)
        {
            // no active stack (e.g. called during battleStart or battleEnd)
            estdmg.try_emplace(cstack);
        }
        else if(astack->unitSide() == cstack->unitSide())
        {
            // no damage to friendly units
            estdmg.try_emplace(cstack);
        }
        else
        {
            const auto attinfo = BattleAttackInfo(astack, cstack, 0, astack->canShoot() && !blocked[astack]);
            estdmg.try_emplace(cstack, battle.calculateDmgRange(attinfo));
        }
    };

    // This must be pre-set as dmg estimation depends on it
    if(astack)
        setBlockedBlocking(astack);

    for(auto & cstack : battle.battleGetStacks())
    {
        if(cstack != astack)
            setBlockedBlocking(cstack);

        estimateDamage(cstack);
        G.add(Graph::Nodes::Unit(...)

*/

