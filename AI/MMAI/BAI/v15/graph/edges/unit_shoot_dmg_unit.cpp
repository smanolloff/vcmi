#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"

namespace MMAI::BAI::V15::Graph::Edges
{

Unit_ShootDmg_Unit::Unit_ShootDmg_Unit(
    const std::shared_ptr<const Nodes::Unit> & srcNode,
    const std::shared_ptr<const Nodes::Unit> & dstNode,
    int vdiffAttacker,
    int vdiffDefender,
    int hpdiffAttacker,
    int hpdiffDefender,
    int battlefieldValue,
    int battlefieldHp
) : detail::Unit_ShootDmg_Unit_Base(srcNode, dstNode)
{
    int netValue = vdiffAttacker - vdiffDefender;
    int attackerHp = static_cast<int>(srcNode->cstack.getTotalHealth());
    int defenderHp = static_cast<int>(dstNode->cstack.getTotalHealth());

    setattr(A::ESTIMATED_NET_VALUE_REL_BF, permille(netValue, battlefieldValue));
    setattr(A::ESTIMATED_ATTACKER_HPDIFF_REL_SELF, permille(hpdiffAttacker, attackerHp));
    setattr(A::ESTIMATED_ATTACKER_HPDIFF_REL_BF, permille(hpdiffAttacker, battlefieldHp));
    setattr(A::ESTIMATED_DEFENDER_HPDIFF_REL_SELF, permille(hpdiffDefender, defenderHp));
    setattr(A::ESTIMATED_DEFENDER_HPDIFF_REL_BF, permille(hpdiffDefender, battlefieldHp));

    static_assert(static_cast<size_t>(A::_count) == 5, "whistleblower in case attributes change");
}

}
