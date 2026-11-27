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

#include "AI/MMAI/common.h" // IWYU pragma: keep

#include "BAI/v16/graph/nodes/base_v16.h"
#include "schema/v16/constants.h"
#include "schema/v16/graph.h"
#include "schema/v16/types.h"

namespace MMAI::BAI::V16::Graph::Nodes
{
namespace S16 = Schema::V16;

namespace detail
{
	using Global_Traits = S16::EncodingTraits<S16::Graph::NodeAttributes::Global>;
	using Global_Base = Base<Global_Traits>;
}

class Global : public detail::Global_Base
{
public:
	struct TowerFlags
	{
		bool hasUpperTower = false;
		bool hasMiddleTower = false;
		bool hasBottomTower = false;
	};

	struct CorpseFlags
	{
		bool hasGateCorpse = false;
		bool hasBridgeCorpse = false;
	};

	struct extra_index_type
	{
		using result_type = int;
		result_type operator()(const std::shared_ptr<const Global> & _global) const
		{
			// There can't be two Global nodes
			// Make sure attempt to insert another one causes an index conflict
			return 0;
		}
	};

	struct Args
	{
		const S16::CombatResult res;
		const int round;
		const int value;
		const int hp;
		const TowerFlags towers;
		const CorpseFlags corpses;
	};

	static std::shared_ptr<const Global> Create(const Args & args)
	{
		return std::make_shared<const Global>(args);
	}

	explicit Global(const Args & args);
};

}
