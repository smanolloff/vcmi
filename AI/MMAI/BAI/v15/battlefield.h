/*
 * battlefield.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/cache.h"
#include "BAI/v15/graph/graph.h"
#include "battle/CPlayerBattleCallback.h"

namespace MMAI::BAI::V15::Battlefield
{
	void Init(
		Cache & cache,
		const CPlayerBattleCallback * battle,
		const CStack * acstack,
		const Graph::Graph & oldG,
		Graph::Graph & G,
		std::map<const CStack *, Graph::Nodes::Unit::Stats> & stacksStats,
		bool isMorale
	);
}
