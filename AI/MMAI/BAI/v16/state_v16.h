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

#include "AI/MMAI/common.h" // IWYU pragma: keep

#include "battle/CBattleInfoEssentials.h"
#include "battle/CPlayerBattleCallback.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v16/attack_log_v16.h"
#include "BAI/v16/supplementary_data_v16.h"
#include "schema/base.h"
#include "schema/v16/types.h"
#include <stdexcept>

namespace MMAI::BAI::V16
{
using BS = Schema::BattlefieldState;
namespace S16 = Schema::V16;

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
		// The valid actions to make can be obtained via:
		// IGraph::getNodes(ElementType::NODE_ACTION) - all potential actions for all units
		// +
		// IGraph::getActiveNodeToActionIds() - indexes of the actions by the active unit
		throw std::runtime_error("getActionMask() should not be called in v16");
	};
	const Schema::AttentionMask * getAttentionMask() const override
	{
		throw std::runtime_error("getAttentionMask() should not be called in v16");
	}
	const Schema::BattlefieldState * getBattlefieldState() const override
	{
		// The battlefield is now represented via IGraph.
		throw std::runtime_error("getBattlefieldState() should not be called in v16");
	}
	std::any getSupplementaryData() const override
	{
		return static_cast<const MMAI::Schema::V16::ISupplementaryData *>(supdata.get());
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

	void onActiveStack(const CStack * acstack, int round, S16::CombatResult result = S16::CombatResult::NONE);
	void onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa);
	void onBattleTriggerEffect(const BattleTriggerEffect & bte);
	void onBattleEnd(const BattleResult & br, int round);

	const int version_;
	const CPlayerBattleCallback & battle;

	GlobalStats startStats;
	GlobalStats lastStats;

	std::shared_ptr<Graph::Graph> G;
	std::unique_ptr<SupplementaryData> supdata = nullptr;
	std::vector<AttackLog> attackLogs;
	std::optional<std::map<uint32_t, AttackLog::UnitInfo>> logUnits;
	const std::string colorname;
	const BattleSide side;
	bool isMorale = false;

private:
	void buildGraph(const CStack * acstack, int round, S16::CombatResult result);
};
}
