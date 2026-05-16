#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"
#include <numbers>

namespace MMAI::BAI::V15::Graph::Edges
{

namespace {
    // chance to deal at least `target` dmg
    double dmgChance(int target, int mindmg, int maxdmg, int k)
    {
        if (maxdmg < target || k <= 0)
            return 0.0;
        if (mindmg >= target)
            return 1.0;

        double mean = 0.5 * (mindmg + maxdmg);
        double variance = ((maxdmg - mindmg) * (maxdmg - mindmg)) / (12.0 * k);
        double stddev = std::sqrt(variance);
        double z = (target - mean) / stddev;
        double normalCdf = 0.5 * (1.0 + std::erf(z / std::numbers::sqrt2));
        return 1.0 - normalCdf;
    }
}

Unit_ShootDmg_Unit::Unit_ShootDmg_Unit(
    const std::shared_ptr<const Nodes::Unit> & srcNode,
    const std::shared_ptr<const Nodes::Unit> & dstNode,
    const DamageEstimation & attackEstimate,
    int battlefieldValue,
    int battlefieldHp
) : detail::Unit_ShootDmg_Unit_Base(srcNode, dstNode)
{
    const auto & A_cstack = srcNode->cstack;
    const auto & B_cstack = dstNode->cstack;

    auto A_dmg_min = static_cast<int>(attackEstimate.damage.min);
    auto A_dmg_max = static_cast<int>(attackEstimate.damage.max);
    auto A_dmg_range = A_dmg_max - A_dmg_min;
    auto A_n = A_cstack.getCount();
    auto A_k = std::min(A_n, 10); // see BattleInfo::getActualDamage()
    auto A_dmg_mean = 0.5 * (A_dmg_min + A_dmg_max);
    auto A_dmg_std = std::sqrt((A_dmg_range * A_dmg_range) / (12.0 * A_k));
    auto B_hp = static_cast<int>(B_cstack.getAvailableHealth());
    auto B_firstHpLeft = static_cast<int>(B_cstack.getFirstHPleft());
    auto A_kills_mean = A_dmg_mean / B_hp;
    auto A_onekill_chance = dmgChance(B_firstHpLeft, A_dmg_min, A_dmg_max, A_k);
    auto A_allkill_chance = dmgChance(B_hp, A_dmg_min, A_dmg_max, A_k);

    setattr(A::ATTACK_DMG_MEAN_REL_OTHER, permille(A_dmg_mean, B_hp));
    setattr(A::ATTACK_DMG_MEAN_REL_BF, permille(A_dmg_mean, battlefieldHp));
    setattr(A::ATTACK_DMG_STD_REL_OTHER, permille(A_dmg_std, B_hp));
    setattr(A::ATTACK_DMG_STD_REL_BF, permille(A_dmg_std, battlefieldHp));
    setattr(A::ATTACK_VALUE_REL_BF, permille(A_kills_mean * Nodes::Unit::GetValue(B_cstack.unitType()), battlefieldValue));
    setattr(A::ATTACK_ONEKILL_CHANCE, permille(A_onekill_chance, 1));
    setattr(A::ATTACK_ALLKILL_CHANCE, permille(A_allkill_chance, 1));

    static_assert(static_cast<size_t>(A::_count) == 7, "whistleblower in case attributes change");
}

}
