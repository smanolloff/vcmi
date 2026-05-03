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

#include "BAI/v15/graph/element.h"
#include "battle/BattleSide.h"
#include "common.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

#include <array>
#include <bitset>

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

class Global : public Element<S15::Graph::INode, S15::EncodingTraits<S15::GlobalEncoding>>
{
	using A = S15::Graph::NodeAttributes::Global;
	using CombatResult = S15::CombatResult;
	using GlobalAction = S15::GlobalAction;
	using GlobalActionMask = std::bitset<EU(GlobalAction::_count)>;

public:
	using TowerFlags = std::bitset<3>;
	using CorpseFlags = std::bitset<2>;

	Global(
		BattleSide side,
		CombatResult res,
		int round,
		int valueStart,
		int value,
		int hpStart,
		int hp,
		TowerFlags towers,
		CorpseFlags corpses
    );

	std::array<int, EU(A::_count)> attrs = {};
	GlobalActionMask actmask = 0;
};

}
