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

#include "BAI/v15/graph/edges/hex_adjacent_hex.h"
#include "BAI/v15/graph/edges/unit_acts_before_unit.h"
#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"
#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "battle/CPlayerBattleCallback.h"
#include "entities/building/TownFortifications.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/state.h"
#include "BAI/v15/supplementary_data.h"
#include "common.h"
#include "schema/v15/types.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

namespace MMAI::BAI::V15
{

namespace
{
	namespace N = Graph::Nodes;
	namespace E = Graph::Edges;
	using UnitPtr = std::shared_ptr<const N::Unit>;
	using HexPtr = std::shared_ptr<const N::Hex>;
	using TowerFlags = N::Global::TowerFlags;
	using CorpseFlags = N::Global::CorpseFlags;
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
			auto v = stack->getCount() * N::Unit::GetValue(stack->unitType());
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

	namespace Q {
		constexpr auto MaxUnits = S15::STACK_QUEUE_SIZE;
		using UnitId = std::uint32_t;
		using Count = std::uint8_t;
		using CountMatrix = std::array<std::array<Count, MaxUnits>, MaxUnits>;

		// stats.count[A][B] = (number of actions A takes before B's first action)
		struct ActsBeforeMatrix {
			std::array<UnitId, MaxUnits> unique_units{};
			std::size_t unique_count = 0;
			CountMatrix count{};
		};

		std::size_t GetOrAddUnitId(UnitId unitId, ActsBeforeMatrix & result) {
			for (std::size_t i = 0; i < result.unique_count; ++i)
				if (result.unique_units[i] == unitId)
					return i;
			const std::size_t id = result.unique_count++;
			result.unique_units[id] = unitId;
			return id;
		}

		ActsBeforeMatrix BuildActsBeforeMatrix(const CPlayerBattleCallback & battle)
		{
			// XXX: there is a bug in VCMI when high morale occurs:
			//      - the stack acts as if it's already the next unit's turn
			//      - as a result, QueuePos for the ACTIVE stack is non-0
			//        while the QueuePos for the next (non-active) stack is 0
			// (this applies only to good morale; bad morale simply skips turn)
			// As a workaround, a "isMorale" flag is passed whenever the astack is
			// acting because of high morale and queue is "shifted" accordingly.

			auto q = std::vector<UnitId>{};
			auto tmp = std::vector<battle::Units>{};
			battle.battleGetTurnOrder(tmp, S15::STACK_QUEUE_SIZE, 0);
			for(const auto & units : tmp)
			{
				for(const auto & unit : units)
				{
					if(q.size() >= S15::STACK_QUEUE_SIZE)
						break;
					q.push_back(unit->unitId());
				}
			}

			ActsBeforeMatrix result;
			std::array<Count, MaxUnits> seen_counts{};
			std::array<bool, MaxUnits> first_seen{};

			for (UnitId unitId : q) {
				const std::size_t b = GetOrAddUnitId(unitId, result);

				if (!first_seen[b]) {
					first_seen[b] = true;

					for (std::size_t a = 0; a < result.unique_count; ++a) {
						result.count[a][b] = seen_counts[a];
					}
				}

				++seen_counts[b];
			}

			return result;
		}
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
		std::unordered_map<const CStack *, N::Unit::Stats> sstats
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

	bool checkBerserk(const CStack & cstack)
	{
		return cstack.hasBonusOfType(BonusType::ATTACKS_NEAREST_CREATURE);
	}

