/*
 * global.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

// #include "battle/BattleSide.h"
// #include "common.h"
#include "schema/v15/graph.h"
// #include "schema/v15/types.h"
// #include "global.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using UA = S15::Graph::NodeAttributes::Unit;
using CombatResult = Schema::V15::CombatResult;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Unit : Schema::V15::Graph::INode
{
public:
    Unit(BattleSide side, int value, int hp);

    S15::Graph::NodeType getType() const override;
    std::vector<float> encodedAttributes() const override;

    int getAttr(PA a) const;
    int attr(PA a) const;
    void update(const Global * global, int value, int hp, int dmgDealt, int dmgReceived, int valueKilled, int valueLost);
    void setattr(PA a, int value);
    std::array<int, EU(PA::_count)> attrs = {};
};
}
