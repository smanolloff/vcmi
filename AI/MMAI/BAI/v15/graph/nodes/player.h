/*
 * player.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "battle/BattleSide.h"
#include "BAI/v15/graph/element.h"
#include "common.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

class Player : public Element<S15::Graph::INode, S15::EncodingTraits<S15::PlayerEncoding>>
{
	using A = S15::Graph::NodeAttributes::Player;
public:
	struct Stats
	{
		int v = 0; // army value
		int hp = 0; // army hp
		int dd = 0; // damage dealt
		int dr = 0; // damage received
		int vk = 0; // value killed
		int vl = 0; // value lost
	};

	Player(
		BattleSide side,
		int globalValueStart,
		int globalHpStart,
		int globalValuePrevRound,
		int globalHpPrevRound,
		int value,
		int hp,
		int dmgDealt,
		int dmgReceived,
		int valueKilled,
		int valueLost
	);

private:
	std::array<int, EU(A::_count)> attrs = {};
	void addattr(A a, int value);
};

}
