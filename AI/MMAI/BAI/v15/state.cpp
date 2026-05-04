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

#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "battle/CPlayerBattleCallback.h"
#include "entities/building/TownFortifications.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/state.h"
#include "BAI/v15/supplementary_data.h"
#include "common.h"
#include <cmath>
#include <numbers>

namespace MMAI::BAI::V15
{

namespace
{
	using Global = Graph::Nodes::Global;
	using Player = Graph::Nodes::Player;
	using TowerFlags = Global::TowerFlags;
	using CorpseFlags = Global::CorpseFlags;
	using ET = S15::Graph::ElementType;
	using GraphBits = std::bitset<EU(ET::_count)>;

    TowerFlags GetSiegeTowers(const CPlayerBattleCallback & battle) {
        TowerFlags res = 0; // {upper, middle, lower}

        auto has = [&battle](EWallPart part) {
            auto ws = battle.battleGetWallState(part);
            return ws != EWallState::NONE && ws != EWallState::DESTROYED;
        };

        if (has(EWallPart::UPPER_TOWER))
            res.set(0);
        if (has(EWallPart::KEEP))
            res.set(1);
        if (has(EWallPart::BOTTOM_TOWER))
            res.set(2);

        return res;
    }

    CorpseFlags GetSiegeCorpses(const CPlayerBattleCallback & battle)
    {
        CorpseFlags res = 0; // {gate, bridge}

        if(battle.battleGetFortifications().wallsHealth == 0)
            return res;

        for(const auto & cstack : battle.battleGetAllStacks(false))
        {
            if(cstack->alive())
                continue;

            if(cstack->coversPos(BattleHex::GATE_INNER) || cstack->coversPos(BattleHex::GATE_OUTER))
                res.set(0);
            if (cstack->coversPos(BattleHex::GATE_BRIDGE))
                res.set(1);
        };

        return res;
    }

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
			.rightHp = rh,
			.totalValue = lv + rv,
			.totalHp = lh + rh
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

	AttackLogAggregateData ProcessAttackLogs(
		const std::vector<AttackLog> & attackLogs,
		std::unordered_map<const CStack *, Graph::Nodes::Unit::Stats> sstats
	)
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
			if(al.attacker)
			{
				sstats.at(al.attacker).dmgDealtNow += al.dmg;
				sstats.at(al.attacker).dmgDealtTotal += al.dmg;
				sstats.at(al.attacker).valueKilledNow += al.value;
				sstats.at(al.attacker).valueKilledTotal += al.value;

				if(al.attacker->unitSide() == BattleSide::LEFT_SIDE)
				{
					res.ldd += al.dmg;
					res.lvk += al.value;
				}
				else
				{
					res.rdd += al.dmg;
					res.rvk += al.value;
				}
			}

			sstats[&al.defender].dmgReceivedNow += al.dmg;
			sstats[&al.defender].dmgReceivedTotal += al.dmg;
			sstats[&al.defender].valueLostNow += al.value;
			sstats[&al.defender].valueLostTotal += al.value;

			if(al.defender.unitSide() == BattleSide::LEFT_SIDE)
			{
				res.ldr += al.dmg;
				res.lvl += al.value;
			}
			else
			{
				res.rdr += al.dmg;
				res.rvl += al.value;
			}
		}

