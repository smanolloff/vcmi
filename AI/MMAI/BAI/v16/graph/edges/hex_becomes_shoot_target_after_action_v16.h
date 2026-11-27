/*
 * hex_becomes_shoot_target_after_action.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "AI/MMAI/common.h" // IWYU pragma: keep

#include "BAI/v16/graph/edges/base_v16.h"
#include "BAI/v16/graph/nodes/action_v16.h"
#include "BAI/v16/graph/nodes/hex_v16.h"
#include "BAI/v16/graph/util_v16.h"
#include "schema/v16/constants.h"

namespace MMAI::BAI::V16::Graph::Edges
{
namespace S16 = Schema::V16;

namespace detail
{
	using Hex_BecomesShootTargetAfter_Action_Traits = S16::EncodingTraits<S16::Graph::EdgeAttributes::Hex_BecomesShootTargetAfter_Action>;
	using Hex_BecomesShootTargetAfter_Action_Base = Base<Nodes::Hex, Nodes::Action, Hex_BecomesShootTargetAfter_Action_Traits>;
}

class Hex_BecomesShootTargetAfter_Action : public detail::Hex_BecomesShootTargetAfter_Action_Base
{
public:
	static std::shared_ptr<const Hex_BecomesShootTargetAfter_Action>
	Create(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Action> & dstNode, float mult)
	{
		return std::make_shared<const Hex_BecomesShootTargetAfter_Action>(srcNode, dstNode, mult);
	};

	Hex_BecomesShootTargetAfter_Action(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Action> & dstNode, float mult)
		: detail::Hex_BecomesShootTargetAfter_Action_Base(srcNode, dstNode), mult(mult)
	{
		setattr(A::DMG_MULT, permille(mult, 1));
		static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
	}

	const float mult;
};
}
