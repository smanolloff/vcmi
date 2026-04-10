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

#include "battle/CPlayerBattleCallback.h"
#include "entities/building/TownFortifications.h"
#include "networkPacks/PacksForClientBattle.h"

#include "BAI/v14/encoder.h"
#include "BAI/v14/hexaction.h"
#include "BAI/v14/state.h"
#include "BAI/v14/supplementary_data.h"
#include "common.h"
#include "schema/v14/constants.h"

namespace MMAI::BAI::V14
{
namespace S14 = Schema::V14;
using GA = Schema::V14::GlobalAttribute;
using PA = Schema::V14::PlayerAttribute;
using HA = Schema::V14::HexAttribute;
using SA = Schema::V14::StackAttribute;

//
// Prevent human errors caused by the Stack / Hex attr overlap
//
static_assert(EI(HA::STACK_SIDE) == EI(SA::SIDE) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_SLOT) == EI(SA::SLOT) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_QUANTITY) == EI(SA::QUANTITY) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_ATTACK) == EI(SA::ATTACK) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DEFENSE) == EI(SA::DEFENSE) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_SHOTS) == EI(SA::SHOTS) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_MIN) == EI(SA::DMG_MIN) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_MAX) == EI(SA::DMG_MAX) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_HP) == EI(SA::HP) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_HP_LEFT) == EI(SA::HP_LEFT) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_SPEED) == EI(SA::SPEED) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_QUEUE) == EI(SA::QUEUE) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_ONE) == EI(SA::VALUE_ONE) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_FLAGS1) == EI(SA::FLAGS1) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_FLAGS2) == EI(SA::FLAGS2) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_REL) == EI(SA::VALUE_REL) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_REL0) == EI(SA::VALUE_REL0) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_KILLED_REL) == EI(SA::VALUE_KILLED_REL) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_KILLED_ACC_REL0) == EI(SA::VALUE_KILLED_ACC_REL0) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_LOST_REL) == EI(SA::VALUE_LOST_REL) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_VALUE_LOST_ACC_REL0) == EI(SA::VALUE_LOST_ACC_REL0) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_DEALT_REL) == EI(SA::DMG_DEALT_REL) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_DEALT_ACC_REL0) == EI(SA::DMG_DEALT_ACC_REL0) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_RECEIVED_REL) == EI(SA::DMG_RECEIVED_REL) + S14::STACK_ATTR_OFFSET);
static_assert(EI(HA::STACK_DMG_RECEIVED_ACC_REL0) == EI(SA::DMG_RECEIVED_ACC_REL0) + S14::STACK_ATTR_OFFSET);
static_assert(EI(StackAttribute::_count) == 25, "whistleblower in case attributes change");

// static
std::vector<float> State::InitNullStack()
{
	auto res = std::vector<float>{};
	for(int i = 0; i < EI(StackAttribute::_count); ++i)
		Encoder::Encode(static_cast<HA>(S14::STACK_ATTR_OFFSET + i), S14::NULL_VALUE_UNENCODED, res);
	return res;
};

