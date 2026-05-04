#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/nodes/player.h"

#include "AI/MMAI/common.h"

namespace MMAI::BAI::V15::Graph::Nodes
{

Player::Player(
    BattleSide side,
    int globalValueStart,
    int globalHpStart,
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

    setattr(A::ARMY_VALUE_NOW_ABS, value);
    setattr(A::ARMY_VALUE_NOW_REL, permille(value, globalValuePrevRound));
    setattr(A::ARMY_VALUE_NOW_REL0, permille(value, globalValueStart));
    setattr(A::ARMY_HP_NOW_ABS, hp);
    setattr(A::ARMY_HP_NOW_REL, permille(hp, globalHpPrevRound));
    setattr(A::ARMY_HP_NOW_REL0, permille(hp, globalHpStart));
    setattr(A::VALUE_KILLED_NOW_ABS, valueKilled);
    setattr(A::VALUE_KILLED_NOW_REL, permille(valueKilled, globalValuePrevRound));
    addattr(A::VALUE_KILLED_ACC_ABS, valueKilled);
    setattr(A::VALUE_KILLED_ACC_REL0, permille(attr(A::VALUE_KILLED_ACC_ABS), globalValueStart));
    setattr(A::VALUE_LOST_NOW_ABS, valueLost);
    setattr(A::VALUE_LOST_NOW_REL, permille(valueLost, globalValuePrevRound));
    addattr(A::VALUE_LOST_ACC_ABS, valueLost);
    setattr(A::VALUE_LOST_ACC_REL0, permille(attr(A::VALUE_LOST_ACC_ABS), globalValueStart));
    setattr(A::DMG_DEALT_NOW_ABS, dmgDealt);
    setattr(A::DMG_DEALT_NOW_REL, permille(dmgDealt, globalHpPrevRound));
    addattr(A::DMG_DEALT_ACC_ABS, dmgDealt);
    setattr(A::DMG_DEALT_ACC_REL0, permille(attr(A::DMG_DEALT_ACC_ABS), globalHpStart));
    setattr(A::DMG_RECEIVED_NOW_ABS, dmgReceived);
    setattr(A::DMG_RECEIVED_NOW_REL, permille(dmgReceived, globalHpPrevRound));
    addattr(A::DMG_RECEIVED_ACC_ABS, dmgReceived);
    setattr(A::DMG_RECEIVED_ACC_REL0, permille(attr(A::DMG_RECEIVED_ACC_ABS), globalHpStart));

    static_assert(EU(A::_count) == 23, "whistleblower in case attributes change");
}

}
