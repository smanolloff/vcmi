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

#include "BAI/v15/graph/edges/action_ends_at_hex.h"
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
#include <string>
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
	using ActionPtr = std::shared_ptr<const N::Action>;
	using TowerFlags = N::Global::TowerFlags;
	using CorpseFlags = N::Global::CorpseFlags;
	using ET = S15::Graph::ElementType;
	using AT = S15::ActionType;

	template <typename T>
	struct Flags
	{
		std::bitset<EU(T::_count)> flags;

	    void set(T v)
	    {
	        assert(EU(v) < flags.size());
	        assert(!flags.test(EU(v)));
	        flags.set(EU(v));
	    };

	    bool isSet(T v)
	    {
	        assert(EU(v) < flags.size());
	        return flags.test(EU(v));
	    };

	    void require(T v)
	    {
	        if (!isSet(v))
	        	throw std::runtime_error("Required flag is not set: " + std::to_string(EU(v)));
	    }

	    void reject(T v)
	    {
	        if (isSet(v))
	        	throw std::runtime_error("Rejected flag is set: " + std::to_string(EU(v)));
	    }

	    void requireExclusive(T v)
	    {
	    	for(int i = 0; i < EU(T::_count); ++i)
	    		EU(v) == i ? require(v) : reject(v);
	    }
	};

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const S15::CombatResult result,
		const int round,
		const State::GlobalStats & startStats,
		const State::GlobalStats & stats)
	{
		etFlags.set(ET::NODE_GLOBAL);

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
		Flags<ET> & etFlags,
		const State::GlobalStats & startStats,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const AttackLogAggregateData & logdata)
	{
		etFlags.set(ET::NODE_PLAYER);

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const State::GlobalStats & startStats,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const std::unordered_map<const CStack *, N::Unit::Stats> & sstats)
	{
		etFlags.set(ET::NODE_UNIT);

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		etFlags.set(ET::NODE_HEX);

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
		Flags<ET> & etFlags)
	{
		etFlags.require(ET::NODE_HEX);
		etFlags.set(ET::EDGE_HEX_ADJACENT_HEX);

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle)
	{
		etFlags.require(ET::NODE_UNIT);
		etFlags.set(ET::EDGE_UNIT_ACTS_BEFORE_UNIT);

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		etFlags.require(ET::NODE_UNIT);
		etFlags.set(ET::EDGE_UNIT_MELEE_DMG_UNIT);

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
		Flags<ET> & etFlags,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		etFlags.require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		etFlags.set(ET::EDGE_UNIT_SHOOT_DMG_UNIT);

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
		Flags<ET> & etFlags)
	{
		etFlags.require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);
		etFlags.set(ET::EDGE_UNIT_BLOCKS_UNIT);

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
		Flags<ET> & etFlags)
	{
		etFlags.require(ET::NODE_UNIT);
		etFlags.require(ET::NODE_HEX);
		etFlags.set(ET::EDGE_UNIT_OCCUPIES_HEX);

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

	int CalcActionId(
		AT actionType,
		const UnitPtr & actingUnit, // actor (N/A for WAIT)
		const UnitPtr & targetUnit, // attacked unit (N/A for MOVE/WAIT)
		const HexPtr & targetHex) // move destination (N/A for SHOOT/WAIT)
	{
		static_assert(EI(S15::HexAction::AMOVE_TR) == 0);
		static_assert(EI(S15::HexAction::AMOVE_R) == 1);
		static_assert(EI(S15::HexAction::AMOVE_BR) == 2);
		static_assert(EI(S15::HexAction::AMOVE_BL) == 3);
		static_assert(EI(S15::HexAction::AMOVE_L) == 4);
		static_assert(EI(S15::HexAction::AMOVE_TL) == 5);
		static_assert(EI(S15::HexAction::AMOVE_2TR) == 6);
		static_assert(EI(S15::HexAction::AMOVE_2R) == 7);
		static_assert(EI(S15::HexAction::AMOVE_2BR) == 8);
		static_assert(EI(S15::HexAction::AMOVE_2BL) == 9);
		static_assert(EI(S15::HexAction::AMOVE_2L) == 10);
		static_assert(EI(S15::HexAction::AMOVE_2TL) == 11);

		// Hack for mapping move target hex + attack target stack
		// to the legacy action space.

		// POV: LEFT active unit
		//
		// x     = (small left stack)      | x        = (wide left stack head)
		// [0-5] = (small right stack)     | [0-5A-F] = (small right stack)
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .
		//  . . . . . . 5 0 . . . . . . . .|. . . . . . F 5 0 . . . . .
		// . . . . . . 4 x 1 . . . . . . . | . . . . . E ~ x 1 . . . .
		//  . . . . . . 3 2 . . . . . . . .|. . . . . . D 3 2 . . . . .
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .

		// x     = (small left stack)      | x        = (wide left stack head)
		// [0-5] = (wide right stack head) | [0-5A-F] = (wide right stack head)
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .
		//  . . . . . 5 5 0 ~ . . . . . . .|. . . . . F F 5 0 ~ . . . .
		// . . . . . 4 - x 1 ~ . . . . . . | . . . . E ~ ~ x 1 ~ . . .
		//  . . . . . 3 3 2 ~ . . . . . . .|. . . . . D D 3 2 ~ . . . .
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .

		// POV: RIGHT active unit

		// x     = (small right stack)     | x        = (wide right stack head)
		// [0-5] = (small left stack)      | [0-5A-F] = (small left stack)
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .
		//  . . . . . . 5 0 . . . . . . . .|. . . . . . . 5 0 A . . . .
		// . . . . . . 4 x 1 . . . . . . . | . . . . . . 4 x ~ B . . .
		//  . . . . . . 3 2 . . . . . . . .|. . . . . . . 3 2 C . . . .
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .

		// x     = (small right stack)     | x        = (wide right stack head)
		// [0-5] = (wide left stack head)  | [0-5A-F] = (wide left stack head)
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .
		//  . . . . . ~ 5 0 0 . . . . . . .|. . . . . . ~ 5 0 A A . . .
		// . . . . . ~ 4 x - 1 . . . . . . | . . . . . ~ 4 x ~ ~ B . .
		//  . . . . . ~ 3 2 2 . . . . . . .|. . . . . . ~ 3 2 C C . . .
		// . . . . . . . . . . . . . . . . | . . . . . . . . . . . . .

		switch(actionType)
		{
		case S15::ActionType::WAIT:
			return 1;
		case S15::ActionType::AMOVE:
		{
			// Plan:
			// if (edir = mutualPosition(a_head, b_head); edir != EDIR::NONE)
			// else if 	(a.wide() && edir = mutualPosition(a_tail, b_head); edir != EDIR::NONE)
			// else if 	(b.wide() && edir = mutualPosition(a_head, b_tail); edir != EDIR::NONE)
			// else if 	(a.wide() && b.wide() && edir = mutualPosition(a_tail, b_tail); edir != EDIR::NONE)
			// else throw

			ASSERT(actingUnit != nullptr, "actingUnit is required for AMOVE");
			ASSERT(targetUnit != nullptr, "targetUnit is required for AMOVE");
			ASSERT(targetHex != nullptr, "targetHex is required for AMOVE");

			static const auto dirmapHead = std::map<BattleHex::EDir, HexAction>
			{
				{BattleHex::EDir::TOP_RIGHT, HexAction::AMOVE_TR},
				{BattleHex::EDir::RIGHT, HexAction::AMOVE_R},
				{BattleHex::EDir::BOTTOM_RIGHT, HexAction::AMOVE_BR},
				{BattleHex::EDir::BOTTOM_LEFT, HexAction::AMOVE_BL},
				{BattleHex::EDir::LEFT, HexAction::AMOVE_L},
				{BattleHex::EDir::TOP_LEFT, HexAction::AMOVE_TL}
			};

			static const auto dirmapTail = std::map<BattleHex::EDir, HexAction>
			{
				{BattleHex::EDir::TOP_RIGHT, HexAction::AMOVE_2TR},
				{BattleHex::EDir::RIGHT, HexAction::AMOVE_2R},
				{BattleHex::EDir::BOTTOM_RIGHT, HexAction::AMOVE_2BR},
				{BattleHex::EDir::BOTTOM_LEFT, HexAction::AMOVE_2BL},
				{BattleHex::EDir::LEFT, HexAction::AMOVE_2L},
				{BattleHex::EDir::TOP_LEFT, HexAction::AMOVE_2TL}
			};

			const auto & astack = actingUnit->cstack;
			const auto & bstack = targetUnit->cstack;

			int amove;

			const auto & a_head = targetHex->bhex;
			const auto & b_head = bstack.getPosition();
			const auto & a_tail = astack.occupiedHex(targetHex->bhex);
			const auto & b_tail = bstack.occupiedHex();

			const auto & a_head_adj = a_head.getNeighbouringTiles();
			const auto & a_tail_adj = a_tail.getNeighbouringTiles(); // OK if a_tail is invalid

			if (a_head_adj.contains(b_head))
				amove = EU(dirmapHead.at(BattleHex::mutualPosition(a_head, b_head)));
			else if (astack.doubleWide() && a_tail_adj.contains(b_head))
				amove = EU(dirmapTail.at(BattleHex::mutualPosition(a_tail, b_head)));
			else if (bstack.doubleWide() && a_head_adj.contains(b_tail))
				amove = EU(dirmapHead.at(BattleHex::mutualPosition(a_head, b_tail)));
			else if (astack.doubleWide() && bstack.doubleWide() && a_tail_adj.contains(b_tail))
				amove = EU(dirmapHead.at(BattleHex::mutualPosition(a_tail, b_tail)));
			else
				throw std::runtime_error("mutual position mapping failed");

			return 2 + (targetHex->id * EU(HexAction::_count)) + amove;
		}
		case S15::ActionType::MOVE:
		{
			ASSERT(actingUnit != nullptr, "actingUnit is required for AMOVE");
			ASSERT(targetHex != nullptr, "targetHex is required for AMOVE");
			return 2 + (targetHex->id * EU(HexAction::_count)) + EU(HexAction::MOVE);
		}
		case S15::ActionType::SHOOT:
		{
			ASSERT(actingUnit != nullptr, "actingUnit is required for AMOVE");
			ASSERT(targetUnit != nullptr, "targetUnit is required for AMOVE");
			// VCMI's BattleAction::makeShotAttack takes a target unit, not hex
			int hexid = N::Hex::CalcId(targetUnit->cstack.getPosition());
			return 2 + (hexid * EU(HexAction::_count)) + EU(HexAction::SHOOT);
		}
		default:
			throw std::runtime_error("Unexpected action type: " + std::to_string(EU(actionType)));
		}
	};

	void AddMoveActionNodes(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags)
	{
		etFlags.require(ET::NODE_UNIT);
		etFlags.require(ET::NODE_HEX);

		// XXX: these edges will be set only for MOVE actions, however
		// they are required the other 3 action types (AMOVE, SHOOT, WAIT)
		atFlags.set(AT::MOVE);
		etFlags.set(ET::NODE_ACTION);
		etFlags.set(ET::EDGE_ACTION_BY_UNIT);
		etFlags.set(ET::EDGE_ACTION_ENDS_AT_HEX);

		for(const auto & unit : G->getAll<N::Unit>())
		{
			const auto & stack = unit->cstack;
			const auto & reachability = G->getReachability(stack);
			for(const auto & hex : G->getAll<N::Hex>())
			{
				if(reachability.distances.at(hex->bhex.toInt()) > unit->cstack.getMovementRange())
				{
					auto stackhexes = std::vector<HexPtr>{};
					for(const auto & stackbhex : stack.getHexes())
						stackhexes.emplace_back(G->getByExtraIndex<N::Hex>(stackbhex.toInt()));

					int id = CalcActionId(AT::MOVE, unit, nullptr, hex);
					const auto action = std::make_shared<N::Action>(AT::MOVE, id, unit, stackhexes);

					G->add(action);
					G->add(std::make_shared<E::Action_By_Unit>(action, unit));

					bool isRear = false; // getHexes always returns primary hex first
					for(const auto & stackhex : stackhexes)
					{
						G->add(std::make_shared<E::Action_EndsAt_Hex>(action, stackhex, isRear));
						isRear = true;
					}
				}
			}
		}
	}

	void AddMoveActionEdges_Action_Blocks_Unit(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		throw std::runtime_error("not implemented");
	}

	void AddMoveActionEdges_Action_ExposesToMeleeFrom_Unit(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags)
	{
		etFlags.require(ET::NODE_ACTION);
		atFlags.requireExclusive(AT::MOVE);
		etFlags.require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		etFlags.require(ET::EDGE_UNIT_ACTS_BEFORE_UNIT);
		etFlags.require(ET::EDGE_ACTION_ENDS_AT_HEX);


		// See note in AddMoveActionNodes()
		etFlags.set(ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT);

		// Plan:
		// For each MOVE action, find units which:
		// 	1. are enemies to the actor (i.e. with "MeleeDmg" edge to it)
		//  2. will act after it (i.e. "ActsBefore" actor->them has times==1)
		//  3. can move such that they will end up occupying
		// 		at least of the hexes around the actor's new position
		//  4. the enemy move doesn't overlap with the actor's own move
		for (const auto & action : G->getAll<N::Action>())
		{
			assert(action->actionType == AT::MOVE);
			const auto & unit = action->by;
			const auto & stack = unit->cstack;
			const auto & stackhexes = stack.getHexes();

			for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_MeleeDmg_Unit>(unit))
			{
				if(G->getEdgeBySrcDst<E::Unit_ActsBefore_Unit>(unit, ounit)->times > 1)
					continue;

				const auto & adjbhexes = stack.getSurroundingHexes();
				for (const auto &oaction : G->getAllEdgesSrcByDst<E::Action_By_Unit>(ounit)) {
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
					bool candidate = false;
					bool overlap = false;
					for (const auto & ohex : oaction->endsAt) {
						candidate |= adjbhexes.contains(ohex->bhex);
						overlap |= stackhexes.contains(ohex->bhex);
					}

					if (candidate && !overlap) {
						G->add(std::make_shared<E::Action_ExposesToMeleeFrom_Unit>(action, ounit));
						break;
					}
				}
			}
		}
	}

	void AddMoveActionEdges_Action_ExposesToShootFrom_Unit(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags,
		const CPlayerBattleCallback & battle)
	{
		etFlags.require(ET::NODE_ACTION);
		atFlags.requireExclusive(AT::MOVE);
		etFlags.require(ET::NODE_ACTION);
		etFlags.require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);
		etFlags.require(ET::EDGE_UNIT_BLOCKS_UNIT);
		etFlags.require(ET::EDGE_ACTION_BLOCKS_UNIT);

		// See note in AddMoveActionNodes()
		etFlags.set(ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT);

		// Plan:
		// For each MOVE action, find units which:
		// 	1. are shooter enemies for the actor (i.e. with "RangedDmg" edge to it)
		// 	2. either:
		//		a. are currently unblocked and will remain unblocked after the move
		// 	    b. are currently blocked *only* by the actor, but will become
		// 			unblocked after the move

		for (const auto & action : G->getAll<N::Action>())
		{
			assert(action->actionType == AT::MOVE);
			const auto & unit = action->by;
			const auto & stack = unit->cstack;

			for (const auto & ounit : G->getAllEdgesSrcByDst<E::Unit_ShootDmg_Unit>(unit))
			{
				const auto & blockers = G->getAllEdgesSrcByDst<E::Unit_Blocks_Unit>(ounit);

				auto numBlockers = std::ranges::distance(blockers);
				if (numBlockers > 1)
					continue; // no threat (already blocked by someone else)

				auto actorIsBlocker = std::ranges::find(blockers, unit) != blockers.end();
				if (numBlockers == 1 && !actorIsBlocker)
					continue; // no threat (already blocked by someone else)

				bool willBlock = G->getEdgeBySrcDst<E::Action_Blocks_Unit>(action, ounit, false) != nullptr;
				if (willBlock)
					continue; // no threat (will become blocked after the move)

				const auto & ostack = ounit->cstack;
				float mult = 1;

				// XXX: are these VCMI functions efficient?
				if(battle.battleHasDistancePenalty(&ostack, ostack.getPosition(), stack.getPosition()))
					mult *= 0.5;
				if(battle.battleHasWallPenalty(&ostack, ostack.getPosition(), stack.getPosition()))
					mult *= 0.5;

				G->add(std::make_shared<E::Action_ExposesToShootFrom_Unit>(action, ounit, mult));
			}
		}
	}

	void AddMoveActionEdges_Action_EnablesMeleeAt_Unit(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
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
		 * TODO
		 * 1. Compile v15 without this edge
		 * 2. Then add it, calculating all reachabilities
		 * 		(compare traditional makeBFS with optimized versions)
		 */

		throw std::runtime_error("not implemented");
	}

	void AddMoveActionEdges_Action_EnablesShootAt_Unit(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		etFlags.require(ET::NODE_ACTION);
		atFlags.requireExclusive(AT::MOVE);
		etFlags.require(ET::EDGE_UNIT_OCCUPIES_HEX);
		etFlags.require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		etFlags.require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);

		// See note in AddMoveActionNodes()
		etFlags.set(ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT);

		// Plan:
		// For each MOVE action:
		//  1. Check if the actor can shoot
		//  1. Check if the actor will be blocked after the move
		// 	2. Find units with "ShootDmg" edge from the actor
		//
		for (const auto & action : G->getAll<N::Action>())
		{
			const auto & unit = action->by;
			const auto & stack = unit->cstack;

			if (!stack.canShoot())
				break;

			const auto & hex = action->endsAt.at(0);

			if (!stack.canShootBlocked())
			{
				// If blocked after the move => return early
				for (const auto & adjbhex : unit->cstack.getSurroundingHexes(hex->bhex))
				{
					const auto & adjhex = G->getByExtraIndex<N::Hex>(adjbhex.toInt());
					const auto & ounit = G->getOneEdgeSrcByDst<E::Unit_Occupies_Hex>(adjhex, false);
					if (ounit && G->getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(ounit, unit, false))
						return; // we will be blocked
				}
			}

			for (const auto & ounit : G->getAllEdgesDstBySrc<E::Unit_ShootDmg_Unit>(unit))
				G->add(std::make_shared<E::Action_EnablesShootAt_Unit>(action, unit));
		}
	}

	template <typename T>
	void CloneActionGenericEdges(
		std::shared_ptr<Graph::Graph> & G,
		const ActionPtr & src,
		const std::shared_ptr<N::Action> & dst)
	{
    	for (const std::shared_ptr<const T> & e : G->getAllEdgesBySrc<T>(src))
    		G->add(std::make_shared<T>(dst, e->dstNode));
	}

	void CloneActionEdges(
		std::shared_ptr<Graph::Graph> & G,
		const ActionPtr & src,
		const std::shared_ptr<N::Action> & dst)
	{
		for(int i = 0; i < EU(ET::_count); ++i)
		{
			switch(ET(i))
			{
		        case ET::EDGE_ACTION_BY_UNIT:
		        	CloneActionGenericEdges<E::Action_By_Unit>(G, src, dst);
		        	break;
		        case ET::EDGE_ACTION_BLOCKS_UNIT:
		        	CloneActionGenericEdges<E::Action_Blocks_Unit>(G, src, dst);
		        	break;
		        case ET::EDGE_ACTION_ENDS_AT_HEX:
		        	for (const auto & e : G->getAllEdgesBySrc<E::Action_EndsAt_Hex>(src))
		        		G->add(std::make_shared<E::Action_EndsAt_Hex>(dst, e->dstNode, e->isRear));
		        	break;
		        case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
		        	CloneActionGenericEdges<E::Action_ExposesToMeleeFrom_Unit>(G, src, dst);
		        	break;
		        case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
		        	for (const auto & e : G->getAllEdgesBySrc<E::Action_ExposesToShootFrom_Unit>(src))
		        		G->add(std::make_shared<E::Action_ExposesToShootFrom_Unit>(dst, e->dstNode, e->mult));
		        	break;
		        case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
		        	CloneActionGenericEdges<E::Action_EnablesMeleeAt_Unit>(G, src, dst);
		        	break;
		        case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
		        	CloneActionGenericEdges<E::Action_EnablesShootAt_Unit>(G, src, dst);
		        	break;
#ifdef MMAI_ENABLE_EDGE_ACTION_ENABLES_AT_HEX
		        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_HEX:
		        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_HEX:
#endif
	        	// Nothing to add for those
		        case ET::NODE_GLOBAL:
		        case ET::NODE_PLAYER:
		        case ET::NODE_UNIT:
		        case ET::NODE_HEX:
		        case ET::NODE_ACTION:
		        case ET::NODE_ACTACTION:
		        case ET::EDGE_HEX_ADJACENT_HEX:
		        case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
		        case ET::EDGE_UNIT_MELEE_DMG_UNIT:
		        case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
		        case ET::EDGE_UNIT_BLOCKS_UNIT:
		        case ET::EDGE_UNIT_OCCUPIES_HEX:
		        case ET::EDGE_ACTION_MELEES_UNIT:
		        case ET::EDGE_ACTION_SHOOTS_UNIT:
		        case ET::EDGE_ACTACTION_BY_UNIT:
		        case ET::EDGE_ACTACTION_BLOCKS_UNIT:
		        case ET::EDGE_ACTACTION_ENDS_AT_HEX:
		        case ET::EDGE_ACTACTION_EXPOSES_TO_MELEE_FROM_UNIT:
		        case ET::EDGE_ACTACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
		        case ET::EDGE_ACTACTION_MELEES_UNIT:
		        case ET::EDGE_ACTACTION_SHOOTS_UNIT:
		        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_UNIT:
		        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_UNIT:
		        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_HEX:
		        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_HEX:
		        	break;
		        default:
		        	throw std::runtime_error("Unexpected edge type: " + std::to_string(i));
			}
		}
	};

	void AddAmoveAction(
		std::shared_ptr<Graph::Graph> & G,
		const ActionPtr & move)
	{
		const auto & unit = move->by;
		const auto & hex = move->endsAt.at(0);

		// XXX: Special case when walking into moat/quicksand (see _notes/moats.txt)
		bool willMoveIntoMoat = (
			hex->bhex != unit->cstack.getPosition() &&
			std::ranges::any_of(move->endsAt, [](const HexPtr & hex) {
			    return hex->statemask.test(EU(S15::HexState::STOPPING));
			})
		);

		if (willMoveIntoMoat)
			return;

		// A wide adjacent unit may have already been inserted
		// The edge is action-melees-unit (and not action-melees-hex)
		// => don't add it twice
		auto ounits = std::unordered_set<UnitPtr>{};

		for (const auto & adjbhex : unit->cstack.getSurroundingHexes(hex->bhex))
		{
			const auto & adjhex = G->getByExtraIndex<N::Hex>(adjbhex.toInt());
			const auto & ounit = G->getOneEdgeSrcByDst<E::Unit_Occupies_Hex>(adjhex);

			if (!ounit)
				continue;

			if (!G->getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(unit, ounit, false))
				continue;

			auto [it, inserted] = ounits.emplace(ounit);
			if (!inserted)
				continue;

			const auto & unit = move->by;
			const auto & hex = move->endsAt.at(0);
			assert(CStack::isMeleeAttackPossible(&unit->cstack, &ounit->cstack, hex->bhex));

			auto id = CalcActionId(AT::MOVE, unit, ounit, hex);
			auto amove = std::make_shared<N::Action>(AT::AMOVE, id, unit, move->endsAt);
			G->add(amove);
			G->add(std::make_shared<E::Action_Melees_Unit>(amove, ounit));
			CloneActionEdges(G, move, amove);
		}
	}

	void AddShootAction(
		std::shared_ptr<Graph::Graph> & G,
		const ActionPtr & defend)
	{
		for (const auto & ounit : G->getAllEdgesDstBySrc<E::Action_EnablesShootAt_Unit>(defend))
		{
			auto id = CalcActionId(AT::SHOOT, defend->by, ounit, nullptr);
			auto shoot = std::make_shared<N::Action>(AT::SHOOT, id, defend->by, defend->endsAt);
			G->add(shoot);
			G->add(std::make_shared<E::Action_Shoots_Unit>(shoot, ounit));
			CloneActionEdges(G, defend, shoot);
		}
	}

	void AddWaitAction(
		std::shared_ptr<Graph::Graph> & G,
		const ActionPtr & defend)
	{
		if (defend->by->cstack.waitedThisTurn)
			return;
		auto id = CalcActionId(AT::WAIT, nullptr, nullptr, nullptr);
		auto wait = std::make_shared<N::Action>(AT::WAIT, id, defend->by, defend->endsAt);
		G->add(wait);
		CloneActionEdges(G, defend, wait);
	}

	void AddOtherActions(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags)
	{
		// All MOVE actions with all their edges must be available here
		// except for MELEES and SHOOTS edges which are for AMOVE only
		etFlags.require(ET::NODE_ACTION);
		atFlags.requireExclusive(AT::MOVE);
        etFlags.require(ET::EDGE_ACTION_BY_UNIT);
        etFlags.require(ET::EDGE_ACTION_BLOCKS_UNIT);
        etFlags.require(ET::EDGE_ACTION_ENDS_AT_HEX);
        etFlags.require(ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT);
        etFlags.require(ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT);
        etFlags.require(ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT);
        etFlags.require(ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT);

        etFlags.set(ET::EDGE_ACTION_MELEES_UNIT);
        etFlags.set(ET::EDGE_ACTION_SHOOTS_UNIT);
		atFlags.set(AT::AMOVE);
		atFlags.set(AT::SHOOT);
		atFlags.set(AT::WAIT);

		// These are needed for SHOOT actions
		auto defendmoves = std::unordered_map<UnitPtr, const ActionPtr>{};

		// Iterate from a vector as we will new nodes will be added to the index
		auto moveactions = std::vector<ActionPtr>{};
		moveactions.reserve(G->size<N::Action>());
		for (const auto & move : G->getAll<N::Action>())
		{
		    moveactions.push_back(move);
		    if(move->by->cstack.getPosition() == move->endsAt.at(0)->bhex)
		    	defendmoves.try_emplace(move->by, move);
		}

		for (const auto & move : moveactions)
		{
			AddAmoveAction(G, move);
		}

		for(const auto & unit : G->getAll<N::Unit>())
		{
			const auto & defendhex = G->getByExtraIndex<N::Hex>(unit->cstack.getPosition());
			const auto & defend = defendmoves.at(unit);
			AddShootAction(G, defend);
			AddWaitAction(G, defend);
		}
	}

	void AddActactions(
		std::shared_ptr<Graph::Graph> & G,
		Flags<ET> & etFlags,
		Flags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack
	)
	{
		if (!acstack)
			return;

		for (const auto & a : G->getAll<N::Action>())
		{
			const auto & unit = a->by;
			const auto & stack = unit->cstack;

			if (&stack != acstack)
				continue;

			const auto aa = std::make_shared<N::Actaction>(a->id);
			G->add(aa);

			#define GENERIC_ACTION_CLONE(tail) \
				for (const auto & e : G->getAllEdgesBySrc<E::Action_##tail>(a)) \
					G->add(std::make_shared<E::Actaction_##tail>(aa, e->dstNode))

			for(int i = 0; i < EU(ET::_count); ++i)
			{
				switch(ET(i))
				{
			        case ET::EDGE_ACTION_BY_UNIT:
						GENERIC_ACTION_CLONE(By_Unit);
						break;
			        case ET::EDGE_ACTION_BLOCKS_UNIT:
						GENERIC_ACTION_CLONE(Blocks_Unit);
						break;
			        case ET::EDGE_ACTION_ENDS_AT_HEX:
						for (const auto & e : G->getAllEdgesBySrc<E::Action_EndsAt_Hex>(a))
							G->add(std::make_shared<E::Actaction_EndsAt_Hex>(aa, e->dstNode, e->isRear));
						break;
			        case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
						GENERIC_ACTION_CLONE(ExposesToMeleeFrom_Unit);
						break;
			        case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
						for (const auto & e : G->getAllEdgesBySrc<E::Action_ExposesToShootFrom_Unit>(a))
							G->add(std::make_shared<E::Actaction_ExposesToShootFrom_Unit>(aa, e->dstNode, e->mult));
			        case ET::EDGE_ACTACTION_MELEES_UNIT:
						GENERIC_ACTION_CLONE(Melees_Unit);
			        case ET::EDGE_ACTACTION_SHOOTS_UNIT:
						GENERIC_ACTION_CLONE(Shoots_Unit);
			        case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
						GENERIC_ACTION_CLONE(EnablesMeleeAt_Unit);
			        case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
						GENERIC_ACTION_CLONE(EnablesShootAt_Unit);
#ifdef MMAI_ENABLE_EDGE_ACTION_ENABLES_AT_HEX
			        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_HEX:
			        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_HEX:
#endif
		        	// Nothing to add for those
			        case ET::NODE_GLOBAL:
			        case ET::NODE_PLAYER:
			        case ET::NODE_UNIT:
			        case ET::NODE_HEX:
			        case ET::NODE_ACTION:
			        case ET::NODE_ACTACTION:
			        case ET::EDGE_HEX_ADJACENT_HEX:
			        case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
			        case ET::EDGE_UNIT_MELEE_DMG_UNIT:
			        case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
			        case ET::EDGE_UNIT_BLOCKS_UNIT:
			        case ET::EDGE_UNIT_OCCUPIES_HEX:
			        case ET::EDGE_ACTION_MELEES_UNIT:
			        case ET::EDGE_ACTION_SHOOTS_UNIT:
			        case ET::EDGE_ACTACTION_BY_UNIT:
			        case ET::EDGE_ACTACTION_BLOCKS_UNIT:
			        case ET::EDGE_ACTACTION_ENDS_AT_HEX:
			        case ET::EDGE_ACTACTION_EXPOSES_TO_MELEE_FROM_UNIT:
			        case ET::EDGE_ACTACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
			        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_UNIT:
			        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_UNIT:
			        case ET::EDGE_ACTACTION_ENABLES_MELEE_AT_HEX:
			        case ET::EDGE_ACTACTION_ENABLES_SHOOT_AT_HEX:
			        	break;
			        default:
			        	throw std::runtime_error("Unexpected edge type: " + std::to_string(i));
				}
			}
		}
	}
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

void State::onActiveStack(
	const CStack * acstack,
	int round,
	S15::CombatResult result)
{
	logAi->debug("onActiveStack: round=%d, result=%d", round, EI(result));
	auto G = std::make_shared<Graph::Graph>(battle, nullptr);

	const auto stats = CalcGlobalStats(battle);
	const auto logdata = ProcessAttackLogs(attackLogs, sstats);
	auto etFlags = Flags<S15::Graph::ElementType>();

	G->buildAccessibilityCache();

	AddGlobalNode(G, etFlags, battle, acstack, result, round, startStats, stats);
	AddPlayerNodes(G, etFlags, startStats, lastStats, stats, logdata);
	AddUnitNodes(G, etFlags, battle, acstack, startStats, lastStats, stats, sstats);
	AddHexNodes(G, etFlags, battle, acstack);

	G->buildReachabilityCache(); // requires etFlags Units

	AddEdges_Hex_Adjacent_Hex(G, etFlags);
	AddEdges_Unit_ActsBefore_Unit(G, etFlags, battle);
	AddEdges_Unit_MeleeDmg_Unit(G, etFlags, battle, stats);
	AddEdges_Unit_ShootDmg_Unit(G, etFlags, battle, stats);
	AddEdges_Unit_Blocks_Unit(G, etFlags);
	AddEdges_Unit_Occupies_Hex(G, etFlags);

	auto atFlags = Flags<S15::ActionType>();
	AddMoveActionNodes(G, etFlags, atFlags); // + edges: ActionByUnit, ActionEndsAtHex
	// AddMoveActionEdges_Action_By_Unit() // already added
	AddMoveActionEdges_Action_Blocks_Unit(G, etFlags, atFlags, battle, acstack);
	// AddMoveActionEdges_Action_EndsAt_Hex() // already added
	AddMoveActionEdges_Action_ExposesToMeleeFrom_Unit(G, etFlags, atFlags);
	AddMoveActionEdges_Action_ExposesToShootFrom_Unit(G, etFlags, atFlags, battle);
    AddMoveActionEdges_Action_EnablesMeleeAt_Unit(G, etFlags, atFlags, battle, acstack);
    AddMoveActionEdges_Action_EnablesShootAt_Unit(G, etFlags, atFlags, battle, acstack);

    AddOtherActions(G, etFlags, atFlags);
    AddActactions(G, etFlags, atFlags, battle, acstack);

	// ...
	// AddActions(G, etFlags, battle, acstack); // add last; also adds Actaction nodes

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
};
