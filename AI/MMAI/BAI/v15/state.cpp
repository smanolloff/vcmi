/*
 * state.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"

#include "BAI/v15/graph/graph.h"
#include "battle/CPlayerBattleCallback.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/state.h"
#include "BAI/v15/supplementary_data.h"
#include "common.h"

namespace MMAI::BAI::V15
{

namespace
{
	using Global = Graph::Nodes::Global;
	using Player = Graph::Nodes::Player;
	using TowerFlags = Global::TowerFlags;
	using CorpseFlags = Global::CorpseFlags;


	State::GlobalStats CalcGlobalStats(const CPlayerBattleCallback & battle)
	{
		int lv = 0;
		int lh = 0;
		int rv = 0;
		int rh = 0;

		for(auto & stack : battle.battleGetStacks())
		{
			auto v = stack->getCount() * Graph::Nodes::Unit::GetValue(stack->unitType());
			auto h = stack->getAvailableHealth();

			if(stack->unitSide() == BattleSide::ATTACKER)
			{
				lv += v;
				lh += static_cast<int>(h);
			}
			else
			{
				rv += v;
				rh += static_cast<int>(h);
			}
		}

		return State::GlobalStats{
			.leftValue = lv,
			.leftHp = lh,
			.rightValue = rv,
			.rightHp = rh
		};
	}

	struct AttackLogAggregateData
	{
		int ldd = 0; // left damage dealt
		int ldr = 0; // left damage received
		int lvk = 0; // left value killed
		int lvl = 0; // left value lost
		int rdd = 0; // right damage dealt
		int rdr = 0; // right damage received
		int rvk = 0; // right value killed
		int rvl = 0; // right value lost
	};

	AttackLogAggregateData ProcessAttackLogs(const std::vector<std::shared_ptr<AttackLog>> & attackLogs, std::map<const CStack *, Graph::Nodes::Unit::Stats> sstats)
	{
		auto res = AttackLogAggregateData{};
		for(auto & [cstack, ss] : sstats)
		{
			ss.dmgDealtNow = 0;
			ss.dmgReceivedNow = 0;
			ss.valueKilledNow = 0;
			ss.valueLostNow = 0;
		}

		for(const auto & al : attackLogs)
		{
			const auto & ald = al->data;
			if(ald.cattacker)
			{
				sstats[ald.cattacker].dmgDealtNow += ald.dmg;
				sstats[ald.cattacker].dmgDealtTotal += ald.dmg;
				sstats[ald.cattacker].valueKilledNow += ald.value;
				sstats[ald.cattacker].valueKilledTotal += ald.value;

				if(ald.cattacker->unitSide() == BattleSide::LEFT_SIDE)
				{
					res.ldd += ald.dmg;
					res.lvk += ald.value;
				}
				else
				{
					res.rdd += ald.dmg;
					res.rvk += ald.value;
				}
			}

			ASSERT(ald.cdefender, "AttackLog cdefender is nullptr!");
			sstats[ald.cdefender].dmgReceivedNow += ald.dmg;
			sstats[ald.cdefender].dmgReceivedTotal += ald.dmg;
			sstats[ald.cdefender].valueLostNow += ald.value;
			sstats[ald.cdefender].valueLostTotal += ald.value;

			if(ald.cdefender->unitSide() == BattleSide::LEFT_SIDE)
			{
				res.ldr += ald.dmg;
				res.lvl += ald.value;
			}
			else
			{
				res.rdr += ald.dmg;
				res.rvl += ald.value;
			}
		}

		return res;
	}

}

State::State(
	int version_,
	const std::string & colorname,
	const CPlayerBattleCallback & battle
)
	: version_(version_)
	, battle(battle)
	, colorname(colorname)
	, side(battle.battleGetMySide())
	, startStats(CalcGlobalStats(battle))
	, lastStats(startStats)
{
}

void State::onActiveStack(
	const CStack * astack,
	int round,
	S15::CombatResult result
)
{
	logAi->debug("onActiveStack: round=%d, result=%d", round, EI(result));
	auto G = std::make_shared<Graph::Graph>(battle, nullptr);

	const auto stats = CalcGlobalStats(battle);
	const auto & [ldd, ldr, lvk, lvl, rdd, rdr, rvk, rvl] = ProcessAttackLogs(attackLogs, sstats);
	const auto lstats = Graph::Nodes::Player::Stats{
		.v = stats.leftValue,
		.hp = stats.leftHp,
		.dd = ldd,
		.dr = ldr,
		.vk = lvk,
		.vl = lvl,
	};

	const auto rstats = Graph::Nodes::Player::Stats{
		.v = stats.rightValue,
		.hp = stats.rightHp,
		.dd = rdd,
		.dr = rdr,
		.vk = rvk,
		.vl = rvl,
	};

	G->addGlobalNode(
		result,
		round,
		startStats.leftValue + startStats.rightValue,
		startStats.leftHp + startStats.rightHp,
		stats.leftValue + stats.rightValue,
		stats.leftHp + stats.rightHp
	);

	static_assert(EU(BattleSide::LEFT_SIDE) == 0, "Nodes::Player index");

	G->addPlayerNode(
		BattleSide::LEFT_SIDE,
		startStats.leftValue + startStats.rightValue,
		startStats.leftHp + startStats.rightHp,
		lastStats.leftValue + lastStats.rightValue,
		lastStats.leftHp + lastStats.rightHp,
		lstats.v,
		lstats.hp,
		lstats.dd,
		lstats.dr,
		lstats.vk,
		lstats.vl
	);

	G->addPlayerNode(
		BattleSide::RIGHT_SIDE,
		startStats.leftValue + startStats.rightValue,
		startStats.leftHp + startStats.rightHp,
		lastStats.leftValue + lastStats.rightValue,
		lastStats.leftHp + lastStats.rightHp,
		rstats.v,
		rstats.hp,
		rstats.dd,
		rstats.dr,
		rstats.vk,
		rstats.vl
	);

	// // Add Unit and Hex nodes
	// Battlefield::Init(cache, battle, astack, *oldG, *G, sstats, false);

	supdata = std::make_unique<SupplementaryData>(
		colorname,
		static_cast<Side>(side),
		G,
		attackLogs, // store the logs since OUR last turn
		result
	);

	attackLogs.clear(); // accumulate new logs until next turn
	isMorale = false;
	lastStats = stats;
}

void State::onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa)
{
	auto cstacks = battle.battleGetStacks();

	for(const auto & elem : bsa)
	{
		const auto * cdefender = battle.battleGetStackByID(elem.stackAttacked, false);
		const auto * cattacker = battle.battleGetStackByID(elem.attackerID, false);

		if(!cdefender)
		{
			logAi->error("MMAI: received BattleStackAttacked with invalid stackAttacked: " + std::to_string(elem.stackAttacked));
			continue;
		}

		auto bf_valueNow = lastStats.leftValue + lastStats.rightValue;
		auto bf_hpNow = lastStats.leftHp + lastStats.rightHp;
		auto value = elem.killedAmount * Graph::Nodes::Unit::GetValue(cdefender->unitType());

		auto ald = AttackLogData{
			.cattacker = cattacker,
			.cdefender = cdefender,
			.dmg = static_cast<int>(elem.damageAmount),
			.dmgPermille = static_cast<int>(1000 * elem.damageAmount / bf_hpNow),
			.units = static_cast<int>(elem.killedAmount),
			.value = static_cast<int>(value),
			.valuePermille = static_cast<int>(1000 * value / bf_valueNow)
		};

		attackLogs.push_back(std::make_shared<AttackLog>(std::move(ald)));
	}
}

void State::onBattleTriggerEffect(const BattleTriggerEffect & bte)
{
	if(bte.effect != BonusType::MORALE)
		return;

	isMorale = true;
}

void State::onBattleEnd(const BattleResult * br, int round)
{
	switch(br->winner)
	{
		case BattleSide::LEFT_SIDE:
			onActiveStack(nullptr, round, S15::CombatResult::LEFT_WINS);
			break;
		case BattleSide::RIGHT_SIDE:
			onActiveStack(nullptr, round, S15::CombatResult::RIGHT_WINS);
			break;
		default:
			onActiveStack(nullptr, round, S15::CombatResult::DRAW);
	}
}
};
