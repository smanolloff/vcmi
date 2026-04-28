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

#include "battle/BattleSide.h"
#include "BAI/v15/graph/nodes/common.h"
#include "AI/MMAI/common.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using GA = S15::Graph::NodeAttributes::Global;
using CombatResult = Schema::V15::CombatResult;
using GlobalAction = Schema::V15::GlobalAction;
using GlobalActionMask = std::bitset<EI(GlobalAction::_count)>;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Global : public S15::Graph::INode
{
public:
	Global(BattleSide side, int value, int hp, TowerFlags towers, CorpseFlags corpses)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		static_assert(EU(GA::_count) == 12, "whistleblower in case attributes change");

		setattr(GA::BATTLE_WINNER, S15::NULL_VALUE_UNENCODED);
		setattr(GA::BATTLE_ROUND, 0);
		setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, S15::NULL_VALUE_UNENCODED);
		setattr(GA::BFIELD_VALUE_START_ABS, value);
		setattr(GA::BFIELD_VALUE_NOW_ABS, value);
		setattr(GA::BFIELD_VALUE_NOW_REL0, 1000);
		setattr(GA::BFIELD_HP_START_ABS, hp);
		setattr(GA::BFIELD_HP_NOW_ABS, hp);
		setattr(GA::BFIELD_HP_NOW_REL0, 1000);
		setattr(GA::SIEGE_TOWERS, towers.to_ulong());
		setattr(GA::SIEGE_CORPSES, corpses.to_ulong());
		setattr(GA::ACTION_MASK, 0);
	}

	S15::Graph::NodeType getType() const override
	{
		return S15::Graph::NodeType::GLOBAL;
	}

	std::vector<float> encodedAttributes() const override
	{
		return encodeNodeAttributes(attrs, S15::GLOBAL_ENCODING);
	}

	int getAttr(GA a) const
	{
		return attr(a);
	}

	int attr(GA a) const
	{
		return attrs.at(EI(a));
	}

	void update(BattleSide side, CombatResult res, int value, int hp, bool canWait, TowerFlags towers, CorpseFlags corpses, int round)
	{
		setattr(GA::BATTLE_ROUND, round);
		(res == CombatResult::NONE) ? setattr(GA::BATTLE_WINNER, S15::NULL_VALUE_UNENCODED) : setattr(GA::BATTLE_WINNER, EI(res));
		(side == BattleSide::NONE) ? setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, S15::NULL_VALUE_UNENCODED) : setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, EI(side));
		setattr(GA::BFIELD_VALUE_NOW_ABS, value);
		setattr(GA::BFIELD_VALUE_NOW_REL0, 1000LL * value / attr(GA::BFIELD_VALUE_START_ABS));
		setattr(GA::BFIELD_HP_NOW_ABS, hp);
		setattr(GA::BFIELD_HP_NOW_REL0, 1000LL * hp / attr(GA::BFIELD_HP_START_ABS));
		setattr(GA::SIEGE_TOWERS, towers.to_ulong());
		setattr(GA::SIEGE_CORPSES, corpses.to_ulong());

		canWait ? actmask.set(EI(GlobalAction::WAIT)) : actmask.reset(EI(GlobalAction::WAIT));
		setattr(GA::ACTION_MASK, actmask.to_ulong());
	}

	void setattr(GA a, int value)
	{
		attrs.at(EI(a)) = value;
	}

	std::array<int, EU(GA::_count)> attrs = {};
	GlobalActionMask actmask = 0;
};
}
