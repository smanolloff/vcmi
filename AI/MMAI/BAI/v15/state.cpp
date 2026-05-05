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
#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "battle/CPlayerBattleCallback.h"
#include "entities/building/TownFortifications.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/state.h"
#include "BAI/v15/supplementary_data.h"
#include "common.h"
#include "schema/v15/types.h"

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
		const State::GlobalStats & stats)
	{
		ASSERT(!added.test(EU(ET::NODE_GLOBAL)), "Elements of type NODE_GLOBAL already added");
		added.set(EU(ET::NODE_GLOBAL));

		G->add(std::make_shared<Graph::Nodes::Global>(
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

		G->add(std::make_shared<Graph::Nodes::Player>(
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

		G->add(std::make_shared<Graph::Nodes::Player>(
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
		const std::unordered_map<const CStack *, Graph::Nodes::Unit::Stats> & sstats)
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

			G->add(std::make_shared<Graph::Nodes::Unit>(*cstack, sc, acstack == cstack));
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
				ASSERT(bh.isAvailable(), "invalid bhex");

				G->add(std::make_shared<Graph::Nodes::Hex>(
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
		const auto hexes = G->getAll<Graph::Nodes::Hex>();

		for(const auto & src : hexes)
		{
			for(const auto & dst : hexes)
			{
				auto it = adjmap.find({src.bhex.toInt(), dst.bhex.toInt()});
				if(it == adjmap.end())
					continue;
				G->add(std::make_shared<Graph::Edges::Hex_Adjacent_Hex>(src, dst, it->second));
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
		const auto & nodes = G->getAll<Graph::Nodes::Unit>();

		ASSERT(matrix.unique_count <= nodes.size(), "unique_count exceeds size of graph nodes");

		for(int i = 0; i < matrix.unique_count; ++i)
		{
			auto unitId = matrix.unique_units.at(i);
			// XXX: assuming unit ID is the same as CStack ID. This must be OK
			// since as of 2026, battle::Unit is just a superclass of CStack.
			const auto & unit = G->getByExtraIndex<Graph::Nodes::Unit>(unitId);
			ASSERT(unit, "unit not found: " + std::to_string(unitId));

			for(int j = 0; j < matrix.unique_count; ++j)
			{
				auto times = matrix.count[i][j];
				if(times == 0)
					continue;

				auto otherId = matrix.unique_units.at(j);
				const auto & other = G->getByExtraIndex<Graph::Nodes::Unit>(otherId);
				ASSERT(other, "unit not found: " + std::to_string(otherId));
				G->add(std::make_shared<Graph::Edges::Unit_ActsBefore_Unit>(*unit, *other, times));
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

				G->add(std::make_shared<Graph::Edges::Unit_MeleeDmg_Unit>(
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
		for(const auto & edge : G->getAll<Graph::Edges::Unit_MeleeDmg_Unit>())
		{
			const auto & unit = edge.srcNode;
			const auto & cstack = unit.cstack;
			if(!cstack.canShoot() || cstack.shots.available() <= 0)
				continue;

			const auto & other = edge.dstNode;
			const auto & ostack = other.cstack;
			const auto attinfo = BattleAttackInfo(&cstack, &ostack, 0, true);
			const auto estimate = battle.battleEstimateDamage(attinfo);
			G->add(std::make_shared<Graph::Edges::Unit_ShootDmg_Unit>(
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
		for(const auto & edge : G->getAll<Graph::Edges::Unit_ShootDmg_Unit>())
		{
			const auto & unit = edge.srcNode;
			const auto & cstack = unit.cstack;

			// These can never be blocked.
			if(cstack.hasBonusOfType(BonusType::FREE_SHOOTING) || cstack.hasBonusOfType(BonusType::SIEGE_WEAPON))
				continue;

			const auto & other = edge.dstNode;
			const auto & ostack = other.cstack;

			for(const auto & bhex : cstack.getSurroundingHexes())
			{
				if(ostack.coversPos(bhex))
				{
					G->add(std::make_shared<Graph::Edges::Unit_Blocks_Unit>(unit, other));
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

		for(const auto & unit : G->getAll<Graph::Nodes::Unit>())
		{
			const auto & cstack = unit.cstack;
			for(const auto & bhex : cstack.getHexes())
			{
				if(!bhex.isAvailable())
					continue;
				const auto & hex = G->getByExtraIndex<Graph::Nodes::Hex>(bhex.toInt());
				ASSERT(hex, "hex not found: " + std::to_string(bhex.toInt()));
				G->add(std::make_shared<Graph::Edges::Unit_Occupies_Hex>(unit, *hex));
			}
		}
	}

	void AddActionNodes(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		ASSERT(!added.test(EU(ET::NODE_ACTION)), "Elements of type NODE_ACTION already added");
		ASSERT(!added.test(EU(ET::NODE_ACTACTION)), "Elements of type NODE_ACTACTION already added");
		added.set(EU(ET::NODE_ACTION));
		added.set(EU(ET::NODE_ACTACTION));

		// WAIT action

		for(const auto & hex : G->getAll<Graph::Nodes::Hex>())
		{
			if(hex.statemask.test(EU(Schema::V15::HexState::OBSTACLE)))
				continue;

			const auto & bhex = hex.bhex;
			for(const auto & unit : G->getAll<Graph::Nodes::Unit>())
			{
				const auto & cstack = unit.cstack;
				bool isActive = (&cstack) == acstack;

				const auto & reachability = G->getReachability(cstack);
				if(reachability.distances.at(bhex.toInt()) <= cstack.getMovementRange())
				{
					const auto & move = G->add(std::make_shared<Graph::Nodes::Action>());
				}
			}
		}
	}

	/* REDUNDANT because it is just saves 1 message passing layer compared
		to UNIT_THREATENS_HEX.
	void AddEdges_Unit_Threatens_Unit(
		std::shared_ptr<Graph::Graph> & G,
		GraphBits & added)
	{
		ASSERT(added.test(EU(ET::EDGE_UNIT_MELEE_DMG_UNIT)), "Elements of type EDGE_UNIT_MELEE_DMG_UNIT should be added first");
		ASSERT(!added.test(EU(ET::EDGE_UNIT_THREATENS_UNIT)), "Elements of type EDGE_UNIT_THREATENS_UNIT already added");
		added.set(EU(ET::EDGE_UNIT_THREATENS_UNIT));

		// THREATENS_UNIT edges use a subset of the MELEE_DMG edge nodes
		for(const auto & edge : G->getAll<Graph::Edges::Unit_MeleeDmg_Unit>())
		{
			const auto & unit = edge.srcNode;
			const auto & cstack = unit.cstack;

			const auto & other = edge.dstNode;
			const auto & ostack = other.cstack;
			const auto & oreach = G->getReachability(ostack);
			const auto & ospeed = ostack.getMovementRange();

			for(const auto & bhex : cstack.getSurroundingHexes())
			{
				if(oreach.distances.at(bhex.toInt()) <= ospeed || G->isRUFR(ostack, bhex))
				{
					G->add(std::make_shared<Graph::Edges::Unit_Threatens_Unit>(unit, other));
					break;
				}
			}
		}
	}
	*/

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

		for(const auto & unit : G->getAll<Graph::Nodes::Unit>())
		{
			const auto & cstack = unit.cstack;
			const auto & speed = cstack.getMovementRange();
			const auto & reach = G->getReachability(cstack);

			for(const auto & hex : G->getAll<Graph::Nodes::Hex>())
			{
				const auto & bhex = hex.bhex;
				for(const auto & nbh : bhex.getNeighbouringTiles())
				{
					if(reach.distances.at(nbh.toInt()) <= speed || G->isRUFR(cstack, nbh))
					{
						G->add(std::make_shared<Graph::Edges::Unit_Threatens_Hex>(unit, hex));
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
		auto value = elem.killedAmount * Graph::Nodes::Unit::GetValue(defender->unitType());

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
