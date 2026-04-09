/*
 * global_stats.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "BAI/v14/global_stats.h"
#include "schema/v14/constants.h"
#include "schema/v14/types.h"

namespace MMAI::BAI::V14
{
namespace S14 = Schema::V14;
using Side = Schema::Side;
using GA = Schema::V14::GlobalAttribute;

static_assert(EI(Side::LEFT) == EI(BattleSide::LEFT_SIDE));
static_assert(EI(Side::RIGHT) == EI(BattleSide::RIGHT_SIDE));

GlobalStats::GlobalStats(BattleSide side, int value, int hp, TowerFlags towers, CorpseFlags corpses)
{
	// Fill with NA to guard against "forgotten" attrs
	// (all attrs are strict so encoder will throw if NAs are found)
	attrs.fill(S14::NULL_VALUE_UNENCODED);

	static_assert(EI(GA::_count) == 12, "whistleblower in case attributes change");

	setattr(GA::BATTLE_WINNER, S14::NULL_VALUE_UNENCODED);
	setattr(GA::BATTLE_ROUND, 0);
	setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, S14::NULL_VALUE_UNENCODED);
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

static_assert(EI(GlobalAction::_count) == 2); // RETREAT, WAIT

void GlobalStats::update(BattleSide side, CombatResult res, int value, int hp, bool canWait, TowerFlags towers, CorpseFlags corpses, int round)
{
	setattr(GA::BATTLE_ROUND, round);
	(res == CombatResult::NONE) ? setattr(GA::BATTLE_WINNER, S14::NULL_VALUE_UNENCODED) : setattr(GA::BATTLE_WINNER, EI(res));

	(side == BattleSide::NONE) ? setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, S14::NULL_VALUE_UNENCODED) : setattr(GA::BATTLE_SIDE_ACTIVE_PLAYER, EI(side));

	// ll (long long) ensures long is 64-bit even on 32-bit systems
	setattr(GA::BFIELD_VALUE_NOW_ABS, value);
	setattr(GA::BFIELD_VALUE_NOW_REL0, 1000LL * value / attr(GA::BFIELD_VALUE_START_ABS));
	setattr(GA::BFIELD_HP_NOW_ABS, hp);
	setattr(GA::BFIELD_HP_NOW_REL0, 1000LL * hp / attr(GA::BFIELD_HP_START_ABS));

	setattr(GA::SIEGE_TOWERS, towers.to_ulong());
	setattr(GA::SIEGE_CORPSES, corpses.to_ulong());

	canWait ? actmask.set(EI(GlobalAction::WAIT)) : actmask.reset(EI(GlobalAction::WAIT));

	setattr(GA::ACTION_MASK, actmask.to_ulong());
}

int GlobalStats::getAttr(GA a) const
{
	return attr(a);
}

int GlobalStats::attr(GA a) const
{
	return attrs.at(EI(a));
};

void GlobalStats::setattr(GA a, int value)
{
	attrs.at(EI(a)) = value;
};
}
