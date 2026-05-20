/*
 * state.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h" // IWYU pragma: keep

#include "BAI/v15/graph/edges/action_ends_at_hex.h"
#include "BAI/v15/graph/edges/action_melees_unit.h"
#include "BAI/v15/graph/edges/hex_adjacent_hex.h"
#include "BAI/v15/graph/edges/unit_acts_before_unit.h"
#include "BAI/v15/graph/edges/unit_melee_dmg_unit.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"
#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "BAI/v15/graph/nodes/player.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "BAI/v15/hexaction.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/DamageCalculator.h"
#include "battle/CUnitState.h"
#include "bonuses/BonusParameters.h" // IWYU pragma: keep (needed for bonus->parameters)
#include "entities/building/TownFortifications.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v15/state.h"
#include "common.h"
#include "schema/v15/types.h"
#include "spells/CSpellHandler.h"
#include "spells/ISpellMechanics.h"
#include "spells/ProxyCaster.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
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

	TowerFlags GetSiegeTowers(const CPlayerBattleCallback & battle) {
		TowerFlags res; // {upper, middle, lower}

		auto has = [&battle](EWallPart part) {
			auto ws = battle.battleGetWallState(part);
			return ws != EWallState::NONE && ws != EWallState::DESTROYED;
		};

		if (has(EWallPart::UPPER_TOWER))
			res.hasUpperTower = true;
		if (has(EWallPart::KEEP))
			res.hasMiddleTower = true;
		if (has(EWallPart::BOTTOM_TOWER))
			res.hasBottomTower = true;

		return res;
	}

	CorpseFlags GetSiegeCorpses(const CPlayerBattleCallback & battle)
	{
		CorpseFlags res; // {gate, bridge}

		if(battle.battleGetFortifications().wallsHealth == 0)
			return res;

		for(const auto & cstack : battle.battleGetAllStacks(false))
		{
			if(cstack->alive())
				continue;

			if(cstack->coversPos(BattleHex::GATE_INNER) || cstack->coversPos(BattleHex::GATE_OUTER))
				res.hasGateCorpse = true;
			if (cstack->coversPos(BattleHex::GATE_BRIDGE))
				res.hasBridgeCorpse = true;
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
			.leftValue=lv,
			.leftHp=lh,
			.rightValue=rv,
			.rightHp=rh,
			.totalValue=lv + rv,
			.totalHp=lh + rh
		};
	}

    // Stolen from BattleActionProcessor::handleDeathStare
    // Calculates number of kills
    double CalcDeathStare(
    	const CPlayerBattleCallback & battle,
    	const battle::CUnitState * attacker,
    	const battle::CUnitState * defender,
    	bool ranged)
    {
    	/*
         * Death stare:
         * - X=10% chance to kill per gorgon
         * - rolled separately for each gorgon in the stack
         * - kills capped to (N*X/100), where N=number of gorgons
         *
         * Accurate Shot (HotA seadogs):
         * - same mechanic as death stare, but ranged
         * - X=3% chance to kill for each seadog (X=2% with range penalty)
         *
         * Commander death stare:
         * - different mechanic: kills depend on level
         */

        auto subtype = BonusCustomSubtype::deathStareGorgon;

        if (ranged)
        {
            bool distancePenalty = battle.battleHasDistancePenalty(attacker, attacker->getPosition(), defender->getPosition());
            bool obstaclePenalty = battle.battleHasWallPenalty(attacker, attacker->getPosition(), defender->getPosition());

            if(distancePenalty)
                subtype = obstaclePenalty
                    ? BonusCustomSubtype::deathStareRangeObstaclePenalty
                    : BonusCustomSubtype::deathStareRangePenalty;
            else
                subtype = obstaclePenalty
                    ? BonusCustomSubtype::deathStareObstaclePenalty
                    : BonusCustomSubtype::deathStareNoRangePenalty;
        }

        // Non-commander death stare
        int n = attacker->getCount();
        int x = attacker->valOfBonuses(BonusType::DEATH_STARE, subtype);
        double kills = n * x / 100.0;

        // Commander death stare
        int x1 = attacker->valOfBonuses(BonusType::DEATH_STARE, BonusCustomSubtype::deathStareCommander);
        kills += static_cast<double>(x1 * attacker->creatureLevel()) / defender->creatureLevel();

        return kills;
    }

    struct CUnitStateWrapper {
	   	explicit CUnitStateWrapper(const CStack * cstack, const std::shared_ptr<battle::CUnitState> & cstate)
    	: cstack(cstack), cstate(cstate) {}

		// XXX: many methods such as unitType() or getAvailableHealth() throw
		// exceptions when called on a CUnitState => calculate health manually.
    	int calcAvailableHealth() const
    	{
			// excplicitly cast to int otherwise unsigned int arithmetic may cause UB
			int n = static_cast<int>(cstate->getCount());
			int hpOne = static_cast<int>(cstack->getMaxHealth());
			int hp1st = static_cast<int>(cstate->getFirstHPleft());
			return ((n - 1) * hpOne) + hp1st;
    	}

    	const CStack * cstack;
    	std::shared_ptr<battle::CUnitState> cstate;
    };

    struct UnitStates {
		CUnitStateWrapper a;
		CUnitStateWrapper b;
    };

    // Executes a single attack (without retaliation logic)
    // Applies damage to defender and to attacker (if fire shield)
    // Mutates the given states.
    void ApplyAttack(
    	UnitStates & states,
		const CPlayerBattleCallback & battle,
    	bool ranged)
    {
    	auto & A_state = states.a.cstate;
    	auto & B_state = states.b.cstate;

		auto A_bai = BattleAttackInfo(A_state.get(), B_state.get(), 0, ranged);
		auto estimation = std::make_shared<DamageEstimation>(DamageCalculator(battle, A_bai).calculateDmgRange());
	    auto A_dmg_min = static_cast<int>(estimation->damage.min);
	    auto A_dmg_max = static_cast<int>(estimation->damage.max);
	    auto A_dmg_mean = static_cast<int64_t>(0.5 * (A_dmg_min + A_dmg_max));

    	int B_qty_old = B_state->getCount();
		// CUnitState->damage() expects a *mutable* ref and can set it to 0 (?!?)
		{
			int64_t dmg = A_dmg_mean;
			B_state->damage(dmg);
		}
    	auto A_kills_mean = B_qty_old - B_state->getCount();

		// 1. Handle LIFE_DRAIN and SOUL_STEAL
		// Stolen from BattleActionProcessor::applyBattleEffects
		bool B_isLiving = B_state->isLiving();
		if(B_isLiving) {
			if(A_state->hasBonusOfType(BonusType::LIFE_DRAIN) && states.a.cstack->getTotalHealth() != states.a.calcAvailableHealth())
			{
				int64_t toHeal = A_dmg_mean * A_state->valOfBonuses(BonusType::LIFE_DRAIN) / 100;
				A_state->heal(toHeal, EHealLevel::RESURRECT, EHealPower::PERMANENT);
			}

			if(A_state->hasBonusOfType(BonusType::SOUL_STEAL))
			{
				//we can have two bonuses - one with subtype 0 and another with subtype 1
				//try to use permanent first, use only one of two
				for(const auto & subtype : { BonusCustomSubtype::soulStealBattle, BonusCustomSubtype::soulStealPermanent})
				{
					if(A_state->hasBonusOfType(BonusType::SOUL_STEAL, subtype))
					{
						int64_t toHeal = static_cast<int64_t>(A_kills_mean) * A_state->valOfBonuses(BonusType::SOUL_STEAL, subtype) * A_state->getMaxHealth();
						bool permanent = subtype == BonusCustomSubtype::soulStealPermanent;
						A_state->heal(toHeal, EHealLevel::OVERHEAL, (permanent ? EHealPower::PERMANENT : EHealPower::ONE_BATTLE));
						break;
					}
				}
			}
		}

		// 2. Handle FIRE_SHIELD (triggers even if B is not alive)
		// Stolen from BattleActionProcessor::applyBattleEffects
		if(!ranged &&
			!B_state->isClone() &&
			B_state->hasBonusOfType(BonusType::FIRE_SHIELD) &&
			!A_state->hasBonusOfType(BonusType::SPELL_SCHOOL_IMMUNITY, BonusSubtypeID(SpellSchool::FIRE)) &&
			!A_state->hasBonusOfType(BonusType::NEGATIVE_EFFECTS_IMMUNITY, BonusSubtypeID(SpellSchool::FIRE)) &&
			A_state->valOfBonuses(BonusType::SPELL_DAMAGE_REDUCTION, BonusSubtypeID(SpellSchool::FIRE)) < 100 &&
			!B_state->isInvincible())
		{
			auto dmg = (std::min(static_cast<int64_t>(states.b.calcAvailableHealth()), A_dmg_mean) * B_state->valOfBonuses(BonusType::FIRE_SHIELD)) / 100;
			A_state->damage(dmg);
		}

		// 3. Handle DEATH_STARE (must come last; uses attacker qty left after fire shield)
		if (B_state->alive() && B_isLiving && A_state->hasBonusOfType(BonusType::DEATH_STARE))
		{
			int staredeaths = static_cast<int>(std::round(CalcDeathStare(battle, A_state.get(), B_state.get(), ranged)));

			while (staredeaths > 0 && B_state->alive())
			{
				/*
				 * VCMI's death stare has a bug:
				 * The top "HP Left" of the remaining defender stack is not
				 * reset to full HP after applying the effect
				 * https://discord.com/channels/298106089885401090/1147259775420207256/1506701782011613356
				 * Once that bug is fixed, change the calculation here to use:
				 * int64_t dmg = B_state->getFirstHPleft();
				 */
				int64_t dmg = B_state->getMaxHealth();
				B_state->damage(dmg);
				--staredeaths;
			}
		}
    }

    /*
     * VCMI's damage estimation helper does not take into account stuff such as:
	 * 	- Base mechanics:
	 * 	 	* HAS_ADDITIONAL_ATTACK // tested
	 * 	 	* DEATH_STARE 			// tested
	 * 	 	* FIRE_SHIELD 			// tested (incl. attacker dying from it)
	 * 	 	* LIFE_DRAIN 			// tested
	 *	- Mod mechanics:
	 * 		* RANGED_RATALIATION 	// not tested
	 * 		* FIRST_STRIKE 			// not tested
	 * 		* SOUL_STEAL 			// not tested
	 * 		* FEROCITY 				// not tested
	 *
	 * This is an attempt to reimplement it here.
	 *
	 * TODO: gather statistical data for simulated<>actual exchange
	 *       to compare.
	 *
	 */
	UnitStates SimulateAttackAction(
		const CPlayerBattleCallback & battle,
		const CStack & attacker,
		const CStack & defender,
		bool isRangedAttack,
		bool isDefenderBlocked)
	{
		// Stolen from BattleActionProcessor::doShootAction
		auto checkRangedRetal = [&attacker, isRangedAttack, isDefenderBlocked](bool canMeleeRetal)
		{
			return (canMeleeRetal
				&& isRangedAttack
				&& !isDefenderBlocked
				&& !attacker.hasBonusOfType(BonusType::BLOCKS_RANGED_RETALIATION));
		};

		// Stolen from BattleActionProcessor::doAttackAction
		// but using different getBonus functions which use caching strs
		auto checkFirstStrike = [&defender, isRangedAttack](bool canMeleeRetal, bool canRangedRetal)
		{
			if (defender.isInvincible())
				return false;

			if ((isRangedAttack && !canRangedRetal) || (!isRangedAttack && !canMeleeRetal))
				return false;

			static const auto selRanged = Selector::typeSubtype(BonusType::FIRST_STRIKE, BonusCustomSubtype::damageTypeAll).Or(Selector::typeSubtype(BonusType::FIRST_STRIKE, BonusCustomSubtype::damageTypeRanged));
			static const auto selMelee = Selector::typeSubtype(BonusType::FIRST_STRIKE, BonusCustomSubtype::damageTypeAll).Or(Selector::typeSubtype(BonusType::FIRST_STRIKE, BonusCustomSubtype::damageTypeMelee));
			static const auto strRanged = std::string("firstStrikeSelectorRanged");
			static const auto strMelee = std::string("firstStrikeSelectorMelee");

			return isRangedAttack
				? defender.hasBonus(selRanged, strRanged)
				: defender.hasBonus(selMelee, strMelee);
		};

		// Stolen from BattleActionProcessor::doAttackAction
		auto getAdditionalAttacks = [&attacker, isRangedAttack]
		{
			int totalAttacks = attacker.getTotalAttacks(isRangedAttack);
			if(const auto * attackingHero = attacker.getMyHero())
				totalAttacks += attackingHero->valOfBonuses(BonusType::HERO_GRANTS_ATTACKS, BonusSubtypeID(attacker.creatureId()));

			return totalAttacks - 1;
		};

		// Stolen from BattleActionProcessor::doAttackAction
		// but using different getBonus functions which use caching strs
		auto getFerocityAttacks = [&attacker](int kills)
		{
			const auto bonuses = attacker.getBonusesOfType(BonusType::FEROCITY);
			const auto bonus = bonuses->getFirst(Selector::all);
			if (!bonus)
				return 0;

			int killThreshold = bonus->parameters ? bonus->parameters->toNumber() : 1;
			return kills >= killThreshold ? bonuses->totalValue(0) : 0;
		};

		bool canMeleeRetal = defender.ableToRetaliate();
		bool canRangedRetal = checkRangedRetal(canMeleeRetal);
		bool switchNext = false;  // cache var to prevent unneeded ranged retal checks
		int ferocityCheckAfter = 0;  // when to check for ferocity (depends on first strike)
		auto positions = std::vector<int>{}; //  0=keep, 1=switch

		if (checkFirstStrike(canMeleeRetal, canRangedRetal))
		{
			positions.push_back(1); // switch: initial strike is by defender
			positions.push_back(1); // switch: attacker strikes (this is not a retaliation)
			ferocityCheckAfter = 1; // initial attacker strike is actually 2nd
		}
		else
		{
			positions.push_back(0); // no switch: initial strike is by attacker
			if (!defender.isInvincible() && (canMeleeRetal || canRangedRetal))
			{
				positions.push_back(1); // switch: defender strikes (if able to retalate)
				switchNext = true;
			}
		}

		for (int i = 0; i < getAdditionalAttacks(); ++i)
		{
			positions.push_back(switchNext);
			switchNext = false; // further additional attacks keep the same position
		}

		const auto states0 = UnitStates{
			.a=CUnitStateWrapper(&attacker, attacker.acquireState()),
			.b=CUnitStateWrapper(&defender, defender.acquireState())
		};
		auto states = states0;


		for (int i = 0; i < positions.size(); ++i)
		{
			bool shouldSwitch = positions.at(i);
			auto prevstates = states;
			states = shouldSwitch
				? UnitStates{.a=states.b, .b=states.a}
				: UnitStates{.a=states.a, .b=states.b};

			// mutates states
			ApplyAttack(states, battle, isRangedAttack);

			if (!states.a.cstate->alive() || !states.b.cstate->alive())
				break;

			if (i == ferocityCheckAfter)
			{
				ASSERT(states.b.cstack->unitId() == defender.unitId(), "SimulateAttackAction: ferocity check: expected A=attacker B=defender");
				const auto prevb = shouldSwitch ? prevstates.a : prevstates.b;
				// ferocity check must always be when a=attacker, b=defender
				int ferocityAttacks = getFerocityAttacks(states.b.cstate->getCount() - prevb.cstate->getCount());
				for (int j = 0; j < ferocityAttacks; ++j)
					positions.push_back(0);
			}
		}

		return states.a.cstack->unitId() == states0.a.cstack->unitId()
			? states
			: UnitStates{.a=states.b, .b=states.a};
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
		const std::vector<AttackLog> & attackLogs)
	{
		auto res = AttackLogAggregateData{};

		for(const auto & al : attackLogs)
		{
		    // TODO: check out how death stare is handled. Maybe add it as dmg?
			if(al.attacker)
			{
				if(al.attacker->cstack.unitSide() == BattleSide::LEFT_SIDE)
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

			if(al.defender->cstack.unitSide() == BattleSide::LEFT_SIDE)
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

	std::unordered_map<std::pair<int, int>, int, PairHash> InitAdjMap()
	{
		auto res = std::unordered_map<std::pair<int, int>, int, PairHash>{};

		for(int id1 = 0; id1 < GameConstants::BFIELD_SIZE; id1++)
		{
			auto hex1 = BattleHex(static_cast<int16_t>(id1));
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
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const S15::CombatResult result,
		const int round,
		const State::GlobalStats & stats)
	{
		G.setFlag(ET::NODE_GLOBAL);

		G.add(std::make_shared<N::Global>(
			result,
			round,
			stats.totalValue,
			stats.totalHp,
			GetSiegeTowers(battle),
			GetSiegeCorpses(battle)
		));
	}

	void AddPlayerNodes(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & lastStats,
		const State::GlobalStats & stats,
		const AttackLogAggregateData & logdata)
	{
		G.setFlag(ET::NODE_PLAYER);

		static_assert(EU(BattleSide::LEFT_SIDE) == 0);
		static_assert(EU(BattleSide::RIGHT_SIDE) == 1);

		G.add(std::make_shared<N::Player>(
			BattleSide::LEFT_SIDE,
			BattleSide::LEFT_SIDE == battle.battleGetMySide(),
			lastStats.totalValue,
			lastStats.totalHp,
			stats.leftValue,
			stats.leftHp,
			logdata.ldd,
			logdata.ldr,
			logdata.lvk,
			logdata.lvl
		));

		G.add(std::make_shared<N::Player>(
			BattleSide::RIGHT_SIDE,
			BattleSide::RIGHT_SIDE == battle.battleGetMySide(),
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
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const CStack * acstack,
		const State::GlobalStats & stats)
	{
		G.setFlag(ET::NODE_UNIT);

		for(auto & cstack : battle.battleGetStacks())
		{
			bool isActive = cstack == acstack;
			bool isEnemy = cstack->unitSide() != battle.battleGetMySide();
			G.add(std::make_shared<N::Unit>(*cstack, isActive, isEnemy, stats.totalValue));
		}
	}

	void AddHexNodes(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		G.setFlag(ET::NODE_HEX);

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
				auto bh = BattleHex(static_cast<int16_t>(x + 1), static_cast<int16_t>(y));
				ASSERT(bh.isAvailable(), "invalid bhex");

				G.add(std::make_shared<N::Hex>(
					bh,
					G.getAccessibility().at(bh.toInt()),
					acstack ? acstack->unitSide() : BattleSide::LEFT_SIDE,
					hexobstacles.at(i),
					WallHP(battle, bh),
					isGateOpen
				));
			}
		}
	}

	void AddEdges_Global_Has_PlayerUnitHex(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle)
	{
		G.getFlags().require(ET::NODE_GLOBAL);
		G.getFlags().require(ET::NODE_PLAYER);
		G.getFlags().require(ET::NODE_UNIT);
		G.getFlags().require(ET::NODE_HEX);
		G.setFlag(ET::EDGE_GLOBAL_HAS_PLAYER);
		G.setFlag(ET::EDGE_GLOBAL_HAS_UNIT);
		G.setFlag(ET::EDGE_GLOBAL_HAS_HEX);

		const auto & global = G.getAll<N::Global>().at(0);

		for (const auto & player : G.getAll<N::Player>())
			G.add(std::make_shared<E::Global_Has_Player>(global, player));

		for (const auto & unit : G.getAll<N::Unit>())
			G.add(std::make_shared<E::Global_Has_Unit>(global, unit));

		for (const auto & hex : G.getAll<N::Hex>())
			G.add(std::make_shared<E::Global_Has_Hex>(global, hex));
	}

	void AddEdges_Player_Owns_Unit(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle)
	{
		G.getFlags().require(ET::NODE_PLAYER);
		G.getFlags().require(ET::NODE_UNIT);
		G.setFlag(ET::EDGE_PLAYER_OWNS_UNIT);

		for (const auto & unit : G.getAll<N::Unit>())
		{
			const auto & player = G.getByExtraIndex<N::Player>(unit->cstack.unitSide());
			G.add(std::make_shared<E::Player_Owns_Unit>(player, unit));
		}
	}

	void AddEdges_Hex_Adjacent_Hex(
		Graph::Graph & G)
	{
		G.getFlags().require(ET::NODE_HEX);
		G.setFlag(ET::EDGE_HEX_ADJACENT_HEX);

		static const auto adjmap = InitAdjMap();
		const auto & hexes = G.getAll<N::Hex>();

		for(const auto & src : hexes)
		{
			for(const auto & dst : hexes)
			{
				auto it = adjmap.find({src->bhex.toInt(), dst->bhex.toInt()});
				if(it == adjmap.end())
					continue;
				G.add(std::make_shared<E::Hex_Adjacent_Hex>(src, dst, it->second));
			}
		}
	}

	void AddEdges_Unit_ActsBefore_Unit(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle)
	{
		G.getFlags().require(ET::NODE_UNIT);
		G.setFlag(ET::EDGE_UNIT_ACTS_BEFORE_UNIT);

		const auto matrix = Q::BuildActsBeforeMatrix(battle);
		const auto & nodes = G.getAll<N::Unit>();

		ASSERT(matrix.unique_count <= nodes.size(), "unique_count exceeds size of graph nodes");

		for(int i = 0; i < matrix.unique_count; ++i)
		{
			auto unitId = matrix.unique_units.at(i);
			// XXX: assuming unit ID is the same as CStack ID. This must be OK
			// since as of 2026, battle::Unit is just a superclass of CStack.
			const auto & unit = G.getByExtraIndex<N::Unit>(unitId);
			ASSERT(unit != nullptr, "unit not found: " + std::to_string(unitId));

			for(int j = 0; j < matrix.unique_count; ++j)
			{
				auto times = matrix.count[i][j];
				if(times == 0)
					continue;

				auto otherId = matrix.unique_units.at(j);
				const auto & other = G.getByExtraIndex<N::Unit>(otherId);
				ASSERT(other != nullptr, "unit not found: " + std::to_string(otherId));
				G.add(std::make_shared<E::Unit_ActsBefore_Unit>(unit, other, times));
			}
		}
	}

	void AddEdges_Unit_MeleeDmg_Unit(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		G.getFlags().require(ET::NODE_UNIT);
		G.setFlag(ET::EDGE_UNIT_MELEE_DMG_UNIT);

		auto pairs = std::unordered_set<std::pair<int, int>, PairHash>{};
		const auto & units = G.getAll<N::Unit>();

		for(const auto & unit : units)
		{
			const auto & stack = unit->cstack;
			bool isBerserk = checkBerserk(stack);

			for(const auto & other : units)
			{
				const auto & ostack = other->cstack;

				if(unit == other || ostack.isInvincible())
					continue;

				const auto pair = std::pair<int, int>{stack.unitId(), ostack.unitId()};
				auto [_, inserted] = pairs.emplace(pair);

				if(!inserted) // key already existed
					continue;

				if(ostack.unitSide() == stack.unitSide() && !isBerserk && !checkBerserk(ostack))
					continue;

				bool isOtherBlocked = G.getOneEdgeByDst<E::Unit_Blocks_Unit>(other, false) != nullptr;
				const auto states = SimulateAttackAction(battle, stack, ostack, false, isOtherBlocked);
				const auto & state = states.a;
				const auto & ostate = states.b;

				ASSERT(state.cstack->unitId() == stack.unitId() && ostate.cstack->unitId() == ostack.unitId(), "SimulateAttackAction: fatal error");

				auto hpdiff_attacker = state.calcAvailableHealth() - stack.getAvailableHealth();
				auto hpdiff_defender = ostate.calcAvailableHealth() - ostack.getAvailableHealth();
				auto qtydiff_attacker = state.cstate->getCount() - stack.getCount();
				auto qtydiff_defender = ostate.cstate->getCount() - ostack.getCount();
				auto vdiff_attacker = unit->valueOne * qtydiff_attacker;
				auto vdiff_defender = other->valueOne * qtydiff_defender;

				G.add(std::make_shared<E::Unit_MeleeDmg_Unit>(
					unit,
					other,
					vdiff_attacker,
					vdiff_defender,
					hpdiff_attacker,
					hpdiff_defender,
					stats.totalValue,
					stats.totalHp
				));
			}
		}
	}

	void AddEdges_Unit_ShootDmg_Unit(
		Graph::Graph & G,
		const CPlayerBattleCallback & battle,
		const State::GlobalStats & stats)
	{
		G.getFlags().require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		G.setFlag(ET::EDGE_UNIT_SHOOT_DMG_UNIT);

		// RANGED_DMG edges use a subset of the MELEE_DMG edge nodes
		// (all ranged units can also melee)
		for(const auto & edge : G.getAll<E::Unit_MeleeDmg_Unit>())
		{
			const auto & unit = edge->srcNode;
			const auto & stack = unit->cstack;
			const auto & other = edge->dstNode;
			const auto & ostack = other->cstack;

			if(!stack.canShoot() || ostack.isInvincible())
				continue;

			bool isOtherBlocked = G.getOneEdgeByDst<E::Unit_Blocks_Unit>(other, false) != nullptr;
			const auto states = SimulateAttackAction(battle, stack, ostack, false, isOtherBlocked);
			const auto & state = states.a;
			const auto & ostate = states.b;

			ASSERT(states.a.cstack->unitId() == stack.unitId() && states.b.cstack->unitId() == ostack.unitId(), "SimulateAttackAction: fatal error");

			auto hpdiff_attacker = state.calcAvailableHealth() - stack.getAvailableHealth();
			auto hpdiff_defender = ostate.calcAvailableHealth() - ostack.getAvailableHealth();
			auto qtydiff_attacker = state.cstate->getCount() - stack.getCount();
			auto qtydiff_defender = ostate.cstate->getCount() - ostack.getCount();
			auto vdiff_attacker = unit->valueOne * qtydiff_attacker;
			auto vdiff_defender = other->valueOne * qtydiff_defender;

			G.add(std::make_shared<E::Unit_ShootDmg_Unit>(
				unit,
				other,
				vdiff_attacker,
				vdiff_defender,
				hpdiff_attacker,
				hpdiff_defender,
				stats.totalValue,
				stats.totalHp
			));
		}
	}

	void AddEdges_Unit_Blocks_Unit(Graph::Graph & G)
	{
		G.getFlags().require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);
		G.setFlag(ET::EDGE_UNIT_BLOCKS_UNIT);

		// BLOCKS edges use a subset of the RANGED_DMG edge nodes
		// (all blocked units must be ranged units)
		for(const auto & edge : G.getAll<E::Unit_ShootDmg_Unit>())
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
					G.add(std::make_shared<E::Unit_Blocks_Unit>(unit, other));
					break;
				}
			}
		}
	}

	void AddEdges_Unit_Occupies_Hex(Graph::Graph & G)
	{
		G.getFlags().require(ET::NODE_UNIT);
		G.getFlags().require(ET::NODE_HEX);
		G.setFlag(ET::EDGE_UNIT_OCCUPIES_HEX);

		for(const auto & unit : G.getAll<N::Unit>())
		{
			const auto & cstack = unit->cstack;
			for(const auto & bhex : cstack.getHexes())
			{
				if(!bhex.isAvailable())
					continue;
				const auto & hex = G.getByExtraIndex<N::Hex>(bhex.toInt());
				ASSERT(hex != nullptr, "hex not found: " + std::to_string(bhex.toInt()));
				G.add(std::make_shared<E::Unit_Occupies_Hex>(unit, hex));
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

			static const auto dirmapHead = std::map<BattleHex::EDir, S15::HexAction>
			{
				{BattleHex::EDir::TOP_RIGHT, S15::HexAction::AMOVE_TR},
				{BattleHex::EDir::RIGHT, S15::HexAction::AMOVE_R},
				{BattleHex::EDir::BOTTOM_RIGHT, S15::HexAction::AMOVE_BR},
				{BattleHex::EDir::BOTTOM_LEFT, S15::HexAction::AMOVE_BL},
				{BattleHex::EDir::LEFT, S15::HexAction::AMOVE_L},
				{BattleHex::EDir::TOP_LEFT, S15::HexAction::AMOVE_TL}
			};

			static const auto dirmapTail = std::map<BattleHex::EDir, S15::HexAction>
			{
				{BattleHex::EDir::TOP_RIGHT, S15::HexAction::AMOVE_2TR},
				{BattleHex::EDir::RIGHT, S15::HexAction::AMOVE_2R},
				{BattleHex::EDir::BOTTOM_RIGHT, S15::HexAction::AMOVE_2BR},
				{BattleHex::EDir::BOTTOM_LEFT, S15::HexAction::AMOVE_2BL},
				{BattleHex::EDir::LEFT, S15::HexAction::AMOVE_2L},
				{BattleHex::EDir::TOP_LEFT, S15::HexAction::AMOVE_2TL}
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
				amove = EU(dirmapTail.at(BattleHex::mutualPosition(a_tail, b_tail)));
			else
				throw std::runtime_error("mutual position mapping failed");

			return 2 + (targetHex->id * EU(S15::HexAction::_count)) + amove;
		}
		case S15::ActionType::DEFEND:
		case S15::ActionType::MOVE:
		{
			ASSERT(actingUnit != nullptr, "actingUnit is required for AMOVE");
			ASSERT(targetHex != nullptr, "targetHex is required for AMOVE");
			return 2 + (targetHex->id * EU(S15::HexAction::_count)) + EU(S15::HexAction::MOVE);
		}
		case S15::ActionType::SHOOT:
		{
			ASSERT(actingUnit != nullptr, "actingUnit is required for AMOVE");
			ASSERT(targetUnit != nullptr, "targetUnit is required for AMOVE");
			// VCMI's BattleAction::makeShotAttack takes a target unit, not hex
			int hexid = N::Hex::CalcId(targetUnit->cstack.getPosition());
			return 2 + (hexid * EU(S15::HexAction::_count)) + EU(S15::HexAction::SHOOT);
		}
		default:
			throw std::runtime_error("Unexpected action type: " + std::to_string(EU(actionType)));
		}
	};

	void AddMoveAndDefendActions(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CStack * acstack)
	{
		G.getFlags().require(ET::NODE_UNIT);
		G.getFlags().require(ET::NODE_HEX);

		// XXX: these edges will be set only for MOVE actions, however
		// they are required the other 3 action types (AMOVE, SHOOT, WAIT)
		atFlags.set(AT::MOVE);
		atFlags.set(AT::DEFEND);
		G.setFlag(ET::NODE_ACTION);
		G.setFlag(ET::EDGE_ACTION_BY_UNIT);
		G.setFlag(ET::EDGE_ACTION_ENDS_AT_HEX);

		for(const auto & unit : G.getAll<N::Unit>())
		{
			const auto & stack = unit->cstack;
			const auto & reachability = G.getReachability(stack);
			bool isActive = &stack == acstack;

			for(const auto & hex : G.getAll<N::Hex>())
			{
				if(reachability.distances.at(hex->bhex.toInt()) > unit->cstack.getMovementRange())
					continue;

				auto stackhexes = std::vector<HexPtr>{};
				for(const auto & stackbhex : stack.getHexes(hex->bhex))
					stackhexes.emplace_back(G.getByExtraIndex<N::Hex>(stackbhex.toInt()));

				auto at = hex->bhex == stack.getPosition()
					? AT::DEFEND
					: AT::MOVE;

				int id = CalcActionId(at, unit, nullptr, hex);

				const auto action = std::make_shared<N::Action>(at, id, unit, stackhexes, isActive);

				G.add(action);
				G.add(std::make_shared<E::Action_By_Unit>(action, unit));

				bool isRear = false; // getHexes always returns primary hex first
				for(const auto & stackhex : stackhexes)
				{
					G.add(std::make_shared<E::Action_EndsAt_Hex>(action, stackhex, isRear));
					isRear = true;
				}
			}
		}
	}

	void AddMoveActionEdges_Action_Blocks_Unit(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		G.getFlags().require(ET::NODE_ACTION);
		atFlags.requireExclusive({AT::DEFEND, AT::MOVE});
		G.getFlags().require(ET::NODE_HEX);
		G.getFlags().require(ET::EDGE_UNIT_OCCUPIES_HEX);
		G.getFlags().require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);

		G.setFlag(ET::EDGE_ACTION_BLOCKS_UNIT);

		for (const auto & action : G.getAll<N::Action>())
		{
			assert(action->actionType == AT::MOVE || action->actionType == AT::DEFEND);
			const auto & unit = action->by;
			const auto & stack = unit->cstack;
			const auto & hex = action->endsAt.at(0);

			// A wide adjacent unit may have already been inserted
			// The edge is action-blocks-unit (and not action-blocks-hex)
			// => don't add it twice
			auto adjunits = std::unordered_set<UnitPtr>{};

			for (const auto & adjbhex : stack.getSurroundingHexes(hex->bhex))
			{
				if (!adjbhex.isAvailable())
					continue;

				const auto & adjhex = G.getByExtraIndex<N::Hex>(adjbhex.toInt());
				const auto & adjunit = G.getOneEdgeSrcByDst<E::Unit_Occupies_Hex>(adjhex, false);

				if (!adjunit)
					continue;

				if (adjunits.contains(adjunit) || adjunit->cstack.canShootBlocked())
						continue;

				if (!G.getEdgeBySrcDst<E::Unit_ShootDmg_Unit>(adjunit, unit, false))
					continue;

				adjunits.emplace(adjunit);
				G.add(std::make_shared<E::Action_Blocks_Unit>(action, adjunit));
			}

		}
	}

	void AddMoveActionEdges_Action_ExposesToMeleeFrom_Unit(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags)
	{
		G.getFlags().require(ET::NODE_ACTION);
		atFlags.requireExclusive({AT::DEFEND, AT::MOVE});
		G.getFlags().require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		G.getFlags().require(ET::EDGE_UNIT_ACTS_BEFORE_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_ENDS_AT_HEX);


		// See note in AddMoveActions()
		G.setFlag(ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT);

		// Plan:
		// For each MOVE action, find units which:
		// 	1. are enemies to the actor (i.e. with "MeleeDmg" edge to it)
		//  2. will act after it (i.e. "ActsBefore" actor->them has times==1)
		//  3. can move such that they will end up occupying
		// 		at least of the hexes around the actor's new position
		//  4. the enemy move doesn't overlap with the actor's own move
		for (const auto & action : G.getAll<N::Action>())
		{
			assert(action->actionType == AT::MOVE || action->actionType == AT::DEFEND);
			const auto & unit = action->by;
			const auto & stack = unit->cstack;
			const auto & hex = action->endsAt.at(0);
			const auto & stackhexes = stack.getHexes(hex->bhex);

			for (const auto & ounit : G.getAllEdgesSrcByDst<E::Unit_MeleeDmg_Unit>(unit))
			{
				const auto & actsBefore = G.getEdgeBySrcDst<E::Unit_ActsBefore_Unit>(unit, ounit, false);
				if(actsBefore && actsBefore->times > 1)
					continue;

				const auto & adjbhexes = stack.getSurroundingHexes(hex->bhex);
				for (const auto &oaction : G.getAllEdgesSrcByDst<E::Action_By_Unit>(ounit)) {
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
						G.add(std::make_shared<E::Action_ExposesToMeleeFrom_Unit>(action, ounit));
						break;
					}
				}
			}
		}
	}

	// This is copied from CBattleInfoCallback::battleHasDistancePenalty
	// but it is changed to accept a hypothetical shooter position
	bool battleHasDistancePenalty_CUSTOM(const IBonusBearer * shooter, const BattleHex & shooterPosition, const BattleHexArray & targetHexes)
	{
		const std::string cachingStrNoDistancePenalty = "type_NO_DISTANCE_PENALTY";
		static const auto selectorNoDistancePenalty = Selector::type()(BonusType::NO_DISTANCE_PENALTY);

		if(shooter->hasBonus(selectorNoDistancePenalty, cachingStrNoDistancePenalty))
			return false;

		int range = GameConstants::BATTLE_SHOOTING_PENALTY_DISTANCE;
		auto bonus = shooter->getBonus(Selector::type()(BonusType::LIMITED_SHOOTING_RANGE));
		if(bonus != nullptr && bonus->parameters)
			range = bonus->parameters->toNumber();

		for(const auto & hex : targetHexes)
			if(BattleHex::getDistance(shooterPosition, hex) <= range)
				//If any hex of target creature is within range, there is no penalty
				return false;

		return true;
	}

	void AddMoveActionEdges_Action_ExposesToShootFrom_Unit(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle)
	{
		G.getFlags().require(ET::NODE_ACTION);
		atFlags.requireExclusive({AT::DEFEND, AT::MOVE});
		G.getFlags().require(ET::NODE_ACTION);
		G.getFlags().require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);
		G.getFlags().require(ET::EDGE_UNIT_BLOCKS_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_BLOCKS_UNIT);

		// See note in AddMoveActions()
		G.setFlag(ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT);

		// Plan:
		// For each MOVE action, find units which:
		// 	1. are shooter enemies for the actor (i.e. with "RangedDmg" edge to it)
		// 	2. either:
		//		a. are currently unblocked and will remain unblocked after the move
		// 	    b. are currently blocked *only* by the actor, but will become
		// 			unblocked after the move

		for (const auto & action : G.getAll<N::Action>())
		{
			assert(action->actionType == AT::MOVE || action->actionType == AT::DEFEND);
			const auto & unit = action->by;
			const auto & stack = unit->cstack;
			const auto & hex = action->endsAt.at(0);
			const auto & bhex = hex->bhex;

			for (const auto & ounit : G.getAllEdgesSrcByDst<E::Unit_ShootDmg_Unit>(unit))
			{
				const auto & blockers = G.getAllEdgesSrcByDst<E::Unit_Blocks_Unit>(ounit);

				auto numBlockers = std::ranges::distance(blockers);
				if (numBlockers > 1)
					continue; // no threat (already blocked by someone else)

				auto actorIsBlocker = std::ranges::find(blockers, unit) != blockers.end();
				if (numBlockers == 1 && !actorIsBlocker)
					continue; // no threat (already blocked by someone else)

				bool willBlock = G.getEdgeBySrcDst<E::Action_Blocks_Unit>(action, ounit, false) != nullptr;
				if (willBlock)
					continue; // no threat (will become blocked after the move)

				const auto & ostack = ounit->cstack;
				float mult = 1;

				// XXX: using custom version of battleHasDistancePenalty where we can specify defender hex
				if(battleHasDistancePenalty_CUSTOM(&ostack, ostack.getPosition(), stack.getHexes(bhex)))
					mult *= 0.5;
				if(battle.battleHasWallPenalty(&ostack, ostack.getPosition(), hex->bhex))
					mult *= 0.5;

				G.add(std::make_shared<E::Action_ExposesToShootFrom_Unit>(action, ounit, mult));
			}
		}
	}

	void AddMoveActionEdges_Action_EnablesMeleeAt_Unit(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
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

		// throw std::runtime_error("AddMoveActionEdges_Action_EnablesMeleeAt_Unit: not implemented");
		// std::cout << "WARNING: AddMoveActionEdges_Action_EnablesMeleeAt_Unit: not implemented\n";
		G.setFlag(ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT);
	}

	void AddMoveActionEdges_Action_EnablesShootAt_Unit(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		G.getFlags().require(ET::NODE_ACTION);
		atFlags.requireExclusive({AT::DEFEND, AT::MOVE});
		G.getFlags().require(ET::EDGE_UNIT_OCCUPIES_HEX);
		G.getFlags().require(ET::EDGE_UNIT_MELEE_DMG_UNIT);
		G.getFlags().require(ET::EDGE_UNIT_SHOOT_DMG_UNIT);

		// See note in AddMoveActions()
		G.setFlag(ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT);

		// Plan:
		// For each MOVE action:
		//  1. Check if the actor can shoot
		//  1. Check if the actor will be blocked after the move
		// 	2. Find units with "ShootDmg" edge from the actor
		//
		for (const auto & action : G.getAll<N::Action>())
		{
			const auto & unit = action->by;
			const auto & stack = unit->cstack;

			if (!stack.canShoot())
				continue;

			const auto & hex = action->endsAt.at(0);

			auto willBeBlocked = [&G, &unit, &hex]()
			{
				for (const auto & adjbhex : unit->cstack.getSurroundingHexes(hex->bhex))
				{
					if (!adjbhex.isAvailable())
						continue;

					const auto & adjhex = G.getByExtraIndex<N::Hex>(adjbhex.toInt());
					const auto & ounit = G.getOneEdgeSrcByDst<E::Unit_Occupies_Hex>(adjhex, false);
					if (ounit && G.getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(ounit, unit, false))
						return true;
				}
				return false;
			};

			if (!stack.canShootBlocked() && willBeBlocked())
				continue;

			for (const auto & ounit : G.getAllEdgesDstBySrc<E::Unit_ShootDmg_Unit>(unit))
				G.add(std::make_shared<E::Action_EnablesShootAt_Unit>(action, ounit));
		}
	}

	void AddMoveActionEdges_Action_EnablesMeleeAt_Hex(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		// throw std::runtime_error("AddMoveActionEdges_Action_EnablesMeleeAt_Hex: not implemented");
		// std::cout << "WARNING: AddMoveActionEdges_Action_EnablesMeleeAt_Hex: not implemented\n";
		G.setFlag(ET::EDGE_ACTION_ENABLES_MELEE_AT_HEX);
	}

	void AddMoveActionEdges_Action_EnablesShootAt_Hex(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle,
		const CStack * acstack)
	{
		// throw std::runtime_error("AddMoveActionEdges_Action_EnablesShootAt_Hex: not implemented");
		// std::cout << "WARNING: AddMoveActionEdges_Action_EnablesShootAt_Hex: not implemented\n";
		G.setFlag(ET::EDGE_ACTION_ENABLES_SHOOT_AT_HEX);
	}

	template <typename T>
	void WithSnapshot(const auto & range, const auto & func)
	{
		// Iterate from a vector as new nodes will be added to the index
		for (const auto &item : std::vector<std::shared_ptr<const T>>(range.begin(), range.end()))
			func(item);
	};

	void CloneActionEdges(
		Graph::Graph & G,
		const ActionPtr & src,
		const std::shared_ptr<N::Action> & dst)
	{
		// Iterator over a *copy* of the index result
		auto iterateActionEdges = [&G, &src]<typename Edge>(const auto & func)
		{
			WithSnapshot<Edge>(G.getAllEdgesBySrc<Edge>(src), func);
		};

		// Most edges are simple edges with just a src and dst
		// => convenience function for cloning those
		auto cloneActionGenericEdges = [&G, &dst, &iterateActionEdges]<typename Edge>()
		{
			iterateActionEdges.template operator()<Edge>([&G, &dst](const auto & e)
			{
				G.add(std::make_shared<Edge>(dst, e->dstNode));
			});
		};

		for(int i = 0; i < EU(ET::_count); ++i)
		{
			switch(ET(i))
			{
				case ET::EDGE_ACTION_BY_UNIT:
					cloneActionGenericEdges.template operator()<E::Action_By_Unit>();
					break;
				case ET::EDGE_ACTION_ENDS_AT_HEX:
					iterateActionEdges.template operator()<E::Action_EndsAt_Hex>([&G, &dst](const auto & e) {
						G.add(std::make_shared<E::Action_EndsAt_Hex>(dst, e->dstNode, e->isRear));
					});
					break;
				case ET::EDGE_ACTION_BLOCKS_UNIT:
					cloneActionGenericEdges.template operator()<E::Action_Blocks_Unit>();
					break;
				case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
					cloneActionGenericEdges.template operator()<E::Action_ExposesToMeleeFrom_Unit>();
					break;
				case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
					iterateActionEdges.template operator()<E::Action_ExposesToShootFrom_Unit>([&G, &dst](const auto & e) {
						G.add(std::make_shared<E::Action_ExposesToShootFrom_Unit>(dst, e->dstNode, e->mult));
					});
					break;
				case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
					cloneActionGenericEdges.template operator()<E::Action_EnablesMeleeAt_Unit>();
					break;
				case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
					cloneActionGenericEdges.template operator()<E::Action_EnablesShootAt_Unit>();
					break;
				case ET::EDGE_ACTION_ENABLES_MELEE_AT_HEX:
					cloneActionGenericEdges.template operator()<E::Action_EnablesMeleeAt_Hex>();
					break;
				case ET::EDGE_ACTION_ENABLES_SHOOT_AT_HEX:
					cloneActionGenericEdges.template operator()<E::Action_EnablesShootAt_Hex>();
					break;
				// Nothing to add for those
        		case ET::EDGE_GLOBAL_HAS_PLAYER:
        		case ET::EDGE_GLOBAL_HAS_UNIT:
        		case ET::EDGE_GLOBAL_HAS_HEX:
        		case ET::EDGE_PLAYER_OWNS_UNIT:
				case ET::NODE_GLOBAL:
				case ET::NODE_PLAYER:
				case ET::NODE_UNIT:
				case ET::NODE_HEX:
				case ET::NODE_ACTION:
				case ET::EDGE_HEX_ADJACENT_HEX:
				case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
				case ET::EDGE_UNIT_MELEE_DMG_UNIT:
				case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
				case ET::EDGE_UNIT_BLOCKS_UNIT:
				case ET::EDGE_UNIT_OCCUPIES_HEX:
				case ET::EDGE_ACTION_MELEES_UNIT:
				case ET::EDGE_ACTION_SHOOTS_UNIT:
					break;
				default:
					throw std::runtime_error("Unexpected edge type: " + std::to_string(i));
			}
		}
	};

	void AddAmoveAction(
		Graph::Graph & G,
		const ActionPtr & move,
		const CPlayerBattleCallback & battle)
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
			if (!adjbhex.isAvailable())
				continue;

			const auto & adjhex = G.getByExtraIndex<N::Hex>(adjbhex.toInt());
			const auto & ounit = G.getOneEdgeSrcByDst<E::Unit_Occupies_Hex>(adjhex, false);

			if (!ounit)
				continue;

			if (!G.getEdgeBySrcDst<E::Unit_MeleeDmg_Unit>(unit, ounit, false))
				continue;

			auto [it, inserted] = ounits.emplace(ounit);
			if (!inserted)
				continue;

			assert(CStack::isMeleeAttackPossible(&unit->cstack, &ounit->cstack, hex->bhex));

			auto id = CalcActionId(AT::AMOVE, unit, ounit, hex);
			auto amove = std::make_shared<N::Action>(AT::AMOVE, id, unit, move->endsAt, move->isActive);

			G.add(amove);
			G.add(std::make_shared<E::Action_Melees_Unit>(amove, ounit, true));

			// Melee AoE attacks, e.g. dragons, hydras
			const auto & stack = unit->cstack;
			const auto & ostack = ounit->cstack;

			// XXX: The code in getAttackedCreatures (for melee attacks) uses
			//      destination hex ONLY for obtaining the defending stack
			// 		(which is great because the attack targets a unit, not a hex)
			const auto & [targets, _] = battle.getAttackedCreatures(
				&stack,
				ostack.getPosition(),
				false,
				hex->bhex
			);

			for (const auto & tstack : targets)
			{
				if (tstack == &ostack)
					// XXX: getAttackedCreatures has some stupid logic which does not
					//      return the creature standing on the target hex.
					//      HOWEVER, if the same creature occupies an AoE hex, it is returned
					//      => make sure to not duplicate it.
					continue;

				const auto & tunit = G.getByExtraIndex<N::Unit>(tstack->unitId());
				G.add(std::make_shared<E::Action_Melees_Unit>(amove, tunit, false));
			}

			CloneActionEdges(G, move, amove);
		}
	}

	void AddShootAction(
		Graph::Graph & G,
		const ActionPtr & defend,
		const CPlayerBattleCallback & battle)
	{
		// Iterate from a snapshot as new nodes will be added to the index (via clone)
		const auto & range = G.getAllEdgesDstBySrc<E::Action_EnablesShootAt_Unit>(defend);
		const auto edges = std::vector(range.begin(), range.end());

		for (const auto & ounit : edges)
		{
			auto id = CalcActionId(AT::SHOOT, defend->by, ounit, nullptr);
			auto shoot = std::make_shared<N::Action>(AT::SHOOT, id, defend->by, defend->endsAt, defend->isActive);
			const auto & unit = shoot->by;

			G.add(shoot);

			G.add(std::make_shared<E::Action_Shoots_Unit>(shoot, ounit, true));

			// AoE attacks - e.g. dragon breath
			const auto & stack = unit->cstack;
			const auto & ostack = ounit->cstack;
			const auto & ohex = G.getByExtraIndex<N::Hex>(ounit->cstack.getPosition().toInt());

			// XXX: The code in getAttackedCreatures (for ranged attacks)
			// 		does NOT consider AoE from SPELL_LIKE_ATTACK
			//      (which is all the AoE in vanilla H3/SoD: Fireball, Death Cloud)
			// 		Call it, in case some mod adds a regular non-spell like ranged AoE,
			// 		but make sure to handle the spell-like attacks separately.
			auto [targets, _] = battle.getAttackedCreatures(
				&stack,
				ohex->bhex,
				true,
				stack.getPosition()
			);

			// Handle spell-like attacks
			if (const auto & bonus = stack.getBonus(Selector::type()(BonusType::SPELL_LIKE_ATTACK)))
			{
				// Stolen from CBattleInfoCallback::estimateSpellLikeAttackDamage
				const auto * spell = bonus->subtype.as<SpellID>().toSpell();
				const auto & proxy = spells::ProxyCaster(&stack);
				const auto & params = spells::BattleCast(&battle, &proxy, spells::Mode::PASSIVE, spell);
				const auto mech = std::unique_ptr<spells::Mechanics>(spell->battleMechanics(&params));
				if(!mech)
					return;
				auto aim = spells::Target{};
				aim.emplace_back(ohex->bhex);
				for (const auto & tstack : mech->getAffectedStacks(aim))
					targets.emplace(tstack);
			}

			for (const auto & tstack : targets)
			{
				if (tstack == &ostack)
					// see note in AddAmoveAction
					continue;

				const auto & tunit = G.getByExtraIndex<N::Unit>(tstack->unitId());
				G.add(std::make_shared<E::Action_Shoots_Unit>(shoot, tunit, false));
			}

			CloneActionEdges(G, defend, shoot);
		}
	}

	void AddWaitAction(
		Graph::Graph & G,
		const ActionPtr & defend)
	{
		if (defend->by->cstack.waitedThisTurn)
			return;

		auto id = CalcActionId(AT::WAIT, nullptr, nullptr, nullptr);
		auto wait = std::make_shared<N::Action>(AT::WAIT, id, defend->by, defend->endsAt, defend->isActive);
		G.add(wait);
		CloneActionEdges(G, defend, wait);
	}

	void AddOtherActions(
		Graph::Graph & G,
		EnumFlags<AT> & atFlags,
		const CPlayerBattleCallback & battle)
	{
		// All MOVE actions with all their edges must be available here
		// except for MELEES and SHOOTS edges which are for AMOVE only
		G.getFlags().require(ET::NODE_ACTION);
		atFlags.requireExclusive({AT::DEFEND, AT::MOVE});
		G.getFlags().require(ET::EDGE_ACTION_BY_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_BLOCKS_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_ENDS_AT_HEX);
		G.getFlags().require(ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT);
		G.getFlags().require(ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT);

		G.setFlag(ET::EDGE_ACTION_MELEES_UNIT);
		G.setFlag(ET::EDGE_ACTION_SHOOTS_UNIT);
		atFlags.set(AT::AMOVE);
		atFlags.set(AT::SHOOT);
		atFlags.set(AT::WAIT);

		// These are needed for SHOOT actions
		auto defendmoves = std::unordered_map<UnitPtr, const ActionPtr>{};

		// Iterate from a snapshot as new actions will be added to the index
		const auto & range = G.getAll<N::Action>();
		const auto actions = std::vector(range.begin(), range.end());
		for (const auto & move : actions)
		{
			assert(move->actionType == AT::MOVE || move->actionType == AT::DEFEND);
			AddAmoveAction(G, move, battle);
			if(move->by->cstack.getPosition() == move->endsAt.at(0)->bhex)
				defendmoves.try_emplace(move->by, move);
		};

		for(const auto & unit : G.getAll<N::Unit>())
		{
			const auto & defendhex = G.getByExtraIndex<N::Hex>(unit->cstack.getPosition().toInt());
			const auto & defend = defendmoves.at(unit);
			AddShootAction(G, defend, battle);
			AddWaitAction(G, defend);
		}
	}
}

