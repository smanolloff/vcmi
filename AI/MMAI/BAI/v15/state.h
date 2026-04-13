/*
 * state.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "battle/CBattleInfoEssentials.h"
#include "battle/CPlayerBattleCallback.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/action.h"
#include "BAI/v15/attack_log.h"
#include "BAI/v15/battlefield.h"
#include "BAI/v15/global_stats.h"
#include "BAI/v15/player_stats.h"
#include "BAI/v15/supplementary_data.h"
#include "schema/base.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
using BS = Schema::BattlefieldState;

static const auto DUMMY_ATTNMASK = Schema::AttentionMask();

class State : public Schema::IState
{
public:
	// IState impl
	const Schema::ActionMask * getActionMask() const override
	{
		return &actmask;
	};
	const Schema::AttentionMask * getAttentionMask() const override
	{
		return &DUMMY_ATTNMASK;
	}
	const Schema::BattlefieldState * getBattlefieldState() const override
	{
		return &bfstate;
	}
	std::any getSupplementaryData() const override
	{
		return static_cast<const MMAI::Schema::V15::ISupplementaryData *>(supdata.get());
	}
	int version() const override
	{
		return version_;
	}

	State() = delete;
	State(
		int version_,
		const std::string & colorname,
		const CPlayerBattleCallback * battle
	);

	void onActiveStack(
		const CStack * astack,
		int round,
		CombatResult result = CombatResult::NONE
	);
	void onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa);
	void onBattleTriggerEffect(const BattleTriggerEffect & bte);
	void onBattleEnd(const BattleResult * br, int round);

	// Subsequent versions may override this if they only change
	// the data type of encoded values (i.e. have their own HEX_ENCODING)
	void encodeGlobal(CombatResult result);
	void encodePlayer(const PlayerStats * pstats);
	void encodeHex(const Hex * hex);
	void verify() const;

	const int version_;
	const CPlayerBattleCallback * const battle;

	Schema::BattlefieldState bfstate;
	Schema::ActionMask actmask;
	std::unique_ptr<SupplementaryData> supdata = nullptr;
	std::vector<std::shared_ptr<AttackLog>> attackLogs;
	std::unique_ptr<Action> action = nullptr;
	std::unique_ptr<GlobalStats> gstats = nullptr;
	std::unique_ptr<PlayerStats> lpstats = nullptr;
	std::unique_ptr<PlayerStats> rpstats = nullptr;
	std::map<const CStack *, Stack::Stats> sstats;
	const std::pair<int, int> initialArmyValues;
	const std::string colorname;
	const BattleSide side;
	std::shared_ptr<const Battlefield> battlefield;
	bool isMorale = false;

	static std::vector<float> InitNullStack();
	const std::vector<float> nullstack;
};
}