	void AddGlobalNode(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const S15::CombatResult result,
		const int round,
		const State::GlobalStats & startStats,
		const State::GlobalStats & stats)
	{
		ASSERT(!added.test(EU(ET::NODE_GLOBAL)), "Elements of type NODE_GLOBAL already added");
		added.set(EU(ET::NODE_GLOBAL));

		G->add(std::make_shared<N::Global>(
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
		const AttackLogAggregateData & logdata)
	{
		ASSERT(!added.test(EU(ET::NODE_PLAYER)), "Elements of type NODE_PLAYER already added");
		added.set(EU(ET::NODE_PLAYER));

		static_assert(EU(BattleSide::LEFT_SIDE) == 0);
		static_assert(EU(BattleSide::RIGHT_SIDE) == 1);

		G->add(std::make_shared<N::Player>(
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

		G->add(std::make_shared<N::Player>(
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
		const CStack * acstack,
		const State::GlobalStats & startStats,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const std::unordered_map<const CStack *, N::Unit::Stats> & sstats)
	{
		ASSERT(!added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT already added");
		added.set(EU(ET::NODE_UNIT));

		for(auto & cstack : battle.battleGetStacks())
		{
			auto sc = N::Unit::StatsContainer{
				.bfieldValueNow = stats.totalValue,
				.bfieldValuePrev = lastStats.totalValue,
				.bfieldValueStart = startStats.totalValue,
				.bfieldHpNow = stats.totalHp,
				.bfieldHpPrev = lastStats.totalHp,
				.bfieldHpStart = startStats.totalHp,
				.stackStats = sstats.at(cstack)
			};

			G->add(std::make_shared<N::Unit>(*cstack, sc, acstack == cstack));
		}
	}

	void AddHexNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		ASSERT(!added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX already added");
		added.set(EU(ET::NODE_HEX));

		auto hexobstacles = std::array<std::vector<std::shared_ptr<const CObstacleInstance>>, 165>{};

		for(const auto & obstacle : battle.battleGetAllObstacles())
			for(const auto & bh : obstacle->getAffectedTiles())
				if(bh.isAvailable())
					hexobstacles.at(N::Hex::CalcId(bh)).push_back(obstacle);

		auto gatestate = battle.battleGetGateState();
		bool isGateOpen = battle.battleGetFortifications().wallsHealth > 0
			&& (gatestate == EGateState::OPENED || gatestate == EGateState::DESTROYED);

		for(int y = 0; y < 11; ++y)
		{
			for(int x = 0; x < 15; ++x)
			{
				auto i = (y * 15) + x;
				auto bh = BattleHex(x + 1, y);
				ASSERT(bh.isAvailable(), "invalid bhex");

				G->add(std::make_shared<N::Hex>(
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
		GraphBits & added)
	{
		ASSERT(added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX should be added first");
		ASSERT(!added.test(EU(ET::EDGE_HEX_ADJACENT_HEX)), "Elements of type EDGE_HEX_ADJACENT_HEX already added");
		added.set(EU(ET::EDGE_HEX_ADJACENT_HEX));

		static const auto adjmap = InitAdjMap();
		const auto & hexes = G->getAll<N::Hex>();

		for(const auto & src : hexes)
		{
			for(const auto & dst : hexes)
			{
				auto it = adjmap.find({src->bhex.toInt(), dst->bhex.toInt()});
				if(it == adjmap.end())
					continue;
				G->add(std::make_shared<E::Hex_Adjacent_Hex>(src, dst, it->second));
			}
		}
	}

	void AddEdges_Unit_ActsBefore_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_ACTS_BEFORE_UNIT)), "Elements of type EDGE_UNIT_ACTS_BEFORE_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_ACTS_BEFORE_UNIT));

		const auto matrix = Q::BuildActsBeforeMatrix(battle);
		const auto & nodes = G->getAll<N::Unit>();

		ASSERT(matrix.unique_count <= nodes.size(), "unique_count exceeds size of graph nodes");

		for(int i = 0; i < matrix.unique_count; ++i)
		{
			auto unitId = matrix.unique_units.at(i);
			// XXX: assuming unit ID is the same as CStack ID. This must be OK
			// since as of 2026, battle::Unit is just a superclass of CStack.
			const auto & unit = G->getByExtraIndex<N::Unit>(unitId);
			ASSERT(unit != nullptr, "unit not found: " + std::to_string(unitId));

			for(int j = 0; j < matrix.unique_count; ++j)
			{
				auto times = matrix.count[i][j];
				if(times == 0)
					continue;

				auto otherId = matrix.unique_units.at(j);
				const auto & other = G->getByExtraIndex<N::Unit>(otherId);
				ASSERT(other != nullptr, "unit not found: " + std::to_string(otherId));
				G->add(std::make_shared<E::Unit_ActsBefore_Unit>(unit, other, times));
			}
		}
	}

	void AddEdges_Unit_MeleeDmg_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT)), "Elements of type EDGE_UNIT_MELEE_DMG_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT));

		auto pairs = std::unordered_set<std::pair<int, int>>{};
		const auto & units = G->getAll<N::Unit>();

		for(const auto & unit : units)
		{
			const auto & cstack = unit->cstack;
			bool isBerserk = checkBerserk(cstack);

			for(const auto & other : units)
			{
				if(unit == other)
					continue;

				const auto & ostack = other->cstack;
				auto [_, inserted] = pairs.emplace<std::pair<int, int>>({cstack.unitId(), ostack.unitId()});

				if(!inserted) // key already existed
					continue;

				if(ostack.unitSide() == cstack.unitSide() && !isBerserk && !checkBerserk(ostack))
					continue;

				const auto attinfo = BattleAttackInfo(&cstack, &ostack, 0, false);
				auto retalEstimate = DamageEstimation{};
				const auto attackEstimate = battle.battleEstimateDamage(attinfo, &retalEstimate);

				G->add(std::make_shared<E::Unit_MeleeDmg_Unit>(
					unit,
					other,
					attackEstimate,
					retalEstimate,
					stats.totalValue,
					stats.totalHp
				));
			}
		}
	}

	void AddEdges_Unit_ShootDmg_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		ASSERT(added.test(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT)), "Elements of type EDGE_UNIT_MELEE_DMG_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_SHOOT_DMG_UNIT)), "Elements of type EDGE_UNIT_SHOOT_DMG_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_SHOOT_DMG_UNIT));

		// RANGED_DMG edges use a subset of the MELEE_DMG edge nodes
		// (all ranged units can also melee)
		for(const auto & edge : G->getAll<E::Unit_MeleeDmg_Unit>())
		{
			const auto & unit = edge->srcNode;
			const auto & cstack = unit->cstack;
			if(!cstack.canShoot())
				continue;

			const auto & other = edge->dstNode;
			const auto & ostack = other->cstack;
			const auto attinfo = BattleAttackInfo(&cstack, &ostack, 0, true);
			const auto estimate = battle.battleEstimateDamage(attinfo);
			G->add(std::make_shared<E::Unit_ShootDmg_Unit>(
				unit,
				other,
				estimate,
				stats.totalValue,
				stats.totalHp
			));
		}
	}

	void AddEdges_Unit_Blocks_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added)
	{
		ASSERT(added.test(EU(ET::EDGE_UNIT_SHOOT_DMG_UNIT)), "Elements of type EDGE_UNIT_SHOOT_DMG_UNIT must be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_BLOCKS_UNIT)), "Elements of type EDGE_UNIT_BLOCKS_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_BLOCKS_UNIT));

		// BLOCKS edges use a subset of the RANGED_DMG edge nodes
		// (all blocked units must be ranged units)
		for(const auto & edge : G->getAll<E::Unit_ShootDmg_Unit>())
		{
			const auto & unit = edge->srcNode;
			const auto & cstack = unit->cstack;

			if(cstack.canShootBlocked())
				continue;

			const auto & other = edge->dstNode;
			const auto & ostack = other->cstack;

			// ATTACKS_NEAREST_CREATURE == berserk
			// XXX: what about hypnotize?
			if(cstack.unitSide() == ostack.unitSide() && !cstack.hasBonusOfType(BonusType::ATTACKS_NEAREST_CREATURE))
				continue;

			for(const auto & bhex : cstack.getSurroundingHexes())
			{
				if(ostack.coversPos(bhex))
				{
					G->add(std::make_shared<E::Unit_Blocks_Unit>(unit, other));
					break;
				}
			}
		}
	}

	void AddEdges_Unit_Occupies_Hex(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT should be added first");
		ASSERT(added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX should be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_OCCUPIES_HEX)), "Elements of type EDGE_UNIT_OCCUPIES_HEX already added");
		added.set(EU(ET::EDGE_UNIT_OCCUPIES_HEX));

