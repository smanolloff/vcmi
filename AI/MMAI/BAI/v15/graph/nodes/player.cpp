#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/nodes/player.h"

#include "AI/MMAI/common.h"

namespace MMAI::BAI::V15::Graph::Nodes
{

Player::Player(
    BattleSide side,
    bool isActive,
    int globalValuePrevRound,
    int globalHpPrevRound,
    int value,
    int hp,
    int dmgDealt,
    int dmgReceived,
    int valueKilled,
    int valueLost
) : side(side)
{
    setattr(A::BATTLE_SIDE, EU(side));
    setattr(A::IS_ACTIVE, isActive);

    setattr(A::ARMY_VALUE_NOW_REL, permille(value, globalValuePrevRound));
    setattr(A::ARMY_HP_NOW_REL, permille(hp, globalHpPrevRound));
    setattr(A::VALUE_KILLED_NOW_REL, permille(valueKilled, globalValuePrevRound));
    setattr(A::VALUE_LOST_NOW_REL, permille(valueLost, globalValuePrevRound));
    setattr(A::DMG_DEALT_NOW_REL, permille(dmgDealt, globalHpPrevRound));
    setattr(A::DMG_RECEIVED_NOW_REL, permille(dmgReceived, globalHpPrevRound));

    static_assert(EU(A::_count) == 8, "whistleblower in case attributes change");
}

}
