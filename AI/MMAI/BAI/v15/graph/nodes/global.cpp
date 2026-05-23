#include "BAI/v15/graph/nodes/global.h"

namespace MMAI::BAI::V15::Graph::Nodes
{

Global::Global(const Args & args)
{
    setattr(A::BATTLE_WINNER, args.res == S15::CombatResult::NONE ? S15::NULL_VALUE_UNENCODED : EU(args.res));
    setattr(A::BATTLE_ROUND, args.round);
    setattr(A::HAS_UPPER_TOWER, args.towers.hasUpperTower);
    setattr(A::HAS_MIDDLE_TOWER, args.towers.hasMiddleTower);
    setattr(A::HAS_BOTTOM_TOWER, args.towers.hasBottomTower);
    setattr(A::HAS_GATE_CORPSE, args.corpses.hasGateCorpse);
    setattr(A::HAS_BRIDGE_CORPSE, args.corpses.hasBridgeCorpse);

    static_assert(EU(A::_count) == 7);
}

}
