/*
 * action.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BAI/v15/action.h"
#include "common.h"

namespace MMAI::BAI::V15
{

namespace
{
	const Graph::Nodes::Hex * initHex(const Schema::Action & a, const Graph::Graph & G)
	{
		// Control actions (<0) should never reach here
		ASSERT(a >= 0 && a < N_ACTIONS, "Invalid action: " + std::to_string(a));

		auto i = a - EI(GlobalAction::_count);

		if(i < 0)
			return nullptr;

		i = i / EI(HexAction::_count);

		// convert the const ref to a pointer-to-const
		return &G.get<Graph::Nodes::Hex>(i);
	}

	HexAction initHexAction(const Schema::Action & a)
	{
		if(a < EI(GlobalAction::_count))
			return static_cast<HexAction>(-1); // a is not about a hex
		return static_cast<HexAction>((a - EI(GlobalAction::_count)) % EI(HexAction::_count));
	}

	const Graph::Nodes::Hex * initAMoveTargetHex(const Schema::Action & a, const Graph::Graph & G)
	{
		const auto * hex = initHex(a, G);
		if(!hex)
			return nullptr;

		auto ha = initHexAction(a);
		if(EI(ha) == -1)
			return nullptr;

		if(ha == HexAction::MOVE || ha == HexAction::SHOOT)
			return nullptr;
		// throw std::runtime_error("MOVE and SHOOT are not AMOVE actions");

		const auto & bh = hex->bhex;

		auto edir = AMOVE_TO_EDIR.at(EI(ha));
		auto nbh = bh.cloneInDirection(edir);

		ASSERT(nbh.isAvailable(), "unavailable AMOVE target hex #" + std::to_string(nbh.toInt()));

		int i = Graph::Nodes::Hex::CalcId(nbh);
		// convert the const ref to a pointer-to-const
		return &G.get<Graph::Nodes::Hex>(i);
	}
}

Action::Action(const Schema::Action action_, const Graph::Graph & G, const std::string & color_)
	: action(action_)
	, hex(initHex(action_, G))
	, aMoveTargetHex(initAMoveTargetHex(action_, G))
	, hexaction(initHexAction(action_))
	, color(color_)
{
	name = buildName(G);
}

std::string Action::buildName(const Graph::Graph & G)
{
	if(action == Schema::ACTION_ERROR)
		return "Error";
	else if(action == Schema::V15::ACTION_RETREAT)
		return "Retreat";
	else if(action == Schema::V15::ACTION_WAIT)
		return "Wait";

	ASSERT(hex, "hex is null");

	auto ha = static_cast<HexAction>((action - EI(GlobalAction::_count)) % EI(HexAction::_count));
	auto res = std::string{};
	const Graph::Nodes::Unit * stack = nullptr;
	std::string stackstr;


	if(ha == HexAction::SHOOT || ha == HexAction::MOVE)
	{
		stack = G.findUnitByHex(hex->bhex);
	}
	else if(aMoveTargetHex)
	{
		stack = G.findUnitByHex(aMoveTargetHex->bhex);
	}

	// colored output does not look good (scrambles default VCMI log coloring)
	// Additionally, hardcoded red/blue colors are relevant during training only
	// => replace with attacker/defender colorless strings instead
	// if (stack) {
	//     std::string targetcolor = "\033[31m";  // red
	//     if (color == "red") targetcolor = "\033[34m"; // blue
	//     stackstr = targetcolor + "#" + std::string(1, stack->getAlias()) + "\033[0m";
	// } else {
	//     std::string targetcolor = "\033[7m";  // white
	//     stackstr = targetcolor + "#?" + "\033[0m";
	// }

	if(stack)
	{
		std::string targetside = (color == "red") ? "L" : "R";
		stackstr = targetside + "-" + std::string(1, stack->getAlias());
	}
	else
	{
		stackstr = "?";
	}

	switch(ha)
	{
		case HexAction::MOVE:
			res = (stack && hex->bhex == stack->cstack.getPosition() ? "Defend on hex(" : "Move to (") + hex->name() + ")";
			break;
		case HexAction::AMOVE_TL:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /top-left/";
			break;
		case HexAction::AMOVE_TR:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /top-right/";
			break;
		case HexAction::AMOVE_R:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /right/";
			break;
		case HexAction::AMOVE_BR:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /bottom-right/";
			break;
		case HexAction::AMOVE_BL:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /bottom-left/";
			break;
		case HexAction::AMOVE_L:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /left/";
			break;
		case HexAction::AMOVE_2BL:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /bottom-left-2/";
			break;
		case HexAction::AMOVE_2L:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /left-2/";
			break;
		case HexAction::AMOVE_2TL:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /top-left-2/";
			break;
		case HexAction::AMOVE_2TR:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /top-right-2/";
			break;
		case HexAction::AMOVE_2R:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /right-2/";
			break;
		case HexAction::AMOVE_2BR:
			res = "Attack stack(" + stackstr + ") from hex(" + hex->name() + ") /bottom-right-2/";
			break;
		case HexAction::SHOOT:
			res = "Attack stack(" + stackstr + ") " + hex->name() + " (ranged)";
			break;
		default:
			THROW_FORMAT("Unexpected hexaction: %d", EI(ha));
	}

	return res;
}

}
