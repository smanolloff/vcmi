/*
 * hex_adjacent_hex.h, part of VCMI engine
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
#include "BAI/v16/graph/nodes/hex_v16.h"
#include "schema/v16/constants.h"

namespace MMAI::BAI::V16::Graph::Edges
{
namespace S16 = Schema::V16;

namespace detail
{
	using Hex_Adjacent_Hex_Traits = S16::EncodingTraits<S16::Graph::EdgeAttributes::Hex_Adjacent_Hex>;
	using Hex_Adjacent_Hex_Base = Base<Nodes::Hex, Nodes::Hex, Hex_Adjacent_Hex_Traits>;
}

class Hex_Adjacent_Hex : public detail::Hex_Adjacent_Hex_Base
{
public:
	static std::shared_ptr<const Hex_Adjacent_Hex>
	Create(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Hex> & dstNode, int direction)
	{
		return std::make_shared<const Hex_Adjacent_Hex>(srcNode, dstNode, direction);
	};

	Hex_Adjacent_Hex(const std::shared_ptr<const Nodes::Hex> & srcNode, const std::shared_ptr<const Nodes::Hex> & dstNode, int direction)
		: detail::Hex_Adjacent_Hex_Base(srcNode, dstNode)
	{
		setattr(A::DIRECTION, direction);
		static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
	}
};
}