		for(const auto & unit : G->getAll<N::Unit>())
		{
			const auto & cstack = unit->cstack;
			for(const auto & bhex : cstack.getHexes())
			{
				if(!bhex.isAvailable())
					continue;
				const auto & hex = G->getByExtraIndex<N::Hex>(bhex.toInt());
				ASSERT(hex != nullptr, "hex not found: " + std::to_string(bhex.toInt()));
				G->add(std::make_shared<E::Unit_Occupies_Hex>(unit, hex));
			}
		}
	}

	void AddActionNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		ASSERT(added.test(EU(ET::EDGE_UNIT_OCCUPIES_HEX)), "Elements of type EDGE_UNIT_OCCUPIES_HEX should be added first");
		ASSERT(added.test(EU(ET::EDGE_HEX_ADJACENT_HEX)), "Elements of type EDGE_HEX_ADJACENT_HEX should be added first");
		ASSERT(added.test(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT)), "Elements of type EDGE_UNIT_MELEE_DMG_UNIT should be added first");
		ASSERT(added.test(EU(ET::EDGE_UNIT_SHOOT_DMG_UNIT)), "Elements of type EDGE_UNIT_SHOOT_DMG_UNIT should be added first");
		ASSERT(added.test(EU(ET::EDGE_UNIT_ACTS_BEFORE_UNIT)), "Elements of type EDGE_UNIT_ACTS_BEFORE_UNIT should be added first");
		ASSERT(!added.test(EU(ET::NODE_ACTION)), "Elements of type NODE_ACTION already added");
		ASSERT(!added.test(EU(ET::NODE_ACTACTION)), "Elements of type NODE_ACTACTION already added");
		added.set(EU(ET::NODE_ACTION));
		added.set(EU(ET::NODE_ACTACTION));

