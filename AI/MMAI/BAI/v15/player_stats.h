/*
 * player_stats.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BAI/v15/global_stats.h"
#include "battle/BattleSide.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{

using IPlayerStats = Schema::V15::IPlayerStats;
using PlayerAttribute = Schema::V15::PlayerAttribute;
using PlayerAttrs = Schema::V15::PlayerAttrs;

class PlayerStats : public IPlayerStats
{
public:
	PlayerStats(BattleSide side, int value, int hp);

	int getAttr(PlayerAttribute a) const override;
	int attr(PlayerAttribute a) const;
	void setattr(PlayerAttribute a, int value);
	void addattr(PlayerAttribute a, int value);
	void update(const GlobalStats * gstats, int value, int hp, int dmgDealt, int dmgReceived, int valueKilled, int valueLost);
	PlayerAttrs attrs = {};
};
}
