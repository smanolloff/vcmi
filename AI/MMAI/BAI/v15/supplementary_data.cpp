/*
 * supplementary_data.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BAI/v15/supplementary_data.h"
#include "common.h"

namespace MMAI::BAI::V15
{
HexesView SupplementaryData::getHexes() const
{
	ASSERT(battlefield, "getHexes() called when battlefield is null");
	auto res = HexesView{};

	for(int y = 0; y < battlefield->hexes->size(); ++y)
	{
		auto & hexrow = battlefield->hexes->at(y);
		auto & resrow = res.at(y);
		for(int x = 0; x < hexrow.size(); ++x)
		{
			resrow.at(x) = hexrow.at(x).get();
		}
	}

	return res;
}

StacksView SupplementaryData::getStacks() const
{
	ASSERT(battlefield, "getStacks() called when battlefield is null");
	auto res = StacksView{};

	for(const auto & stack : battlefield->stacks)
	{
		res.push_back(stack.get());
	}

	return res;
}

AllLinksView SupplementaryData::getAllLinks() const
{
	ASSERT(battlefield, "getAllLinks() called when battlefield is null");
	auto res = AllLinksView{};

	for(const auto & [lt, links] : battlefield->allLinks)
	{
		res[lt] = links.get();
	}

	return res;
}

Schema::V15::AttackLogs SupplementaryData::getAttackLogs() const
{
	auto res = Schema::V15::AttackLogs{};
	res.reserve(attackLogs.size());

	for(const auto & al : attackLogs)
		res.push_back(al.get());

	return res;
}

}