		auto aunit = acstack ? G->getByExtraIndex<N::Unit>(acstack->unitId()) : nullptr;

		constexpr std::array<ET, 9> ACTION_EDGE_TYPES {
			ET::EDGE_ACTION_BY_UNIT,
			ET::EDGE_ACTION_BLOCKS_UNIT,
			ET::EDGE_ACTION_ENDS_AT_HEX,
			ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT,
			ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT,
			ET::EDGE_ACTION_MELEES_UNIT,
			ET::EDGE_ACTION_SHOOTS_UNIT,
        	ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT,
        	ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT,
		};

		constexpr int offset = 12;
		// Verify the order (to ensure bitset positions match the enum)
		static_assert(EU(ACTION_EDGE_TYPES[0]) == offset + 0);
		static_assert(EU(ACTION_EDGE_TYPES[1]) == offset + 1);
		static_assert(EU(ACTION_EDGE_TYPES[2]) == offset + 2);
		static_assert(EU(ACTION_EDGE_TYPES[3]) == offset + 3);
		static_assert(EU(ACTION_EDGE_TYPES[4]) == offset + 4);
		static_assert(EU(ACTION_EDGE_TYPES[5]) == offset + 5);
		static_assert(EU(ACTION_EDGE_TYPES[6]) == offset + 6);
		static_assert(EU(ACTION_EDGE_TYPES[7]) == offset + 7);
		static_assert(EU(ACTION_EDGE_TYPES[8]) == offset + 8);
		static_assert(ACTION_EDGE_TYPES.size() == 9);

        using Checks = std::bitset<ACTION_EDGE_TYPES.size()>;

		struct ActionInfo {
			const bool active;
			const S15::ActionType actionType;

			// TODO:
			// For best performance, better to have both set and vector
			// - set is fast for access by key
			// - vector is fast for iteration
			//
			// Alternatively, use vector with {key, value} entries
			// and implement find() using seq scan -- will be faster if size < 10
			//

