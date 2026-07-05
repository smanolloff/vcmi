/*
 * global_stats.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "battle/BattleSide.h"
#include "schema/v14/types.h"

namespace MMAI::BAI::V14
{

using CombatResult = Schema::V14::CombatResult;
using GlobalAction = Schema::V14::GlobalAction;
using GlobalAttribute = Schema::V14::GlobalAttribute;
using GlobalAttrs = Schema::V14::GlobalAttrs;
using IGlobalStats = Schema::V14::IGlobalStats;

using GlobalActionMask = std::bitset<EI(GlobalAction::_count)>;

using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class GlobalStats : public IGlobalStats
{
public:
	GlobalStats(BattleSide side, int value, int hp, TowerFlags towers, CorpseFlags corpses);

	int getAttr(GlobalAttribute a) const override;
	int attr(GlobalAttribute a) const;
	void update(BattleSide side, CombatResult res, int value, int hp, bool canWait, TowerFlags towers, CorpseFlags corpses, int round);
	void setattr(GlobalAttribute a, int value);
	GlobalAttrs attrs = {};
	GlobalActionMask actmask = 0; // for active stack only
};
}
