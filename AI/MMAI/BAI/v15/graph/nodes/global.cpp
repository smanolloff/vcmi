#include "BAI/v15/graph/nodes/global.h"

namespace MMAI::BAI::V15::Graph::Nodes
{

Global::Global(
    S15::CombatResult res,
    int round,
    int value,
    int hp,
    TowerFlags towers,
    CorpseFlags corpses
)
{
    setattr(A::BATTLE_WINNER, res == S15::CombatResult::NONE ? S15::NULL_VALUE_UNENCODED : EU(res));
    setattr(A::BATTLE_ROUND, round);
    setattr(A::HAS_UPPER_TOWER, towers.hasUpperTower);
    setattr(A::HAS_MIDDLE_TOWER, towers.hasMiddleTower);
    setattr(A::HAS_BOTTOM_TOWER, towers.hasBottomTower);
    setattr(A::HAS_GATE_CORPSE, corpses.hasGateCorpse);
    setattr(A::HAS_BRIDGE_CORPSE, corpses.hasBridgeCorpse);

    static_assert(EU(A::_count) == 7);
}

}