			UnitPtr by; 									// [0] EDGE_ACTION_BY_UNIT
			std::unordered_set<UnitPtr> blocks; 			// [1] EDGE_ACTION_BLOCKS_UNIT
			std::unordered_set<HexPtr> endsAt; 				// [2] EDGE_ACTION_ENDS_AT_HEX
			std::unordered_set<UnitPtr> exposesToMeleeFrom;	// [3] EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT
			std::unordered_set<UnitPtr> exposesToShootFrom;	// [4] EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT
			std::unordered_set<UnitPtr> melees; 			// [5] EDGE_ACTION_MELEES_UNIT
			std::unordered_set<UnitPtr> shoots; 			// [6] EDGE_ACTION_SHOOTS_UNIT
			std::unordered_set<UnitPtr> enablesMeleeAt; 	// [7] EDGE_ACTION_ENABLES_MELEE_AT_UNIT
			std::unordered_set<UnitPtr> enablesShootAt; 	// [8] EDGE_ACTION_ENABLES_SHOOT_AT_UNIT

			ActionInfo(const ActionInfo &) = delete;
			ActionInfo & operator=(const ActionInfo &) = delete;
			ActionInfo(ActionInfo &&) = delete;
			ActionInfo & operator=(ActionInfo &&) = delete;

			explicit ActionInfo(bool active, S15::ActionType actionType)
			: active(active), actionType(actionType) {};

			// copy into this, but via a regular method
			// (the implicit copy constructor is be deleted to prevent accidents)
			void copyFrom(const ActionInfo & other)
			{
				by = other.by;
				blocks = other.blocks;
				endsAt = other.endsAt;
				exposesToMeleeFrom = other.exposesToMeleeFrom;
				exposesToShootFrom = other.exposesToShootFrom;
				melees = other.melees;
				shoots = other.shoots;
				enablesMeleeAt = other.enablesMeleeAt;
				enablesShootAt = other.enablesShootAt;
			};
		};

        auto movechecks = Checks{};

        auto check = [](Checks & checks, ET et)
        {
            assert(EU(et) < checks.size());
            int i = EU(et) - offset;
            assert(!checks.test(i));
            checks.set(i);
        };

        auto isChecked = [](Checks & checks, ET et)
        {
            assert(EU(et) < checks.size());
            int i = EU(et) - offset;
            return checks.test(i);
        };

		using ActInfoPtr = std::shared_ptr<ActionInfo>;
		auto allmoveinfos = std::unordered_map<UnitPtr, std::unordered_map<HexPtr, ActInfoPtr>>{};

		auto toDstHexFlags = [](const std::unordered_map<HexPtr, ActInfoPtr> & moveinfos)
		{
			auto flags = std::bitset<GameConstants::BFIELD_SIZE>{};
			for (const auto & [_, moveinfo] : moveinfos)
				for (const auto & hex : moveinfo->endsAt)
					flags.set(hex->bhex.toInt());

			return flags;
		};

		//
		// 1st pass: MOVE actions (only BY + ENDS_AT edges)
		//
		for(const auto et : {
			ET::EDGE_ACTION_BY_UNIT,
			ET::EDGE_ACTION_ENDS_AT_HEX
		})
		{
			assert(!isChecked(movechecks, et));

			for(const auto & unit : G->getAll<N::Unit>())
			{
				const auto & stack = unit->cstack;
				const auto & reachability = G->getReachability(stack);
				bool isActive = unit == aunit;

				auto & moveinfos = allmoveinfos.try_emplace(unit, std::unordered_map<HexPtr, ActInfoPtr>{}).first->second;

				for(const auto & hex : G->getAll<N::Hex>())
				{
					const auto & bhex = hex->bhex;
					if(reachability.distances.at(bhex.toInt()) > stack.getMovementRange())
						continue;

					auto & moveinfo = moveinfos.try_emplace(hex, std::make_shared<ActionInfo>(isActive, S15::ActionType::MOVE)).first->second;

					switch(et) {
					case ET::EDGE_ACTION_BY_UNIT:
					{
						moveinfo->by = unit;
						break;
					}
					case ET::EDGE_ACTION_ENDS_AT_HEX:
					{
						moveinfo->endsAt.emplace(hex);
						if(unit->cstack.doubleWide())
						{
							const auto rearbhex = unit->cstack.occupiedHex(hex->bhex, true, unit->cstack.unitSide());
							moveinfo->endsAt.emplace(G->getByExtraIndex<N::Hex>(rearbhex.toInt()));
						}
						break;
					}
					default:
						throw std::runtime_error("Unexpected edge type: " + std::to_string(EU(et)));
					}
				}
			}

			check(movechecks, et);
		}

