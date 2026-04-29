/*
 * supplementary_data.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "BAI/v15/attack_log.h"
#include "BAI/v15/battlefield.h"
#include "BAI/v15/global_stats.h"
#include "BAI/v15/player_stats.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
using Side = Schema::Side;
using ErrorCode = Schema::V15::ErrorCode;
using StacksView = std::vector<const Stack *>;
using HexesView = std::array<std::array<const Hex *, 15>, 11>;
using AllLinksView = std::map<LinkType, const Links *>;

// match sides for convenience when determining winner (see `victory`)
static_assert(EI(CombatResult::LEFT_WINS) == EI(Side::LEFT));
static_assert(EI(CombatResult::RIGHT_WINS) == EI(Side::RIGHT));

class SupplementaryData : public Schema::V15::ISupplementaryData
{
public:
	SupplementaryData() = delete;

	// Called on activeStack (complete battlefield info)
	SupplementaryData(
		const std::string & colorname_,
		Side side_,
		const GlobalStats * gstats_,
		const PlayerStats * lpstats_,
		const PlayerStats * rpstats_,
		const Battlefield * battlefield_,
		const std::vector<std::shared_ptr<AttackLog>> & attackLogs_,
		CombatResult result
	)
		: colorname(colorname_)
		, side(side_)
		, battlefield(battlefield_)
		, gstats(gstats_)
		, lpstats(lpstats_)
		, rpstats(rpstats_)
		, attackLogs(attackLogs_)
		, ended(result != CombatResult::NONE)
		, victory(EI(result) == EI(side)) {};

	// impl ISupplementaryData
	Type getType() const override
	{
		return type;
	};
	ErrorCode getErrorCode() const override
	{
		return errcode;
	};
	const Schema::V15::Graph::IGraph * getGraph() const override
	{
		return nullptr;
	}
	Schema::V15::AttackLogs getAttackLogs() const override;

	Side getSide() const
	{
		return side;
	}
	std::string getColor() const
	{
		return colorname;
	}
	bool getIsBattleEnded() const
	{
		return ended;
	}
	bool getIsVictorious() const
	{
		return victory;
	}

	StacksView getStacks() const;
	HexesView getHexes() const;
	AllLinksView getAllLinks() const;
	const GlobalStats * getGlobalStats() const
	{
		return gstats;
	}
	const PlayerStats * getLeftPlayerStats() const
	{
		return lpstats;
	}
	const PlayerStats * getRightPlayerStats() const
	{
		return rpstats;
	}
	std::string getAnsiRender() const override
	{
		return ansiRender;
	}

	const std::string colorname;
	const Side side;
	const Battlefield * const battlefield;
	const GlobalStats * const gstats;
	const PlayerStats * const lpstats;
	const PlayerStats * const rpstats;
	const std::vector<std::shared_ptr<AttackLog>> attackLogs;
	const bool ended = false;
	const bool victory = false;

	// Optionally modified (during activeStack if action was invalid)
	ErrorCode errcode = ErrorCode::OK;

	// Optionally modified (during activeStack if action was RENDER)
	Type type = Type::REGULAR;
	std::string ansiRender;
};
}
