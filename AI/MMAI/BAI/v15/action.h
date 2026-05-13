/*
 * action.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/base.h"
#include "BAI/v15/graph/graph.h"

namespace MMAI::BAI::V15
{
/*
 * Wrapper around Schema::Action
 */
struct Action
{
	Action(
		Schema::Action actionId,
		const CStack * acstack,
		const std::shared_ptr<Graph::Graph> & G,
		const std::string & color
	);

	const Schema::Action id;
	const std::string color;
	const std::string name;
};
}
