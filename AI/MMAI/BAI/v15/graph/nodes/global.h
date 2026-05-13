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

#include "BAI/v15/graph/nodes/base.h"
#include "battle/BattleSide.h"
#include "common.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

#include <bitset>

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

namespace detail
{
	using Global_Traits = S15::EncodingTraits<S15::Graph::NodeAttributes::Global>;
	using Global_Base = Base<Global_Traits>;
}

class Global : public detail::Global_Base
{
public:
	// TODO: make these 3 and 2 some enum (because they are also referenced in encoding)
	using TowerFlags = std::bitset<3>;
	using CorpseFlags = std::bitset<2>;

	struct extra_index_type {
		using result_type = int;
		result_type operator()(const std::shared_ptr<const Global> & _global) const {
			// There can't be two Global nodes
			// Make sure attempt to insert another one causes an index conflict
			return 0;
		}
	};

	Global(
		BattleSide side,
		S15::CombatResult res,
		int round,
		int valueStart,
		int value,
		int hpStart,
		int hp,
		TowerFlags towers,
		CorpseFlags corpses
    );
};

}
