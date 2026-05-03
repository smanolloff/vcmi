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

#include "BAI/v15/graph/graph.h"
#include "BAI/v15/hexaction.h"

namespace MMAI::BAI::V15
{
/*
 * Wrapper around Schema::Action
 */
struct Action
{
	// Action(Schema::Action action_, const Graph::Graph & G, Cache & cache, const std::string & color_);
	Action(Schema::Action action_, const Graph::Graph & G, const std::string & color_);

	const Schema::Action action;
	const Graph::Nodes::Hex * hex;
	const Graph::Nodes::Hex * aMoveTargetHex;
	const HexAction hexaction;
	const std::string color;

	std::string name;

private:
	std::string buildName(const Graph::Graph & G);
};
}
