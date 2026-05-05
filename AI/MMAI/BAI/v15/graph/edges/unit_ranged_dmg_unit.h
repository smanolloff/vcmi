/*
 * unit_ranged_dmg_unit.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "battle/IBattleInfoCallback.h"
#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
	using Unit_RangedDmg_Unit_Traits = S15::EncodingTraits<S15::EdgeEncoding_Unit_RangedDmg_Unit>;
	using Unit_RangedDmg_Unit_Base = Base<Nodes::Unit, Nodes::Unit, Unit_RangedDmg_Unit_Traits>;
}

class Unit_RangedDmg_Unit : public detail::Unit_RangedDmg_Unit_Base
{
public:
	Unit_RangedDmg_Unit(
		const Nodes::Unit & srcNode,
		const Nodes::Unit & dstNode,
		const DamageEstimation & attackEstimate,
		int battlefieldValue,
		int battlefieldHp
	);
};
}