		//
		// 2nd pass: MOVE actions (all edges)
		//
		for(const auto et : ACTION_EDGE_TYPES)
		{
			if (isChecked(movechecks, et))
				continue;

			for(const auto & unit : G->getAll<N::Unit>())
			{
				const auto & stack = unit->cstack;
				const auto & moveinfos = allmoveinfos.at(unit);

				// this is used in only 1 edge
				auto dstflags = (et == ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT)
					? toDstHexFlags(moveinfos)
					: std::bitset<GameConstants::BFIELD_SIZE>{};

				for(const auto & hex : G->getAll<N::Hex>())
				{
					const auto & bhexes = stack.getHexes();
					const auto & moveinfo = moveinfos.at(hex);

					switch(et) {
					case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
					{
						// Plan:
						// Find units which:
						// 	1. are enemies (i.e. with "MeleeDmg" edge to us)
						//  2. will act after us (i.e. "ActsBefore" us->them has times==1)
						//  3. can move such that they will end up occupying
						// 		at least of the hexes around our new position
						//  4. the move from 3. doesn't overlap with our own move
						for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_MeleeDmg_Unit>(unit))
						{
							if(G->getEdgeBySrcDst<E::Unit_ActsBefore_Unit>(unit, ounit)->times > 1)
								continue;

							const auto & adjbhexes = stack.getSurroundingHexes();
							for (const auto &[_ohex, omoveinfo] : allmoveinfos.at(ounit)) {
								// must check for overlaps to ensure no exposure is set if
								// the hypothetical endsAt hexes of both stacks overlap
								// e.g. both our and enemy stack can move onto the "x" hexes
								// 		BUT if we move there, the enemy would be unable to
								// 		=> we would NOT be exposed
								// 		(x=endsAt, @=obstacle)
								//
								// . . @ @ @ . . . . .
								//  @ . x x @ . 1 1 .
								// . . @ @ @ . . . . .
								//  . . . . . 2 2 . .
								// . . . . . . . . . .
								assert(isChecked(movechecks, ET::EDGE_ACTION_ENDS_AT_HEX));
								bool candidate = false;
								bool overlap = false;
								for (const auto & ohex : omoveinfo->endsAt) {
									candidate |= adjbhexes.contains(ohex->bhex);
									overlap |= bhexes.contains(ohex->bhex);
								}

								if (candidate && !overlap) {
									moveinfo->exposesToMeleeFrom.emplace(ounit);
								}
							}
						}
						break;
					}
					case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
					{
						// Plan:
						// Find units which:
						// 	1. are shooter enemies (i.e. with "RangedDmg" edge to us)
						// 	2. either:
						//		a. are unblocked and will remain unblocked after we move
						// 	    b. are blocked only by us, but will become unblocked after we move
						for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_ShootDmg_Unit>(unit))
						{
							const auto & blockers = G->getAllEdgesSrcByDst<E::Unit_Blocks_Unit>(ounit);
							auto numBlockers = std::ranges::distance(blockers);

							assert(isChecked(movechecks, ET::EDGE_ACTION_BLOCKS_UNIT));
							bool willBeBlocked = std::ranges::find(moveinfo->blocks, ounit) != moveinfo->blocks.end();

							if ((numBlockers == 0 && !willBeBlocked) ||
									(numBlockers == 1 && !willBeBlocked && *std::ranges::begin(blockers) == unit))
							{
								moveinfo->exposesToShootFrom.emplace(ounit);
							}
						}
						break;
					}
					case ET::EDGE_ACTION_MELEES_UNIT:
					case ET::EDGE_ACTION_SHOOTS_UNIT:
						// nothing to do (can't attack with MOVE actions)
						break;
					case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
					{
						// Plan:
						// Find units which:
						// 	1. Have "MeleeDmg" edge from us
						//  2. we can move from our hypothetical new position
						// 		to at least 1 hex around their current position

						// XXX: 2. needs reachability for the (unit, hypothethicalNewPos) combo
						// 	 	This means calculating up to 14 stacks * 151 hexes = 2114 reachabilities.
						//
						// https://trello.com/c/hRe8u4CX/233-reachability-notes

						/*
						 * LEAVE IT FOR NOW.
						 * 1. Compile v15 without this edge
						 * 2. Then add it, calculating all reachabilities
						 * 		(compare traditional makeBFS with optimized versions)
						 */

						// throw std::runtime_error("not implemented");
						break;
					}
					case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
					{
						// Plan:
						//  1. Check if we:
						// 		- can shoot
						// 		- will be unblocked after we move
						// 	1. If yes, find units with "ShootDmg" edge from us
						//
						if (!stack.canShoot())
							break;

						const bool willBeBlocked = [&G, &unit, &hex]() {
							for (const auto & adjbhex : unit->cstack.getSurroundingHexes(hex->bhex))
							{
								const auto & adjhex = G->getByExtraIndex<N::Hex>(adjbhex.toInt());
								for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_Occupies_Hex>(adjhex))
								{
									// Checking if unit side != ounit side does not account for berserks
									// => checking if Unit_MeleeDmg_Unit exists for this pair
									if (G->getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(ounit, unit, false))
										return true;
								}
							}
							return false;
						}();

						if(willBeBlocked)
							break;

						for (const auto & ounit : G->getAllEdgesDstBySrc<E::Unit_ShootDmg_Unit>(unit))
							moveinfo->enablesShootAt.emplace(ounit);

						break;
					}
					default:
						throw std::runtime_error("Unexpected edge type: " + std::to_string(EU(et)));
					}
				}
			}

			check(movechecks, et);
		}

		//
		// 3rd pass: All other actions: WAIT, SHOOT, AMOVE
		//

		// Plan:
		//  for {unit, moveinfos} in allmoveinfos:
		// 	  defendinfo = moveinfos.at(unit.pos)
		// 	  add_wait(defendinfo)  // +clone edges
		//
		// 	  for (enemy in defendinfo.enablesShootAt)
		//    	add_shoot(enemy, defending)  // +clone edges
		//
		//    for {hex, moveinfo} in moveinfos:
		// 		for nbhex : unit.getSurroundingHexes(hex):
		// 			if enemy_occupies(nbhex) && have_edge<melee_dmg>(unit, enemy):
		// 				add_amove(unit, hex, enemy)
		//

		auto allwaitinfos = std::unordered_map<UnitPtr, std::unordered_map<HexPtr, ActInfoPtr>>{};
		auto allshootinfos = std::unordered_map<UnitPtr, std::unordered_map<HexPtr, ActInfoPtr>>{};
		auto allamoveinfos = std::unordered_map<UnitPtr, std::unordered_map<HexPtr, ActInfoPtr>>{};

		for (const auto & [unit, moveinfos] : allmoveinfos)
		{
			bool isActive = unit == aunit;
			auto & amoveinfos = allamoveinfos.try_emplace(unit, std::unordered_map<HexPtr, ActInfoPtr>{}).first->second;

			// AMOVE
			for (const auto & [hex, moveinfo] : moveinfos)
			{
				for (const auto & adjbhex : unit->cstack.getSurroundingHexes(hex->bhex))
				{
					const auto & adjhex = G->getByExtraIndex<N::Hex>(adjbhex.toInt());
					for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_Occupies_Hex>(adjhex))
					{
						if (!G->getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(unit, ounit, false))
							continue;

						auto & amoveinfo = amoveinfos.try_emplace(hex, std::make_shared<ActionInfo>(isActive, S15::ActionType::AMOVE)).first->second;
						amoveinfo->copyFrom(*moveinfo);
						amoveinfo->melees.emplace(ounit);
					}
				}
			}

			const auto & defendhex = G->getByExtraIndex<N::Hex>(unit->cstack.getPosition());
			const auto defendinfo = moveinfos.at(defendhex);
			auto & shootinfos = allshootinfos.try_emplace(unit, std::unordered_map<HexPtr, ActInfoPtr>{}).first->second;

			// SHOOT
			for (const auto & ounit : defendinfo->enablesShootAt)
			{
				for (const auto & hex : G->getAllEdgesDstBySrc<E::Unit_Occupies_Hex>(ounit))
				{
					auto & shootinfo = shootinfos.try_emplace(hex, std::make_shared<ActionInfo>(isActive, S15::ActionType::SHOOT)).first->second;
					shootinfo->copyFrom(*defendinfo); // SHOOT action is for hex, but ends at defendhex
					shootinfo->shoots.emplace(ounit);
				}
			}

			// Wait
			if (!unit->cstack.waitedThisTurn)
			{
				auto & waitinfos = allwaitinfos.try_emplace(unit, std::unordered_map<HexPtr, ActInfoPtr>{}).first->second;
				auto & waitinfo = waitinfos.try_emplace(defendhex, std::make_shared<ActionInfo>(isActive, S15::ActionType::WAIT)).first->second;
				waitinfo->copyFrom(*defendinfo);
			}
		}
	}

	/* REDUNDANT because Action_ExposesToMeleeFrom_Unit gives better version
		of the same info. Difference is
		1/ the direction
		2/ stone golems at opposite sides - it is irrelevant that each can
			"threaten" 3 hexes when they are so far
	void AddEdges_Unit_Threatens_Hex(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added)
	{
		ASSERT(added.test(EU(ET::NODE_UNIT)), "Elements of type NODE_UNIT should be added first");
		ASSERT(added.test(EU(ET::NODE_HEX)), "Elements of type NODE_HEX should be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_THREATENS_HEX)), "Elements of type EDGE_UNIT_THREATENS_HEX already added");
		added.set(EU(ET::EDGE_UNIT_THREATENS_HEX));

		for(const auto & unit : G->getAll<N::Unit>())
		{
			const auto & cstack = unit.cstack;
			const auto & speed = cstack.getMovementRange();
			const auto & reach = G->getReachability(cstack);

			for(const auto & hex : G->getAll<N::Hex>())
			{
				const auto & bhex = hex.bhex;
				for(const auto & nbh : bhex.getNeighbouringTiles())
				{
					if(reach.distances.at(nbh.toInt()) <= speed || G->isRUFR(cstack, nbh))
					{
						G->add(std::make_shared<E::Unit_Threatens_Hex>(unit, hex));
						break;
					}
				}
			}
		}
	}
	*/

}

