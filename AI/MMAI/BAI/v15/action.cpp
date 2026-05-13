/*
 * action.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "BAI/v15/action.h"
#include "common.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{

namespace
{
	namespace S15 = ::MMAI::Schema::V15;

	std::string BuildName(
		const CStack * acstack,
		const std::shared_ptr<Graph::Graph> & G,
		const std::string & color, int id)
	{
		if(id == Schema::ACTION_ERROR)
			return "Error";
		else if(id == Schema::V15::ACTION_RETREAT)
			return "Retreat";

		const auto & action = G->getByExtraIndex<Graph::Nodes::Action>(std::pair<int, int>{id, acstack->unitId()});
		const auto & endhex = action->endsAt.at(0);

		auto stackstr = [&color](const auto & edge) {
			std::string targetside = (color == "red") ? "L" : "R";
			return targetside + "-" + std::string(1, edge->dstNode->getAlias());
		};

		switch(action->actionType)
		{
		case S15::ActionType::WAIT:
			return "Wait";
		case S15::ActionType::DEFEND:
			return "Defend";
		case S15::ActionType::MOVE:
			return "Defend on hex(" + endhex->name() + ")";
		case S15::ActionType::AMOVE:
			for (const auto & edge : G->getAllEdgesBySrc<Graph::Edges::Action_Melees_Unit>(action))
				if (edge->isPrimaryTarget)
					return "Attack stack(" + stackstr(edge) + ") from hex(" + endhex->name() + ")";

			throw std::runtime_error("Got AMOVE but there are no valid targets");
		case S15::ActionType::SHOOT:
			for (const auto & edge : G->getAllEdgesBySrc<Graph::Edges::Action_Shoots_Unit>(action))
				if (edge->isPrimaryTarget)
					return "Attack stack(" + stackstr(edge) + ") from hex(" + endhex->name() + ")";
			throw std::runtime_error("Got SHOOT but there are no valid targets");
		default:
	    	throw std::runtime_error("Unexpected action type: " + std::to_string(EU(action->actionType)));
		}
	}
}

Action::Action(
	Schema::Action actionId,
	const CStack * acstack,
	const std::shared_ptr<Graph::Graph> & G,
	const std::string & color)
: id(actionId), name(BuildName(acstack, G, color, actionId)), color(color)
{}

}