namespace
{
	std::tuple<int, int, int, int> CalcGlobalStats(const CPlayerBattleCallback * battle)
	{
		int lv = 0;
		int lh = 0;
		int rv = 0;
		int rh = 0;

		for(auto & stack : battle->battleGetStacks())
		{
			auto v = stack->getCount() * Stack::GetValue(stack->unitType());
			auto h = stack->getAvailableHealth();

			if(stack->unitSide() == BattleSide::ATTACKER)
			{
				lv += v;
				lh += h;
			}
			else
			{
				rv += v;
				rh += h;
			}
		}

		return {lv, lh, rv, rh};
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

	AttackLogAggregateData ProcessAttackLogs(const std::vector<std::shared_ptr<AttackLog>> & attackLogs, std::map<const CStack *, Stack::Stats> sstats)
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

	TowerFlags GetSiegeTowers(const CPlayerBattleCallback * battle) {
		TowerFlags res = 0; // {upper, middle, lower}

		auto has = [&battle](EWallPart part) {
			auto ws = battle->battleGetWallState(part);
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

	CorpseFlags GetSiegeCorpses(const CPlayerBattleCallback * battle)
	{
		CorpseFlags res = 0; // {gate, bridge}

		if(battle->battleGetFortifications().wallsHealth == 0)
			return res;

		for(const auto & cstack : battle->battleGetAllStacks(false))
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
}

State::State(
	int version_,
	const std::string & colorname,
	const CPlayerBattleCallback * battle
)
	: version_(version_)
	, battle(battle)
	, colorname(colorname)
	, side(battle->battleGetMySide())
	, nullstack(InitNullStack())
{
	auto [lv, lh, rv, rh] = CalcGlobalStats(battle);

	auto cache = std::make_shared<Cache>(battle);
	gstats = std::make_unique<GlobalStats>(battle->battleGetMySide(), lv + rv, lh + rh, GetSiegeTowers(battle), GetSiegeCorpses(battle));
	lpstats = std::make_unique<PlayerStats>(BattleSide::LEFT_SIDE, lv, lh);
	rpstats = std::make_unique<PlayerStats>(BattleSide::RIGHT_SIDE, rv, rh);

	battlefield = Battlefield::Create(cache, battle, nullptr, gstats.get(), gstats.get(), sstats, false);
	bfstate.reserve(S14::BATTLEFIELD_STATE_SIZE);
	actmask.reserve(S14::N_ACTIONS);
}

void State::onActiveStack(
	const CStack * astack,
	int round,
	CombatResult result
)
{
	logAi->debug("onActiveStack: round=%d, result=%d", round, EI(result));
	auto cache = std::make_shared<Cache>(battle);
	const auto & [lv, lh, rv, rh] = CalcGlobalStats(battle);
	const auto & [ldd, ldr, lvk, lvl, rdd, rdr, rvk, rvl] = ProcessAttackLogs(attackLogs, sstats);
	auto ogstats = *gstats; // a copy of the "old" gstats

	(result == CombatResult::NONE) ? gstats->update(astack->unitSide(), result, lv + rv, lh + rh, !astack->waitedThisTurn, GetSiegeTowers(battle), GetSiegeCorpses(battle), round)
								   : gstats->update(battle->battleGetMySide(), result, lv + rv, lh + rh, false, GetSiegeTowers(battle), GetSiegeCorpses(battle), round);
	lpstats->update(&ogstats, lv, lh, ldd, ldr, lvk, lvl);
	rpstats->update(&ogstats, rv, rh, rdd, rdr, rvk, rvl);

	battlefield = Battlefield::Create(cache, battle, astack, &ogstats, gstats.get(), sstats, isMorale);
	bfstate.clear();
	actmask.clear();

	for(int i = 0; i < EI(GlobalAction::_count); i++)
	{
		switch(static_cast<GlobalAction>(i))
		{
			case GlobalAction::RETREAT:
				actmask.push_back(battle->battleCanFlee());
				break;
			case GlobalAction::WAIT:
				actmask.push_back(battlefield->astack && !battlefield->astack->cstack->waitedThisTurn);
				break;
			default:
				THROW_FORMAT("Unexpected GlobalAction: %d", i);
		}
	}

	encodeGlobal(result);
	encodePlayer(lpstats.get());
	encodePlayer(rpstats.get());

	for(const auto & hexrow : *battlefield->hexes)
		for(const auto & hex : hexrow)
			encodeHex(hex.get());

	// Links are not part of the state
	// They are handled separately by the connector
	// for (auto &link : battlefield->links)
	//     encodeLink(link);

	verify();

	isMorale = false;

	supdata = std::make_unique<SupplementaryData>(
		colorname,
		static_cast<Side>(side),
		gstats.get(),
		lpstats.get(),
		rpstats.get(),
		battlefield.get(),
		attackLogs, // store the logs since OUR last turn
		result
	);

	attackLogs.clear(); // accumulate new logs until next turn
}

void State::encodeGlobal(CombatResult result)
{
	for(int i = 0; i < EI(GA::_count); ++i)
	{
		Encoder::Encode(static_cast<GA>(i), gstats->attrs.at(i), bfstate);
	}
}

void State::encodePlayer(const PlayerStats * pstats)
{
	for(int i = 0; i < EI(PA::_count); ++i)
	{
		Encoder::Encode(static_cast<PA>(i), pstats->attrs.at(i), bfstate);
	}
}

void State::encodeHex(const Hex * hex)
{
	// Battlefield state
	for(int i = 0; i < EI(HA::_count); ++i)
		Encoder::Encode(static_cast<HA>(i), hex->attrs.at(i), bfstate);

	// Action mask
	for(int m = 0; m < hex->actmask.size(); ++m)
		actmask.push_back(hex->actmask.test(m));
}

void State::verify() const
{
	ASSERT(bfstate.size() == S14::BATTLEFIELD_STATE_SIZE, "unexpected bfstate.size(): " + std::to_string(bfstate.size()));
	ASSERT(actmask.size() == N_ACTIONS, "unexpected actmask.size(): " + std::to_string(actmask.size()));
}

void State::onBattleStacksAttacked(const std::vector<BattleStackAttacked> & bsa)
{
	auto stacks = battlefield->stacks;

	for(const auto & elem : bsa)
	{
		const auto * cdefender = battle->battleGetStackByID(elem.stackAttacked, false);
		const auto * cattacker = battle->battleGetStackByID(elem.attackerID, false);

		if(!cdefender)
		{
			logAi->error("MMAI: received BattleStackAttacked with invalid stackAttacked: " + std::to_string(elem.stackAttacked));
			continue;
		}

		const auto defender = std::ranges::find_if(
			stacks,
			[&cdefender](const std::shared_ptr<Stack> & stack)
			{
				return cdefender == stack->cstack;
			}
		);

		if(defender == stacks.end())
		{
			logAi->info("defender cstack '%s' not found in stacks. Maybe it was just summoned/resurrected?", cdefender->getDescription());
		}

		const auto attacker = std::ranges::find_if(
			stacks,
			[&cattacker](const std::shared_ptr<Stack> & stack)
			{
				return cattacker == stack->cstack;
			}
		);

		auto bf_valueNow = gstats->attr(GA::BFIELD_VALUE_NOW_ABS);
		auto bf_hpNow = gstats->attr(GA::BFIELD_HP_NOW_ABS);
		auto value = elem.killedAmount * Stack::GetValue(cdefender->unitType());

		// XXX: attacker can be NULL when an effect does dmg (eg. Acid)
		// XXX: attacker or defender can be NULL if it did not exist
		//      when `stacks` was built (e.g. during our last turn),
		//      Can happen if the enemy has now summonned/resurrected it.
		auto ald = AttackLogData{
			.attacker = (attacker != stacks.end() ? *attacker : nullptr),
			.defender = (defender != stacks.end() ? *defender : nullptr),
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
			onActiveStack(nullptr, round, CombatResult::LEFT_WINS);
			break;
		case BattleSide::RIGHT_SIDE:
			onActiveStack(nullptr, round, CombatResult::RIGHT_WINS);
			break;
		default:
			onActiveStack(nullptr, round, CombatResult::DRAW);
	}
}
};