State::State(
	int version,
	const std::string & colorname,
	const CPlayerBattleCallback & battle)
: version_(version)
, battle(battle)
, colorname(colorname)
, side(battle.battleGetMySide())
, startStats(CalcGlobalStats(battle))
, lastStats(startStats)
{
}

void State::onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa)
{
	if (!G)
		// Ignore logs until our first turn starts
		return;

	auto cstacks = battle.battleGetStacks();

	for(const auto & elem : bsa)
	{
		const auto * defender = battle.battleGetStackByID(static_cast<int>(elem.stackAttacked), false);
		const auto * attacker = battle.battleGetStackByID(static_cast<int>(elem.attackerID), false);

		if(!defender)
		{
			logAi->error("MMAI: received BattleStackAttacked with invalid stackAttacked: " + std::to_string(elem.stackAttacked));
			continue;
		}

		auto bf_valueNow = lastStats.leftValue + lastStats.rightValue;
		auto bf_hpNow = lastStats.leftHp + lastStats.rightHp;
		auto value = elem.killedAmount * N::Unit::GetValue(defender->unitType());

		attackLogs.emplace_back(
			// attacker and/or defender CStack may be missing in G
			// (e.g. resurrected after G was constructed)
			attacker ? G->getByExtraIndex<N::Unit>(attacker->unitId(), false) : nullptr,
			G->getByExtraIndex<N::Unit>(defender->unitId(), false),
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
	G = std::make_shared<Graph::Graph>(battle);

	const auto stats = CalcGlobalStats(battle);
	const auto logdata = ProcessAttackLogs(attackLogs);

	G->buildAccessibilityCache();

	AddGlobalNode(*G, battle, acstack, result, round, stats);
	AddPlayerNodes(*G, battle, lastStats, stats, logdata);
	AddUnitNodes(*G, battle, acstack, stats);
	AddHexNodes(*G, battle, acstack);

	G->buildReachabilityCache(); // requires Units

	AddEdges_Global_Has_PlayerUnitHex(*G, battle);
	AddEdges_Player_Owns_Unit(*G, battle);
	AddEdges_Hex_Adjacent_Hex(*G);
	AddEdges_Unit_ActsBefore_Unit(*G, battle);
	AddEdges_Unit_MeleeDmg_Unit(*G, battle, stats);
	AddEdges_Unit_ShootDmg_Unit(*G, battle, stats);
	AddEdges_Unit_Blocks_Unit(*G);
	AddEdges_Unit_Occupies_Hex(*G);

	auto atFlags = EnumFlags<S15::ActionType>();
	AddMoveAndDefendActions(*G, atFlags, acstack); // + edges: ActionByUnit, ActionEndsAtHex
	// AddMoveActionEdges_Action_By_Unit() // already added
	AddMoveActionEdges_Action_Blocks_Unit(*G, atFlags, battle, acstack);
	// AddMoveActionEdges_Action_EndsAt_Hex() // already added
	AddMoveActionEdges_Action_ExposesToMeleeFrom_Unit(*G, atFlags);
	AddMoveActionEdges_Action_ExposesToShootFrom_Unit(*G, atFlags, battle);
	AddMoveActionEdges_Action_EnablesMeleeAt_Unit(*G, atFlags, battle, acstack);
	AddMoveActionEdges_Action_EnablesShootAt_Unit(*G, atFlags, battle, acstack);

	// active actions only
	AddMoveActionEdges_Action_EnablesMeleeAt_Hex(*G, atFlags, battle, acstack);
	AddMoveActionEdges_Action_EnablesShootAt_Hex(*G, atFlags, battle, acstack);

	AddOtherActions(*G, atFlags, battle);

	ASSERT(G->getFlags().flags.all(), "etFlags check: " + G->getFlags().flags.to_string());
	ASSERT(atFlags.flags.all(), "atFlags check: " + atFlags.flags.to_string());
	G->verify();

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
