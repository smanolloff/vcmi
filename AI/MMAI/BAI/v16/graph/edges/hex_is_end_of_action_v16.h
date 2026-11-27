/*
 * hex_is_end_of_action.h, part of VCMI engine
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
#include "schema/v16/constants.h"

namespace MMAI::BAI::V16::Graph::Edges
{
namespace S16 = Schema::V16;

namespace detail
{
	using Hex_IsEndOf_Action_Traits = S16::EncodingTraits<S16::Graph::EdgeAttributes::Hex_IsEndOf_Action>;
	using Hex_IsEndOf_Action_Base = Base<Nodes::Hex, Nodes::Action, Hex_IsEndOf_Action_Traits>;
}

class Hex_IsEndOf_Action : public detail::Hex_IsEndOf_Action_Base
{
public:
	static std::shared_ptr<const Hex_IsEndOf_Action>
	Create(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Action> & dstNode, int isRear)
	{
		return std::make_shared<const Hex_IsEndOf_Action>(srcNode, dstNode, isRear);
	};

	Hex_IsEndOf_Action(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Action> & dstNode, int isRear)
		: detail::Hex_IsEndOf_Action_Base(srcNode, dstNode), isRear(isRear)
	{
		setattr(A::IS_REAR, isRear);
		static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
	}

	const bool isRear;
};
}
