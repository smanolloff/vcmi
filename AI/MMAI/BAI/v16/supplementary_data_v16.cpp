/*
 * supplementary_data.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "BAI/v16/supplementary_data_v16.h"

#include "schema/v16/types.h"

namespace MMAI::BAI::V16
{

Schema::V16::AttackLogs SupplementaryData::getAttackLogs() const
{
	auto res = Schema::V16::AttackLogs{};
	res.reserve(attackLogs.size());

	for(const auto & al : attackLogs)
		res.push_back(&al);

	return res;
}

}
