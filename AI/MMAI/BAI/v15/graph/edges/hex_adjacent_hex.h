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

#include "BAI/v15/graph/element.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;
using EA = S15::Graph::EdgeAttributes::Hex_Adjacent_Hex;

class Hex_Adjacent_Hex : public Element<S15::EncodingTraits<Schema::V15::EdgeEncoding_Hex_Adjacent_Hex>, S15::Graph::IEdge>
{
public:
	Hex_Adjacent_Hex(int srcIndex, int dstIndex, int direction)
		: srcIndex(srcIndex), dstIndex(dstIndex)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		setattr(EA::DIRECTION, direction);
		static_assert(static_cast<size_t>(EA::_count) == 1, "whistleblower in case attributes change");
	}

	std::pair<S15::Graph::ElementType, S15::Graph::ElementType> nodeTypes() const override
	{
		return {S15::Graph::ElementType::NODE_HEX, S15::Graph::ElementType::NODE_HEX};
	}

	std::pair<int, int> nodeIndexes() const override
	{
		return {srcIndex, dstIndex};
	}

	const int srcIndex;
	const int dstIndex;
};
}
