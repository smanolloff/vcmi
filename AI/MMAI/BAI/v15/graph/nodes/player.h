/*
 * global.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/nodes/common.h"
#include "battle/BattleSide.h"
#include "AI/MMAI/common.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include "global.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using PA = S15::Graph::NodeAttributes::Player;
using CombatResult = Schema::V15::CombatResult;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Player : public S15::Graph::INode
{
public:
	Player(BattleSide side, int value, int hp)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		static_assert(EU(PA::_count) == 23, "whistleblower in case attributes change");

		setattr(PA::BATTLE_SIDE, EI(side));
		setattr(PA::VALUE_KILLED_ACC_ABS, 0);
		setattr(PA::VALUE_LOST_ACC_ABS, 0);
		setattr(PA::DMG_DEALT_ACC_ABS, 0);
		setattr(PA::DMG_RECEIVED_ACC_ABS, 0);
	}

	S15::Graph::NodeType getType() const override
	{
		return S15::Graph::NodeType::PLAYER;
	}

	std::vector<float> encodedAttributes() const override
	{
		return encodeNodeAttributes(attrs, S15::PLAYER_ENCODING);
	}

	int getAttr(PA a) const
	{
		return attr(a);
	}

	int attr(PA a) const
	{
		return attrs.at(EI(a));
	}

	void update(const Global * global, int value, int hp, int dmgDealt, int dmgReceived, int valueKilled, int valueLost)
	{
		setattr(PA::ARMY_VALUE_NOW_ABS, value);
		setattr(PA::ARMY_VALUE_NOW_REL, 1000LL * value / global->attr(GA::BFIELD_VALUE_NOW_ABS));
		setattr(PA::ARMY_VALUE_NOW_REL0, 1000LL * value / global->attr(GA::BFIELD_VALUE_START_ABS));
		setattr(PA::ARMY_HP_NOW_ABS, hp);
		setattr(PA::ARMY_HP_NOW_REL, 1000LL * hp / global->attr(GA::BFIELD_HP_NOW_ABS));
		setattr(PA::ARMY_HP_NOW_REL0, 1000LL * hp / global->attr(GA::BFIELD_HP_START_ABS));
		setattr(PA::VALUE_KILLED_NOW_ABS, valueKilled);
		setattr(PA::VALUE_KILLED_NOW_REL, 1000LL * valueKilled / global->attr(GA::BFIELD_VALUE_NOW_ABS));
		addattr(PA::VALUE_KILLED_ACC_ABS, valueKilled);
		setattr(PA::VALUE_KILLED_ACC_REL0, 1000LL * attr(PA::VALUE_KILLED_ACC_ABS) / global->attr(GA::BFIELD_VALUE_START_ABS));
		setattr(PA::VALUE_LOST_NOW_ABS, valueLost);
		setattr(PA::VALUE_LOST_NOW_REL, 1000LL * valueLost / global->attr(GA::BFIELD_VALUE_NOW_ABS));
		addattr(PA::VALUE_LOST_ACC_ABS, valueLost);
		setattr(PA::VALUE_LOST_ACC_REL0, 1000LL * attr(PA::VALUE_LOST_ACC_ABS) / global->attr(GA::BFIELD_VALUE_START_ABS));
		setattr(PA::DMG_DEALT_NOW_ABS, dmgDealt);
		setattr(PA::DMG_DEALT_NOW_REL, 1000LL * dmgDealt / global->attr(GA::BFIELD_HP_NOW_ABS));
		addattr(PA::DMG_DEALT_ACC_ABS, dmgDealt);
		setattr(PA::DMG_DEALT_ACC_REL0, 1000LL * attr(PA::DMG_DEALT_ACC_ABS) / global->attr(GA::BFIELD_HP_START_ABS));
		setattr(PA::DMG_RECEIVED_NOW_ABS, dmgReceived);
		setattr(PA::DMG_RECEIVED_NOW_REL, 1000LL * dmgReceived / global->attr(GA::BFIELD_HP_NOW_ABS));
		addattr(PA::DMG_RECEIVED_ACC_ABS, dmgReceived);
		setattr(PA::DMG_RECEIVED_ACC_REL0, 1000LL * attr(PA::DMG_RECEIVED_ACC_ABS) / global->attr(GA::BFIELD_HP_START_ABS));
	}

	void setattr(PA a, int value)
	{
		attrs.at(EI(a)) = value;
	}

	void addattr(PA a, int value)
	{
		attrs.at(EI(a)) += value;
	}

	std::array<int, EU(PA::_count)> attrs = {};
};
}