		return res;
	}

	int WallHP(const CPlayerBattleCallback & battle, const BattleHex & bhex) {
		auto part = battle.battleHexToWallPart(bhex);
		switch(part)
		{
			case EWallPart::BOTTOM_WALL:
			case EWallPart::BELOW_GATE:
			case EWallPart::OVER_GATE:
			case EWallPart::UPPER_WALL:
			case EWallPart::GATE:
				switch(battle.battleGetWallState(part))
				{
					case EWallState::NONE:
						return Schema::V15::NULL_VALUE_UNENCODED;
					case EWallState::DESTROYED:
						return 0;
					case EWallState::DAMAGED:
						return 1;
					case EWallState::INTACT:
						return 2;
					case EWallState::REINFORCED:
						return 3;
					default:
						logAi->warn("MMAI: unexpected wall state: %d", EI(battle.battleGetWallState(part)));
						return Schema::V15::NULL_VALUE_UNENCODED;
				}
			default:
				return Schema::V15::NULL_VALUE_UNENCODED;
			break;
		}
	}

	// A custom hash function must be provided for the adjmap
	struct PairHash
	{
		std::size_t operator()(const std::pair<si16, si16> & t) const
		{
			auto h0 = std::hash<int>{}(std::get<0>(t));
			auto h1 = std::hash<int>{}(std::get<1>(t));
			return h0 ^ (h1 << 1);
		}
	};

	std::unordered_map<std::pair<si16, si16>, int, PairHash> InitAdjMap()
	{
		auto res = std::unordered_map<std::pair<si16, si16>, int, PairHash>{};

		for(int id1 = 0; id1 < GameConstants::BFIELD_SIZE; id1++)
		{
			auto hex1 = BattleHex(id1);
			for(int i=0; i<6; ++i)
			{
				auto hex2 = hex1.cloneInDirection(AMOVE_TO_EDIR[i], false);
				res[{hex1.toInt(), hex2.toInt()}] = i;
			}
		}

		return res;
	}

	void AddGlobalNode(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const S15::CombatResult result,
		const int round,
		const State::GlobalStats & startStats,
		const State::GlobalStats & stats
	)
	{
		ASSERT(!added.test(EU(ET::NODE_GLOBAL)), "Elements of type NODE_GLOBAL already added");
		added.set(EU(ET::NODE_GLOBAL));

		G->add(Graph::Nodes::Global(
			acstack ? acstack->unitSide() : battle.battleGetMySide(),
	        result,
	        round,
	        startStats.totalValue,
	        startStats.totalHp,
	        stats.totalValue,
	        stats.totalHp,
	        GetSiegeTowers(battle),
	        GetSiegeCorpses(battle)
		));
	}

	void AddPlayerNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const State::GlobalStats & startStats,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const AttackLogAggregateData & logdata
	)
	{
		ASSERT(!added.test(EU(ET::NODE_PLAYER)), "Elements of type NODE_PLAYER already added");
		added.set(EU(ET::NODE_PLAYER));

        static_assert(EU(BattleSide::LEFT_SIDE) == 0);
        static_assert(EU(BattleSide::RIGHT_SIDE) == 1);

		G->add(Graph::Nodes::Player(
			BattleSide::LEFT_SIDE,
			startStats.totalValue,
			startStats.totalHp,
			lastStats.totalValue,
			lastStats.totalHp,
			stats.leftValue,
			stats.leftHp,
			logdata.ldd,
			logdata.ldr,
			logdata.lvk,
			logdata.lvl
		));

		G->add(Graph::Nodes::Player(
			BattleSide::LEFT_SIDE,
			startStats.totalValue,
			startStats.totalHp,
			lastStats.totalValue,
			lastStats.totalHp,
			stats.rightValue,
			stats.rightHp,
			logdata.rdd,
			logdata.rdr,
			logdata.rvk,
			logdata.rvl
		));
	}

	void AddUnitNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & startStats,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const std::unordered_map<const CStack *, Graph::Nodes::Unit::Stats> & sstats
	)
	{
		ASSERT(!added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT already added");
		added.set(EU(ET::NODE_UNIT));

		for(auto & cstack : battle.battleGetStacks())
		{
			auto sc = Graph::Nodes::Unit::StatsContainer{
				.bfieldValueNow = stats.totalValue,
				.bfieldValuePrev = lastStats.totalValue,
				.bfieldValueStart = startStats.totalValue,
				.bfieldHpNow = stats.totalHp,
				.bfieldHpPrev = lastStats.totalHp,
				.bfieldHpStart = startStats.totalHp,
				.stackStats = sstats.at(cstack)
			};

			G->add(Graph::Nodes::Unit(*cstack, G->getQueue(), sc));
		}
	}

	void AddHexNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack
	)
	{
		ASSERT(!added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX already added");
		added.set(EU(ET::NODE_HEX));

		auto hexobstacles = std::array<std::vector<std::shared_ptr<const CObstacleInstance>>, 165>{};

		for(const auto & obstacle : battle.battleGetAllObstacles())
			for(const auto & bh : obstacle->getAffectedTiles())
				if(bh.isAvailable())
					hexobstacles.at(Graph::Nodes::Hex::CalcId(bh)).push_back(obstacle);

		auto gatestate = battle.battleGetGateState();
		bool isGateOpen = battle.battleGetFortifications().wallsHealth > 0
			&& (gatestate == EGateState::OPENED || gatestate == EGateState::DESTROYED);

		for(int y = 0; y < 11; ++y)
		{
			for(int x = 0; x < 15; ++x)
			{
				auto i = (y * 15) + x;
				auto bh = BattleHex(x + 1, y);

				G->add(Graph::Nodes::Hex(
					bh,
					G->getAccessibility().at(bh.toInt()),
					acstack ? acstack->unitSide() : BattleSide::LEFT_SIDE,
					hexobstacles.at(i),
					WallHP(battle, bh),
					isGateOpen
				));
			}
		}
	}

	void AddEdges_Hex_Adjacent_Hex(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added
	)
	{
		ASSERT(added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX should be added first");
		ASSERT(!added.test(EU(ET::EDGE_HEX_ADJACENT_HEX)), "Elements of type EDGE_HEX_ADJACENT_HEX already added");
		added.set(EU(ET::EDGE_HEX_ADJACENT_HEX));

		static const auto adjmap = InitAdjMap();
		const auto hexes = G->getAll<Graph::Nodes::Hex>();

		for(const auto & src : hexes)
		{
			for(const auto & dst : hexes)
			{
				auto it = adjmap.find({src.bhex.toInt(), dst.bhex.toInt()});
				if(it == adjmap.end())
					continue;
				G->add(Graph::Edges::Hex_Adjacent_Hex(src, dst, it->second));
			}
		}
	}

	void AddEdges_Unit_Blocks_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added
	)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_BLOCKS_UNIT)), "Elements of type EDGE_HEX_ADJACENT_HEX already added");
		added.set(EU(ET::EDGE_UNIT_BLOCKS_UNIT));

		auto pairs = std::unordered_set<std::pair<int, int>>{};

		for(const auto & unit : G->getAll<Graph::Nodes::Unit>())
		{
			const auto & cstack = unit.cstack;
			for(const auto & bh : unit.cstack.getSurroundingHexes())
			{
				const auto * other = G->findUnitByBHex(bh);

				if(!other)
					continue;

				const auto & ostack = other->cstack;
				auto [_, inserted] = pairs.emplace<std::pair<int, int>>({cstack.unitId(), ostack.unitId()});

				// inserted == false when this pair was already processed
				if(inserted &&
					ostack.unitSide() != cstack.unitSide() &&
					ostack.canShoot() &&
					!ostack.hasBonusOfType(BonusType::FREE_SHOOTING) &&
					!ostack.hasBonusOfType(BonusType::SIEGE_WEAPON)
				) {
					G->add(Graph::Edges::Unit_Blocks_Unit(unit, *other));
				}
			}
		};
	}

	void AddEdges_Unit_MeleeDmg_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats
	)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT)), "Elements of type EDGE_UNIT_MELEE_DMG_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT));

		auto pairs = std::unordered_set<std::pair<int, int>>{};
		const auto & units = G->getAll<Graph::Nodes::Unit>();

		for(const auto & unit : units)
		{
			const auto & cstack = unit.cstack;
			for(const auto & other : units)
			{
				const auto & ostack = other.cstack;
				auto [_, inserted] = pairs.emplace<std::pair<int, int>>({cstack.unitId(), ostack.unitId()});

				if(inserted || ostack.unitSide() == cstack.unitSide())
					continue;

				const auto attinfo = BattleAttackInfo(&cstack, &ostack, 0, false);
				auto retalEstimate = DamageEstimation{};
				const auto attackEstimate = battle.battleEstimateDamage(attinfo, &retalEstimate);

				G->add(Graph::Edges::Unit_MeleeDmg_Unit(
					unit,
					other,
					attackEstimate,
					retalEstimate,
					stats.totalValue,
					stats.totalHp
				));
			}
		};
	}


	void AddEdges_Unit_RangedDmg_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats
	)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT must be added first");
		ASSERT(added.test(EU(ET::EDGE_UNIT_BLOCKS_UNIT)), "Elements of type EDGE_UNIT_BLOCKS_UNIT must be added first");
		ASSERT(added.test(EU(ET::EDGE_UNIT_RANGED_DMG_UNIT)), "Elements of type EDGE_UNIT_RANGED_DMG_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_RANGED_DMG_UNIT));

		for(const auto & unit : G->getAll<Graph::Nodes::Unit>())
		{
			const auto & cstack = unit.cstack;
			if(!cstack.canShoot() || cstack.shots.available() <= 0)
				continue;

			for(const auto & bh : unit.cstack.getSurroundingHexes())
			{
				const auto * other = G->findUnitByBHex(bh);

				if(!other)
					continue;

				const auto & ostack = other->cstack;
				auto [_, inserted] = pairs.emplace<std::pair<int, int>>({cstack.unitId(), ostack.unitId()});

				// inserted == false when this pair was already processed
				if(inserted &&
					ostack.unitSide() != cstack.unitSide() &&
					ostack.canShoot() &&
					!ostack.hasBonusOfType(BonusType::FREE_SHOOTING) &&
					!ostack.hasBonusOfType(BonusType::SIEGE_WEAPON)
				) {
					G->add(Graph::Edges::Unit_Blocks_Unit(unit, *other));
				}
			}
		};
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
	const CStack * acstack,
	int round,
	S15::CombatResult result
)
{
	logAi->debug("onActiveStack: round=%d, result=%d", round, EI(result));
	auto G = std::make_shared<Graph::Graph>(battle, nullptr);

	const auto stats = CalcGlobalStats(battle);
	const auto logdata = ProcessAttackLogs(attackLogs, sstats);
	auto added = std::bitset<EU(S15::Graph::ElementType::_count)>{};

	AddGlobalNode(G, added, battle, acstack, result, round, startStats, stats);
	AddPlayerNodes(G, added, startStats, lastStats, stats, logdata);

	AddUnitNodes(G, added, battle, startStats, lastStats, stats, sstats);
	G->buildAccessibilityCache();
	AddHexNodes(G, added, battle, acstack);
	G->buildQueueCache(isMorale);
	G->buildUnitsByBHexCache();
	G->buildReachabilityCache();

	AddEdges_Hex_Adjacent_Hex(G, added);
	AddEdges_Unit_Blocks_Unit(G, added);
	AddEdges_Unit_MeleeDmg_Unit(G, added, battle, stats);
	AddEdges_Unit_RangedDmg_Unit(G, added);
	// AddEdges_Unit_CanMelee_Unit()
	// AddEdges_Unit_CanShoot_Unit()
	// AddEdges_Unit_ActsBefore_Unit()
	// AddEdges_Unit_Threatens_Hex()
	// AddEdges_Unit_Occupies_Hex()

	// Added last
	// AddActionNodes()
	// AddEdges_ActionExposesTo_Unit()
	// AddEdges_ActionThreatens_Unit()
	// AddEdges_ActionDamages_Unit()
	// AddEdges_ActionEndsAt_Hex()
	// AddEdges_ActionBy_Unit()

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
		const auto * defender = battle.battleGetStackByID(elem.stackAttacked, false);
		const auto * attacker = battle.battleGetStackByID(elem.attackerID, false);

		if(!defender)
		{
			logAi->error("MMAI: received BattleStackAttacked with invalid stackAttacked: " + std::to_string(elem.stackAttacked));
			continue;
		}

		auto bf_valueNow = lastStats.leftValue + lastStats.rightValue;
		auto bf_hpNow = lastStats.leftHp + lastStats.rightHp;
		auto value = elem.killedAmount * Graph::Nodes::Unit::GetValue(defender->unitType());

		attackLogs.emplace_back(AttackLog(
			attacker,
			*defender,
			static_cast<int>(elem.damageAmount),
			static_cast<int>(1000 * elem.damageAmount / bf_hpNow),
			static_cast<int>(elem.killedAmount),
			static_cast<int>(value),
			static_cast<int>(1000 * value / bf_valueNow)
		));
	}
}

void State::onBattleTriggerEffect(const BattleTriggerEffect & bte)
{
	if(bte.effect != BonusType::MORALE)
		return;

	isMorale = true;
}

void State::onBattleEnd(const BattleResult & br, int round)
{
	switch(br.winner)
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
