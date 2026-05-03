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
#include "BAI/v15/supplementary_data.h"
#include "schema/base.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include <stdexcept>

namespace MMAI::BAI::V15
{
using BS = Schema::BattlefieldState;
namespace S15 = Schema::V15;

static const auto DUMMY_ATTNMASK = Schema::AttentionMask();

class State : public Schema::IState
{
public:
	struct GlobalStats
	{
		int leftValue;
		int leftHp;
		int rightValue;
		int rightHp;
		// used in many places => precalculate
		int totalValue;
		int totalHp;
	};

	const Schema::ActionMask * getActionMask() const override
	{
		throw std::runtime_error("getActionMask() not yet implemented in v15");
	};
	const Schema::AttentionMask * getAttentionMask() const override
	{
		throw std::runtime_error("getAttentionMask() should not be called in v15");
	}
	const Schema::BattlefieldState * getBattlefieldState() const override
	{
		throw std::runtime_error("getBattlefieldState() should not be called in v15");
	}
	std::any getSupplementaryData() const override
	{
		return static_cast<const SupplementaryData *>(supdata.get());
	}
	int version() const override
	{
		return version_;
	}

	State() = delete;
	State(int version_, const std::string & colorname, const CPlayerBattleCallback & battle);

    State(const State &) = delete;
    State & operator=(const State &) = delete;
    State(State &&) = delete;
    State & operator=(State &&) = delete;

	void onActiveStack(
		const CStack * acstack,
		int round,
		S15::CombatResult result = S15::CombatResult::NONE
	);
	void onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa);
	void onBattleTriggerEffect(const BattleTriggerEffect & bte);
	void onBattleEnd(const BattleResult & br, int round);

	const int version_;
	const CPlayerBattleCallback & battle;

	GlobalStats startStats;
	GlobalStats lastStats;
	std::unique_ptr<SupplementaryData> supdata = nullptr;
	std::vector<AttackLog> attackLogs;
	std::unique_ptr<Action> action = nullptr;
	std::unordered_map<const CStack *, Graph::Nodes::Unit::Stats> sstats;
	const std::string colorname;
	const BattleSide side;
	bool isMorale = false;
};
}
