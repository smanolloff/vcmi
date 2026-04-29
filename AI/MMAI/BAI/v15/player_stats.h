/*
 * player_stats.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BAI/v15/graph/nodes/player.h"
#include "BAI/v15/global_stats.h"
#include "battle/BattleSide.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
using PlayerAttribute = Schema::V15::Graph::NodeAttributes::Player;
using PlayerStats = Graph::Nodes::Player;
}
