/*
 * global_stats.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BAI/v15/graph/nodes/global.h"
#include "battle/BattleSide.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
using CombatResult = Schema::V15::CombatResult;
using GlobalAction = Schema::V15::GlobalAction;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

using GlobalAttribute = Schema::V15::Graph::NodeAttributes::Global;
using GlobalStats = Graph::Nodes::Global;
}