State::State(
	int version_,
	const std::string & colorname,
	const CPlayerBattleCallback & battle)
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
	S15::CombatResult result)
{
	logAi->debug("onActiveStack: round=%d, result=%d", round, EI(result));
	auto G = std::make_shared<Graph::Graph>(battle, nullptr);

	const auto stats = CalcGlobalStats(battle);
	const auto logdata = ProcessAttackLogs(attackLogs, sstats);
	auto added = std::bitset<EU(S15::Graph::ElementType::_count)>{};

	G->buildAccessibilityCache();

	AddGlobalNode(G, added, battle, acstack, result, round, startStats, stats);
	AddPlayerNodes(G, added, startStats, lastStats, stats, logdata);
	AddUnitNodes(G, added, battle, acstack, startStats, lastStats, stats, sstats);
	AddHexNodes(G, added, battle, acstack);

	G->buildReachabilityCache(); // requires added Units

	AddEdges_Hex_Adjacent_Hex(G, added);
	AddEdges_Unit_ActsBefore_Unit(G, added, battle);
	AddEdges_Unit_MeleeDmg_Unit(G, added, battle, stats);
	AddEdges_Unit_ShootDmg_Unit(G, added, battle, stats);
	AddEdges_Unit_Blocks_Unit(G, added);
	AddEdges_Unit_Occupies_Hex(G, added);

	AddActionNodes(G, added, battle, acstack); // add last; also adds Actaction nodes
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
		auto value = elem.killedAmount * N::Unit::GetValue(defender->unitType());

		attackLogs.emplace_back(
			attacker,
			*defender,
			static_cast<int>(elem.damageAmount),
			static_cast<int>(1000 * elem.damageAmount / bf_hpNow),
			static_cast<int>(elem.killedAmount),
			static_cast<int>(value),
			static_cast<int>(1000 * value / bf_valueNow)
		);
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
