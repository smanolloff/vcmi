/*
 * unit_melee_dmg_unit.h, part of VCMI engine
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
using EA = S15::Graph::EdgeAttributes::Unit_MeleeDmg_Unit;

class Unit_MeleeDmg_Unit : public Element<S15::EncodingTraits<Schema::V15::EdgeEncoding_Unit_MeleeDmg_Unit>, S15::Graph::IEdge>
{
public:
	Unit_MeleeDmg_Unit(int srcIndex, int dstIndex, int attackDmgRel, int retalDmgRel)
		: srcIndex(srcIndex), dstIndex(dstIndex)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		setattr(EA::ATTACK_DMG_REL, attackDmgRel);
		setattr(EA::RETAL_DMG_REL, retalDmgRel);
		static_assert(static_cast<size_t>(EA::_count) == 2, "whistleblower in case attributes change");
	}

	std::pair<S15::Graph::ElementType, S15::Graph::ElementType> nodeTypes() const override
	{
		return {S15::Graph::ElementType::NODE_UNIT, S15::Graph::ElementType::NODE_UNIT};
	}

	std::pair<int, int> nodeIndexes() const override
	{
		return {srcIndex, dstIndex};
	}

	const int srcIndex;
	const int dstIndex;
};
}
