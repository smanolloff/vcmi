/*
 * unit_acts_before_unit.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;
using Unit_ActsBefore_Unit_Traits = S15::EncodingTraits<S15::EdgeEncoding_Unit_ActsBefore_Unit>;
using Unit_ActsBefore_Unit_Base = Base<Nodes::Unit, Nodes::Unit, Unit_ActsBefore_Unit_Traits>;

class Unit_ActsBefore_Unit : public Unit_ActsBefore_Unit_Base
{
public:
	Unit_ActsBefore_Unit(
		const Nodes::Unit & srcNode,
		const Nodes::Unit & dstNode,
		int times
	) : Unit_ActsBefore_Unit_Base(srcNode, dstNode)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		setattr(A::TIMES, times);
		static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
	}
};
}
