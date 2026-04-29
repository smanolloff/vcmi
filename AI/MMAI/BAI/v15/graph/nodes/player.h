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
#include "AI/MMAI/common.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include "schema/v15/constants.h"
#include "global.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using PA = S15::Graph::NodeAttributes::Player;
using CombatResult = Schema::V15::CombatResult;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Player : public Element<S15::EncodingTraits<Schema::V15::PlayerEncoding>, S15::Graph::INode>
{
public:
	Player(BattleSide side, int value, int hp)
	: index_(EU(side))
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		setattr(PA::BATTLE_SIDE, EU(side));
		setattr(PA::VALUE_KILLED_ACC_ABS, 0);
		setattr(PA::VALUE_LOST_ACC_ABS, 0);
		setattr(PA::DMG_DEALT_ACC_ABS, 0);
		setattr(PA::DMG_RECEIVED_ACC_ABS, 0);
	}

    int nodeIndex() const override {
        return index_;  // always exactly 2 Player nodes
    }

	void update(const Global * global, int value, int hp, int dmgDealt, int dmgReceived, int valueKilled, int valueLost)
	{
		// setattr(PA::BATTLE_SIDE, EU(side)); // no change
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
		static_assert(EU(PA::_count) == 23, "whistleblower in case attributes change");
	}

private:
    const int index_;
	std::array<int, EU(PA::_count)> attrs = {};

	void addattr(PA a, int value)
	{
		attrs.at(EU(a)) += value;
	}
};
}
