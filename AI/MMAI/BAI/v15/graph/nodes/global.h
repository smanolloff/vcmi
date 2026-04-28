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

#include "battle/BattleSide.h"
#include "common.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using GA = S15::Graph::NodeAttributes::Global;
using CombatResult = Schema::V15::CombatResult;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Global : Schema::V15::Graph::INode
{
public:
    Global(BattleSide side, int value, int hp, TowerFlags towers, CorpseFlags corpses);

    S15::Graph::NodeType getType() const override;
    std::vector<float> encodedAttributes() const override;

    int getAttr(GA a) const;
    int attr(GA a) const;
    void update(BattleSide side, CombatResult res, int value, int hp, bool canWait, TowerFlags towers, CorpseFlags corpses, int round);
    void setattr(GA a, int value);
    std::array<int, EU(GA::_count)> attrs = {};
};
}
