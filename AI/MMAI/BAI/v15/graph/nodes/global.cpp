#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/nodes/global.h"
#include <limits>

namespace MMAI::BAI::V15::Graph::Nodes
{

Global::Global(
    BattleSide side,
    CombatResult res,
    int round,
    int valueStart,
    int value,
    int hpStart,
    int hp,
    TowerFlags towers,
    CorpseFlags corpses
)
{
    setattr(A::BATTLE_ROUND, round);

    (res == CombatResult::NONE)
        ? setattr(A::BATTLE_WINNER, S15::NULL_VALUE_UNENCODED)
        : setattr(A::BATTLE_WINNER, EU(res));

    (side == BattleSide::NONE)
        ? setattr(A::BATTLE_SIDE_ACTIVE_PLAYER, S15::NULL_VALUE_UNENCODED)
        : setattr(A::BATTLE_SIDE_ACTIVE_PLAYER, EU(side));

    setattr(A::BFIELD_VALUE_START_ABS, valueStart);
    setattr(A::BFIELD_VALUE_NOW_ABS, value);
    setattr(A::BFIELD_VALUE_NOW_REL0, permille(value, valueStart));
    setattr(A::BFIELD_HP_START_ABS, hpStart);
    setattr(A::BFIELD_HP_NOW_ABS, hp);
    setattr(A::BFIELD_HP_NOW_REL0, permille(hp, hpStart));

    static_assert(TowerFlags{}.size() < std::numeric_limits<int>::digits);
    setattr(A::SIEGE_TOWERS, static_cast<int>(towers.to_ulong()));

    static_assert(CorpseFlags{}.size() < std::numeric_limits<int>::digits);
    setattr(A::SIEGE_CORPSES, static_cast<int>(corpses.to_ulong()));
}

}
