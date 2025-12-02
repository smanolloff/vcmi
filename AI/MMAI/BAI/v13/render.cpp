/*
 * render.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleAttackInfo.h"
#include "battle/CObstacleInstance.h"
#include "battle/IBattleInfoCallback.h"
#include "constants/EntityIdentifiers.h"
#include "mapObjects/CGTownInstance.h"
#include "vcmi/spells/Caster.h"

#include "BAI/v13/hex.h"
#include "BAI/v13/hexactmask.h"
#include "BAI/v13/render.h"

#include "schema/v13/constants.h"
#include "schema/v13/types.h"

namespace MMAI::BAI::V13
{

namespace S13 = Schema::V13;
using IHex = Schema::V13::IHex;
using IStack = Schema::V13::IStack;
using ISupplementaryData = Schema::V13::ISupplementaryData;
using GA = Schema::V13::GlobalAttribute;
using HA = Schema::V13::HexAttribute;
using PA = Schema::V13::PlayerAttribute;
using SA = StackAttribute;
using SF1 = StackFlag1;
using SF2 = StackFlag2;

namespace
{
	std::string PadLeft(const std::string & input, size_t desiredLength, char paddingChar)
	{
		std::ostringstream ss;
		ss << std::right << std::setfill(paddingChar) << std::setw(desiredLength) << input;
		return ss.str();
	}

	std::string PadRight(const std::string & input, size_t desiredLength, char paddingChar)
	{
		std::ostringstream ss;
		ss << std::left << std::setfill(paddingChar) << std::setw(desiredLength) << input;
		return ss.str();
	}

	template<typename... Args>
	inline void expect(bool exp, const std::string_view format, Args &&... args)
	{
		if(exp)
			return;

		constexpr std::size_t bufferSize = 2048;
		char buffer[bufferSize];
		std::string fmt{format};
		std::snprintf(buffer, bufferSize, fmt.c_str(), std::forward<Args>(args)...);
		throw std::runtime_error(buffer);
	}

	template<typename... Args>
	inline void expect(bool exp, const std::string_view format)
	{
		if(exp)
			return;

		constexpr std::size_t bufferSize = 2048;
		char buffer[bufferSize];
		std::string fmt{format};
		std::snprintf(buffer, bufferSize, "%s", fmt.c_str());
		throw std::runtime_error(buffer);
	}
}

// This function used during model development and is never called otherwise
void Verify(const State * state) // NOSONAR - function used for debugging only
{
	const auto * battle = state->battle;
	auto hexes = Hexes();

	expect(battle, "no battle to verify");

	const CStack * astack = nullptr; // XXX: can remain nullptr (for terminal obs)
	auto l_CStacks = std::array<const CStack *, 7>{};
	auto r_CStacks = std::array<const CStack *, 7>{};
	auto l_CStacksSummons = std::vector<const CStack *>{};
	auto r_CStacksSummons = std::vector<const CStack *>{};
	auto l_CStacksMachines = std::vector<const CStack *>{};
	auto r_CStacksMachines = std::vector<const CStack *>{};
	auto hexstacks = std::array<const CStack *, 165>{};

	auto rinfos = std::map<const CStack *, ReachabilityInfo>{};
	auto allstacks = battle->battleGetStacks();
	std::sort(
		allstacks.begin(),
		allstacks.end(),
		[](const CStack * a, const CStack * b)
		{
			return a->unitId() < b->unitId();
		}
	);

	for(auto & cstack : allstacks)
	{
		if(cstack->unitId() == battle->battleActiveUnit()->unitId())
			astack = cstack;

		if(cstack->unitSlot() < 0)
		{
			if(cstack->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER)
				cstack->unitSide() == BattleSide::DEFENDER ? r_CStacksSummons.push_back(cstack) : l_CStacksSummons.push_back(cstack);
			else if(cstack->unitSlot() == SlotID::WAR_MACHINES_SLOT)
				cstack->unitSide() == BattleSide::DEFENDER ? r_CStacksMachines.push_back(cstack) : l_CStacksMachines.push_back(cstack);
		}
		else
		{
			cstack->unitSide() == BattleSide::DEFENDER ? r_CStacks.at(cstack->unitSlot()) = cstack : l_CStacks.at(cstack->unitSlot()) = cstack;
		}

		rinfos.insert({cstack, battle->getReachability(cstack)});

		for(const auto & bh : cstack->getHexes())
		{
			if(!bh.isAvailable())
				continue; // war machines rear hex, arrow towers
			expect(!hexstacks.at(Hex::CalcId(bh)), "hex occupied by multiple stacks?");
			hexstacks.at(Hex::CalcId(bh)) = cstack;
		}
	}

	auto l_CStacksAll = std::vector<const CStack *>{};
	l_CStacksAll.insert(l_CStacksAll.end(), l_CStacks.begin(), l_CStacks.end());
	l_CStacksAll.insert(l_CStacksAll.end(), l_CStacksSummons.begin(), l_CStacksSummons.end());
	l_CStacksAll.insert(l_CStacksAll.end(), l_CStacksMachines.begin(), l_CStacksMachines.end());

	auto r_CStacksAll = std::vector<const CStack *>{};
	r_CStacksAll.insert(r_CStacksAll.end(), r_CStacks.begin(), r_CStacks.end());
	r_CStacksAll.insert(r_CStacksAll.end(), r_CStacksSummons.begin(), r_CStacksSummons.end());
	r_CStacksAll.insert(r_CStacksAll.end(), r_CStacksMachines.begin(), r_CStacksMachines.end());

	auto l_CStacksExtra = std::vector<const CStack *>{};
	l_CStacksExtra.insert(l_CStacksExtra.end(), l_CStacksSummons.begin(), l_CStacksSummons.end());
	l_CStacksExtra.insert(l_CStacksExtra.end(), l_CStacksMachines.begin(), l_CStacksMachines.end());

	auto r_CStacksExtra = std::vector<const CStack *>{};
	r_CStacksExtra.insert(r_CStacksExtra.end(), r_CStacksSummons.begin(), r_CStacksSummons.end());
	r_CStacksExtra.insert(r_CStacksExtra.end(), r_CStacksMachines.begin(), r_CStacksMachines.end());

	auto getAllStacksForSide = [&r_CStacksAll, &l_CStacksAll](bool side)
	{
		return side ? r_CStacksAll : l_CStacksAll;
	};

	auto ended = state->supdata->ended;

	if(!astack)
		expect(ended, "astack is NULL, but ended is not true");
	else if(ended)
	{
		// at battle-end, activeStack is usually the ENEMY stack
		// XXX: this expect will incorrectly throw if we retreated as a regular action
		//      (in which case our stack will be active, but we would have lost the battle)
		// expect(state->supdata->victory == (astack->getOwner() == battle->battleGetMySide()), "state->supdata->victory is %d, but astack->side=%d and myside=%d", state->supdata->victory, astack->getOwner(), battle->battleGetMySide());

		// at battle-end, even regardless of the actual active stack,
		// battlefield->astack must be nullptr
		expect(!state->battlefield->astack, "ended, but battlefield->astack is not NULL");
		expect(state->supdata->getIsBattleEnded(), "ended, but state->supdata->getIsBattleEnded() is false");
	}

	// Return (attr == N/A), but after performing some checks
	auto isNA = [](int v, const CStack * stack, const std::string_view attrname)
	{
		if(v == S13::NULL_VALUE_UNENCODED)
		{
			expect(!stack, "%s: N/A but stack != nullptr", attrname);
			return true;
		}
		expect(stack, "%s: != N/A but stack = nullptr", attrname);
		return false;
	};

	auto checkReachable = [=](BattleHex bh, bool v, const CStack * stack)
	{
		auto distance = rinfos.at(stack).distances.at(bh.toInt());
		auto canreach = (stack->getMovementRange() >= distance);

		// XXX: if v=false, returns true when UNreachable
		//      if v=true returns true when reachable
		return v ? canreach : !canreach;
	};

	auto ensureReachability = [=](BattleHex bh, bool v, const CStack * stack, const char * attrname)
	{
		expect(checkReachable(bh, v, stack), "%s: (bhex=%d) reachability expected: %d", attrname, bh.toInt(), v);
	};

	auto ensureValueMatch = [](int have, int want, const std::string_view attrname, const std::string & desc = "")
	{
		desc.empty() ? expect(have == want, "%s: have: %d, want: %d", attrname, have, want)
					 : expect(have == want, "%s: have: %d, want: %d (%s)", attrname, have, want, desc.c_str());
	};

	auto ensureStackNullOrMatch = [=](HexAttribute a, const CStack * cstack, int have, auto wantfunc, const std::string_view attrname)
	{
		auto vmax = std::get<3>(S13::HEX_ENCODING.at(EI(a)));
		if(isNA(have, cstack, attrname))
			return;
		int want = wantfunc();
		want = std::min(want, vmax);
		have = std::min(have, vmax); // this is usually done by the encoder
		ensureValueMatch(have, want, attrname);
	};

	auto ensureMeleeability = [=](BattleHex bh, HexActMask mask, HexAction ha, const CStack * cstack, const char * attrname)
	{
		auto mv = mask.test(EI(ha));

		// if AMOVE is allowed, we must be able to reach hex
		// (no else -- we may still be able to reach it)
		if(mv == 1)
			ensureReachability(bh, 1, cstack, attrname);

		auto r_nbh = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
		auto l_nbh = bh.cloneInDirection(BattleHex::EDir::LEFT, false);
		auto nbh = BattleHex{};

		switch(ha)
		{
			case HexAction::AMOVE_TR:
				nbh = bh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false);
				break;
			case HexAction::AMOVE_R:
				nbh = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
				break;
			case HexAction::AMOVE_BR:
				nbh = bh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false);
				break;
			case HexAction::AMOVE_BL:
				nbh = bh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false);
				break;
			case HexAction::AMOVE_L:
				nbh = bh.cloneInDirection(BattleHex::EDir::LEFT, false);
				break;
			case HexAction::AMOVE_TL:
				nbh = bh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false);
				break;
			case HexAction::AMOVE_2TR:
				nbh = r_nbh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false);
				break;
			case HexAction::AMOVE_2R:
				nbh = r_nbh.cloneInDirection(BattleHex::EDir::RIGHT, false);
				break;
			case HexAction::AMOVE_2BR:
				nbh = r_nbh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false);
				break;
			case HexAction::AMOVE_2BL:
				nbh = l_nbh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false);
				break;
			case HexAction::AMOVE_2L:
				nbh = l_nbh.cloneInDirection(BattleHex::EDir::LEFT, false);
				break;
			case HexAction::AMOVE_2TL:
				nbh = l_nbh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false);
				break;
			default:
				THROW_FORMAT("Unexpected HexAction: %d", EI(ha));
				break;
		}

		auto estacks = getAllStacksForSide(!EI(cstack->unitSide()));
		auto it = std::find_if(
			estacks.begin(),
			estacks.end(),
			[&nbh](auto stack)
			{
				return stack && stack->coversPos(nbh);
			}
		);

		const auto * estack = it == estacks.end() ? nullptr : *it;

		if(mv)
		{
			expect(estack, "%s: =1 (bhex %d, nbhex %d), but estack is nullptr", attrname, bh.toInt(), nbh.toInt());
			// must not pass "nbh" for defender position, as it could be its rear hex
			expect(
				cstack->isMeleeAttackPossible(cstack, estack, bh),
				"%s: =1 (bhex %d, nbhex %d), but VCMI says isMeleeAttackPossible=0",
				attrname,
				bh.toInt(),
				nbh.toInt()
			);
		}

		//  else {
		//     if (it != estacks.end()) {
		//         auto estack = *it;
		//         // MASK may prohibit attack, but hex may still be reachable
		//         if (checkReachable(bh, 1, cstack))
		//             // MASK may prohibita this specific attack from a reachable hex
		//             // it does not mean any attack is impossible
		//             expect(!cstack->isMeleeAttackPossible(cstack, estack, bh), "%s: =0 (bhex %d, nbhex %d), but bhex is reachable and VCMI says isMeleeAttackPossible=1", attrname, bh.toInt(), nbh.toInt());
		//     }
		// }
	};

	// as opposed to ensureHexShootableOrNA, this hexattr works with a mask
	// values are 0 or 1 and this check requires a valid target
	auto ensureShootability = [=](BattleHex bh, int v, const CStack * cstack, const char * attrname)
	{
		auto canshoot = battle->battleCanShoot(cstack);
		auto estacks = getAllStacksForSide(!EI(cstack->unitSide()));
		auto it = std::find_if(
			estacks.begin(),
			estacks.end(),
			[&bh](auto estack)
			{
				return estack && estack->coversPos(bh);
			}
		);

		const auto * estack = it == estacks.end() ? nullptr : *it;

		// XXX: the estack on `bh` might be "hidden" from the state
		//      in which case the mask for shooting will be 0 although
		//      there IS a stack to shoot on this hex
		if(v)
		{
			expect(estack, "%s: =%d, but estack is nullptr", attrname, bh.toInt());
			expect(canshoot, "%s: =%d but canshoot=%d", attrname, v, canshoot);
		}
		else
		{
			// stack must be unable to shoot
			// OR there must be no target at hex
			expect(!canshoot || !estack, "%s: =%d but canshoot=%d and estack is not null", attrname, v, canshoot);
		}
	};

	auto ensureCorrectMaskOrNA = [=](BattleHex bh, int v, const CStack * cstack, const CPlayerBattleCallback * battle, const std::string_view attrname)
	{
		if(isNA(v, cstack, attrname))
			return;

		auto basename = std::string(attrname);
		auto mask = HexActMask(v);

		ensureReachability(bh, mask.test(EI(HexAction::MOVE)), cstack, (basename + "{MOVE}").c_str());
		ensureShootability(bh, mask.test(EI(HexAction::SHOOT)), cstack, (basename + "{SHOOT}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_TR, cstack, (basename + "{AMOVE_TR}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_R, cstack, (basename + "{AMOVE_R}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_BR, cstack, (basename + "{AMOVE_BR}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_BL, cstack, (basename + "{AMOVE_BL}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_L, cstack, (basename + "{AMOVE_L}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_TL, cstack, (basename + "{AMOVE_TL}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2TR, cstack, (basename + "{AMOVE_2TR}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2R, cstack, (basename + "{AMOVE_2R}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2BR, cstack, (basename + "{AMOVE_2BR}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2BL, cstack, (basename + "{AMOVE_2BL}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2L, cstack, (basename + "{AMOVE_2L}").c_str());
		ensureMeleeability(bh, mask, HexAction::AMOVE_2TL, cstack, (basename + "{AMOVE_2TL}").c_str());
	};

	// XXX: good morale is NOT handled here for simplicity
	//      See comments in Battlefield::GetQueue how to handle it.
	auto tmp = std::vector<battle::Units>{};
	battle->battleGetTurnOrder(tmp, S13::STACK_QUEUE_SIZE, 0);
	auto queue = std::vector<const battle::Unit *>{};
	for(auto & units : tmp)
	{
		for(auto & unit : units)
		{
			if(queue.size() < S13::STACK_QUEUE_SIZE)
				queue.push_back(unit);
			else
				break;
		}
	}

	const auto * gstats = state->supdata->getGlobalStats();
	auto gmask = GlobalActionMask(gstats->getAttr(GA::ACTION_MASK));
	ensureValueMatch(gmask.test(EI(GlobalAction::RETREAT)), false, "GA.ACTION_MASK[RETREAT]");

	if(ended)
	{
		static_assert(EI(Side::LEFT) == EI(BattleSide::ATTACKER));
		static_assert(EI(Side::RIGHT) == EI(BattleSide::DEFENDER));

		auto fin = battle->battleIsFinished();

		// XXX: The logic in battleIsFinished is flawed and returns no value
		//      (i.e. "not finished") if both sides have units, which can
		//      happen if the WE some has retreated as a regular action (not via reset).
		// ASSERT(fin.has_value(), "ended, but battleIsFinished returns no value?");

		if(fin.has_value())
		{
			// NONE means draw (no units on battlefield) -- our value will be null in this case
			(fin == BattleSide::NONE) ? ensureValueMatch(gstats->getAttr(GA::BATTLE_WINNER), S13::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (draw)")
									  : ensureValueMatch(gstats->getAttr(GA::BATTLE_WINNER), EI(fin.value()), "GA.BATTLE_WINNER");
		}
		else
		{
			// we have retreated *as an action*
			// There seems to be no way to ask vcmi "which side retreated"
		}

		ensureValueMatch(gstats->getAttr(GA::BATTLE_SIDE_ACTIVE_PLAYER), S13::NULL_VALUE_UNENCODED, "GA.BATTLE_SIDE_ACTIVE_PLAYER");
		ensureValueMatch(gmask.test(EI(GlobalAction::WAIT)), false, "GA.ACTION_MASK[WAIT]");
	}
	else
	{
		static_assert(EI(Side::LEFT) == EI(BattleSide::LEFT_SIDE));
		static_assert(EI(Side::RIGHT) == EI(BattleSide::RIGHT_SIDE));
		ASSERT(astack != nullptr, "not ended, but no astack either");
		ensureValueMatch(gstats->getAttr(GA::BATTLE_WINNER), S13::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (battle ongoing)");
		ensureValueMatch(gstats->getAttr(GA::BATTLE_SIDE_ACTIVE_PLAYER), EI(astack->unitSide()), "GA.BATTLE_SIDE_ACTIVE_PLAYER");
		ensureValueMatch(gmask.test(EI(GlobalAction::WAIT)), !astack->waitedThisTurn, "GA.ACTION_MASK[WAIT]");
	}
	auto alogs = state->supdata->getAttackLogs();

	for(int ihex = 0; ihex < 165; ihex++)
	{
		int x = ihex % 15;
		int y = ihex / 15;
		auto & hex = state->battlefield->hexes->at(y).at(x);
		auto bh = hex->bhex;
		expect(bh == BattleHex(x + 1, y), "hex->bhex mismatch");

		auto ainfo = battle->getAccessibility();
		auto aa = ainfo.at(bh.toInt());

		for(int i = 0; i < EI(HexAttribute::_count); i++)
		{
			auto attr = HexAttribute(i);
			auto v = hex->attrs.at(i);
			const auto * cstack = hexstacks.at(ihex);

			if(cstack)
			{
				expect(!!hex->stack, "cstack is present, but hex->stack is nullptr");
				expect(hex->stack->cstack == cstack, "hex->cstack != cstack");
			}
			else
			{
				expect(!hex->stack, "cstack is nullptr, but hex->stack is present");
			}

			switch(attr)
			{
				case HA::Y_COORD:
					expect(v == y, "HEX.Y_COORD: %d != %d", v, y);
					break;
				case HA::X_COORD:
					expect(v == x, "HEX.X_COORD: %d != %d", v, x);
					break;
				case HA::STATE_MASK:
				{
					auto obstacles = battle->battleGetAllObstaclesOnPos(bh, false);
					auto anyobstacle = [&obstacles](auto fn)
					{
						return std::any_of(
							obstacles.begin(),
							obstacles.end(),
							[&fn](std::shared_ptr<const CObstacleInstance> & obstacle)
							{
								return fn(obstacle.get());
							}
						);
					};

					auto mask = HexStateMask(v);
					BattleSide side = astack ? astack->unitSide() : BattleSide::ATTACKER; // XXX: Hex defaults to 0 if there is no astack

					if(mask.test(EI(HexState::PASSABLE)))
					{
						expect(
							aa == EAccessibility::ACCESSIBLE || (EI(side) && aa == EAccessibility::GATE),
							"HEX.STATE_MASK: PASSABLE bit is set, but accessibility is %d (side: %d)",
							EI(aa),
							side
						);
					}
					else
					{
						if(aa == EAccessibility::OBSTACLE || aa == EAccessibility::ALIVE_STACK)
							break;

						switch(aa)
						{
							case EAccessibility::ACCESSIBLE:
								throw std::runtime_error("HEX.STATE_MASK: PASSABLE bit not set, but accessibility is ACCESSIBLE");
								break;
							case EAccessibility::ALIVE_STACK:
							case EAccessibility::OBSTACLE:
							case EAccessibility::DESTRUCTIBLE_WALL:
							case EAccessibility::GATE:
								break;
							case EAccessibility::UNAVAILABLE:
								// only Fort and Boat battles can have unavailable hexes
								expect(
									battle->battleGetFortifications().wallsHealth > 0 || battle->battleTerrainType() == TerrainId::WATER,
									"Found UNAVAILABLE accessibility on non-fort, non-boat battlefield: tertype=%d",
									EI(battle->battleTerrainType())
								);
								break;
							case EAccessibility::SIDE_COLUMN:
								// side hexes should are not included in the observation
								throw std::runtime_error("HEX.STATE_MASK: SIDE_COLUMN accessibility found");
								break;
							default:
								throw std::runtime_error("Unexpected accessibility: " + std::to_string(EI(aa)));
						}
					}

					if(mask.test(EI(HexState::STOPPING)))
					{
						auto stopping = anyobstacle(std::mem_fn(&CObstacleInstance::stopsMovement));
						expect(stopping, "HEX.STATE_MASK: STOPPING bit is set, but no obstacle stops movement");
					}

					if(mask.test(EI(HexState::DAMAGING_L)))
					{
						auto damaging = anyobstacle(
							[side](const CObstacleInstance * o)
							{
								if(o->obstacleType == CObstacleInstance::MOAT)
									return true;
								if(!o->triggersEffects())
									return false;
								auto s = SpellID(o->ID);
								if(s == SpellID::FIRE_WALL)
									return true;
								if(s != SpellID::LAND_MINE)
									return false;
								const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
								auto bside = static_cast<bool>(side);
								return (side == so->casterSide) ? bside : !bside;
							}
						);
						expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
					}

					if(mask.test(EI(HexState::DAMAGING_R)))
					{
						auto damaging = anyobstacle(
							[side](const CObstacleInstance * o)
							{
								if(o->obstacleType == CObstacleInstance::MOAT)
									return true;
								if(!o->triggersEffects())
									return false;
								auto s = SpellID(o->ID);
								if(s == SpellID::FIRE_WALL)
									return true;
								if(s != SpellID::LAND_MINE)
									return false;
								const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
								auto bside = static_cast<bool>(side);
								return (side == so->casterSide) ? !bside : bside;
							}
						);
						expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
					}

					// if (mask.test(EI(HexState::GATE))) {
					//     expect(battle->battleGetSiegeLevel(), "HEX.STATE_MASK: GATE bit is set, but there is no fort");
					//     expect(bh == BattleHex::GATE_INNER || bh == BattleHex::GATE_OUTER, "HEX.STATE_MASK: GATE bit is set on bhex#%d", bh.toInt());
					// }
				}
				break;
				case HA::ACTION_MASK:
				{
					if(ended)
					{
						expect(v == 0, "HEX.ACTION_MASK: battle ended, but action mask is %d", v);
					}
					else
					{
						ensureCorrectMaskOrNA(bh, v, astack, battle, "HEX.ACTION_MASK");
					}
				}
				break;
				case HA::IS_REAR:
				{
					ensureValueMatch(v, cstack ? cstack->occupiedHex() == hex->bhex : 0, "HEX.IS_REAR");
				}
				break;
				case HA::STACK_SIDE:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return EI(cstack->unitSide());
						},
						"HA.STACK_SIDE"
					);
					// break; case HA::STACK_CREATURE_ID:
					//     ensureStackValueMatch(a, v, cstack->unitType()->getId(), "HEX.STACK_CREATURE_ID");
					break;
				case HA::STACK_SLOT:
				{
					if(!cstack)
						break;

					auto want = static_cast<int>(cstack->unitSlot());
					if(want == SlotID::WAR_MACHINES_SLOT)
						want = S13::STACK_SLOT_WARMACHINES;
					else if(want < 0 || want > 7)
						want = S13::STACK_SLOT_SPECIAL;

					ensureValueMatch(v, want, "HA.STACK_SLOT");
				}
				break;
				case HA::STACK_QUANTITY:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return std::round(S13::STACK_QTY_MAX * static_cast<float>(cstack->getCount()) / S13::STACK_QTY_MAX);
						},
						"HEX.STACK_QUANTITY"
					);
					break;
				case HA::STACK_ATTACK:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getAttack(false);
						},
						"HEX.STACK_ATTACK"
					);
					break;
				case HA::STACK_DEFENSE:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getDefense(false);
						},
						"HEX.STACK_DEFENSE"
					);
					break;
				case HA::STACK_SHOTS:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->shots.available();
						},
						"HEX.STACK_SHOTS"
					);
					break;
				case HA::STACK_DMG_MIN:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getMinDamage(false);
						},
						"HEX.STACK_DMG_MIN"
					);
					break;
				case HA::STACK_DMG_MAX:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getMaxDamage(false);
						},
						"HEX.STACK_DMG_MAX"
					);
					break;
				case HA::STACK_HP:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getMaxHealth();
						},
						"HEX.STACK_HP"
					);
					break;
				case HA::STACK_HP_LEFT:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getFirstHPleft();
						},
						"HEX.STACK_VALUE_REL"
					);
					break;
				case HA::STACK_SPEED:
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return cstack->getMovementRange();
						},
						"HEX.STACK_SPEED"
					);
					break;
				case HA::STACK_QUEUE:
				{
					// at battle end, queue is messed up
					// (the stack that dealt the killing blow is still "active", but not on 0 pos)
					if(ended || !cstack)
						break;

					auto qbits = std::bitset<S13::STACK_QUEUE_SIZE>(v);

					for(int i = 0; i < S13::STACK_QUEUE_SIZE; ++i)
					{
						int have = qbits.test(i);
						int want = (queue.at(i) == cstack);
						ensureValueMatch(have, want, ("HEX.STACK_QUEUE[" + std::to_string(i) + "]"));
					}
				}
				break;
				case HA::STACK_VALUE_ONE:
				{
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack]
						{
							return Stack::CalcValue(cstack->unitType());
						},
						"HEX.STACK_VALUE_ONE"
					);
				}
				break;
				case HA::STACK_VALUE_REL:
				{
					int tot = 0;
					for(auto & s : allstacks)
						tot += s->getCount() * Stack::CalcValue(s->unitType());

					// if (cstack) {
					//     std::cout << "Stack type: ";
					//     std::cout << cstack->unitType()->getNameSingularTextID();
					//     auto vv = 100.0 * cstack->getCount() * Stack::CalcValue(cstack->unitType()) / tot;
					//     std::cout << " Have: " << v << ", want: " << vv;
					//     std::cout << "\n";
					// }
					ensureStackNullOrMatch(
						attr,
						cstack,
						v,
						[&cstack, &tot]
						{
							return 1000ll * cstack->getCount() * Stack::CalcValue(cstack->unitType()) / tot;
						},
						"HEX.STACK_VALUE_REL"
					);
				}
				break;
				// These require historical information
				// (CPlayerCallback does not provide such)
				case HA::STACK_VALUE_REL0:
				case HA::STACK_VALUE_KILLED_REL:
				case HA::STACK_VALUE_KILLED_ACC_REL0:
				case HA::STACK_VALUE_LOST_REL:
				case HA::STACK_VALUE_LOST_ACC_REL0:
				case HA::STACK_DMG_DEALT_REL:
				case HA::STACK_DMG_DEALT_ACC_REL0:
				case HA::STACK_DMG_RECEIVED_REL:
				case HA::STACK_DMG_RECEIVED_ACC_REL0:
					break;
				case HA::STACK_FLAGS1:
				{
					if(!isNA(v, cstack, "HEX.STACK_FLAGS"))
					{
						for(int j = 0; j < EI(StackFlag1::_count); j++)
						{
							auto f = StackFlag1(j);
							auto vf = hex->stack->flag(f);

							switch(f)
							{
								case SF1::IS_ACTIVE:
									// at battle end, queue is messed up
									// (the stack that dealt the killing blow is still "active", but not on 0 pos)
									if(ended)
										break;

									if(vf == 0)
										expect(cstack != astack, "HEX.STACK_FLAGS1.IS_ACTIVE: =0 but cstack == astack");
									else
										expect(cstack == astack, "HEX.STACK_FLAGS1.IS_ACTIVE: =%d but cstack != astack", vf);
									break;
								case SF1::WILL_ACT:
									ensureValueMatch(vf, cstack->willMove(), "HEX.STACK_FLAGS1.WILL_ACT");
									break;
								case SF1::CAN_WAIT:
									ensureValueMatch(vf, cstack->willMove() && !cstack->waitedThisTurn, "HEX.STACK_FLAGS1.CAN_WAIT");
									break;
								case SF1::CAN_RETALIATE:
									// XXX: ableToRetaliate() calls CAmmo's (i.e. CRetaliations's) canUse() method
									//      which takes into account relevant bonuses (e.g. NO_RETALIATION from expert Blind)
									//      It does *NOT* take into account the attacker's BLOCKS_RETALIATION bonus
									//      (if it did, what would be the correct CAN_RETALIATE value for friendly units?)
									ensureValueMatch(vf, cstack->ableToRetaliate(), "HEX.STACK_FLAGS1.CAN_RETALIATE");
									break;
								case SF1::SLEEPING:
									cstack->unitType()->getId() == CreatureID::AMMO_CART
										? ensureValueMatch(vf, false, "HEX.STACK_FLAGS1.SLEEPING")
										: ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::NOT_ACTIVE), "HEX.STACK_FLAGS1.SLEEPING");
									break;
								case SF1::BLOCKED:
									ensureValueMatch(vf, cstack->canShoot() && battle->battleIsUnitBlocked(cstack), "HEX.STACK_FLAGS1.TWO_HEX_ATTACK_BREATH");
									break;
								case SF1::BLOCKING:
								{
									auto want = 0;
									for(auto & adjstack : battle->battleAdjacentUnits(cstack))
									{
										if(adjstack->unitSide() != cstack->unitSide() && adjstack->canShoot() && battle->battleIsUnitBlocked(adjstack)
										   && !adjstack->hasBonusOfType(BonusType::FREE_SHOOTING) && !adjstack->hasBonusOfType(BonusType::SIEGE_WEAPON))
										{
											want = 1;
											break;
										}
									}
									ensureValueMatch(vf, want, "HEX.STACK_FLAGS1.BLOCKING");
								}
								break;
								case SF1::IS_WIDE:
									ensureValueMatch(vf, cstack->occupiedHex().isAvailable(), "HEX.STACK_FLAGS1.IS_WIDE");
									break;
								case SF1::FLYING:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::FLYING), "HEX.STACK_FLAGS1.FLYING");
									break;
								case SF1::ADDITIONAL_ATTACK:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::ADDITIONAL_ATTACK), "HEX.STACK_FLAGS1.ADDITIONAL_ATTACK");
									break;
								case SF1::NO_MELEE_PENALTY:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "HEX.STACK_FLAGS1.NO_MELEE_PENALTY");
									break;
								case SF1::TWO_HEX_ATTACK_BREATH:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "HEX.STACK_FLAGS1.TWO_HEX_ATTACK_BREATH");
									break;
								case SF1::BLOCKS_RETALIATION:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "HEX.STACK_FLAGS1.BLOCKS_RETALIATION");
									break;
								case SF1::SHOOTER:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::SHOOTER), "HEX.STACK_FLAGS1.SHOOTER");
									// canShoot returns false if ammo = 0
									// ensureValueMatch(vf, cstack->canShoot(), "HEX.STACK_FLAGS1.SHOOTER (canShoot)");
									break;
								case SF1::NON_LIVING:
								{
									auto undead = cstack->hasBonusOfType(BonusType::UNDEAD);
									auto nonliving = cstack->hasBonusOfType(BonusType::NON_LIVING);
									ensureValueMatch(vf, undead || nonliving, "HEX.STACK_FLAGS1.NON_LIVING", cstack->getDescription());
								}
								break;
								case SF1::WAR_MACHINE:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::SIEGE_WEAPON), "HEX.STACK_FLAGS1.WAR_MACHINE");
									break;
								case SF1::FIREBALL:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::FIREBALL)), "HEX.STACK_FLAGS1.FIREBALL"
									);
									break;
								case SF1::DEATH_CLOUD:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::DEATH_CLOUD)), "HEX.STACK_FLAGS1.DEATH_CLOUD"
									);
									break;
								case SF1::THREE_HEADED_ATTACK:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::THREE_HEADED_ATTACK), "HEX.STACK_FLAGS1.THREE_HEADED_ATTACK");
									break;
								case SF1::ALL_AROUND_ATTACK:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::ATTACKS_ALL_ADJACENT), "HEX.STACK_FLAGS1.ALL_AROUND_ATTACK");
									break;
								case SF1::RETURN_AFTER_STRIKE:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::RETURN_AFTER_STRIKE), "HEX.STACK_FLAGS1.RETURN_AFTER_STRIKE");
									break;
								case SF1::ENEMY_DEFENCE_REDUCTION:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::ENEMY_DEFENCE_REDUCTION), "HEX.STACK_FLAGS1.ENEMY_DEFENCE_REDUCTION"
									);
									break;
								case SF1::LIFE_DRAIN:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::LIFE_DRAIN), "HEX.STACK_FLAGS1.LIFE_DRAIN");
									break;
								case SF1::DOUBLE_DAMAGE_CHANCE:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::DOUBLE_DAMAGE_CHANCE), "HEX.STACK_FLAGS1.DOUBLE_DAMAGE_CHANCE");
									break;
								case SF1::DEATH_STARE:
									ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::DEATH_STARE), "HEX.STACK_FLAGS1.DEATH_STARE");
									break;
								default:
									THROW_FORMAT("Unexpected StackFlag: %d", EI(f));
							}
						}
					}
				}
				break;
				case HA::STACK_FLAGS2:
				{
					if(!isNA(v, cstack, "HEX.STACK_FLAGS"))
					{
						for(int j = 0; j < EI(StackFlag2::_count); j++)
						{
							auto f = StackFlag2(j);
							auto vf = hex->stack->flag(f);

							switch(f)
							{
								case SF2::AGE:
									ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE");
									break;
								case SF2::AGE_ATTACK:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE_ATTACK"
									);
									break;
								case SF2::BIND:
									ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND");
									break;
								case SF2::BIND_ATTACK:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND_ATTACK"
									);
									break;
								case SF2::BLIND:
								{
									auto blind = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BLIND));
									auto paralyzed = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::PARALYZE));
									ensureValueMatch(vf, blind || paralyzed, "HEX.STACK_FLAGS2.BLIND");
								}
								break;
								case SF2::BLIND_ATTACK:
								{
									auto blind = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BLIND));
									auto paralyze = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::PARALYZE));
									ensureValueMatch(vf, blind || paralyze, "HEX.STACK_FLAGS2.BLIND_ATTACK");
								}
								break;
								case SF2::CURSE:
									ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE");
									break;
								case SF2::CURSE_ATTACK:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE_ATTACK"
									);
									break;
								case SF2::DISPEL_ATTACK:
									ensureValueMatch(
										vf,
										cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::DISPEL_HELPFUL_SPELLS)),
										"HEX.STACK_FLAGS2.DISPEL_ATTACK"
									);
									break;
								case SF2::PETRIFY:
									ensureValueMatch(
										vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::STONE_GAZE)), "HEX.STACK_FLAGS2.PETRIFY"
									);
									break;
								case SF2::PETRIFY_ATTACK:
									ensureValueMatch(
										vf,
										cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::STONE_GAZE)),
										"HEX.STACK_FLAGS2.PETRIFY_ATTACK"
									);
									break;
								case SF2::POISON:
									ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON");
									break;
								case SF2::POISON_ATTACK:
									ensureValueMatch(
										vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON_ATTACK"
									);
									break;
								case SF2::WEAKNESS:
									ensureValueMatch(
										vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::WEAKNESS)), "HEX.STACK_FLAGS2.WEAKNESS"
									);
									break;
								case SF2::WEAKNESS_ATTACK:
									ensureValueMatch(
										vf,
										cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::WEAKNESS)),
										"HEX.STACK_FLAGS2.WEAKNESS_ATTACK"
									);
									break;
								default:
									THROW_FORMAT("Unexpected StackFlag2: %d", EI(f));
							}
						}
					}
				}
				break;
				default:
					THROW_FORMAT("Unexpected HexAttribute: %d", EI(attr));
			}
		}
	}

	// Mask is undefined at battle end
	if(ended)
		return;
}

// This intentionally uses the IState interface to ensure that
// the schema is properly exposing all needed informaton
std::string Render(const Schema::IState * istate, const Action * action) // NOSONAR - function used for debugging only
{
	// auto bfstate = istate->getBattlefieldState();
	auto supdata_ = istate->getSupplementaryData();
	expect(supdata_.has_value(), "supdata_ holds no value");
	expect(supdata_.type() == typeid(const ISupplementaryData *), "supdata_ of unexpected type");
	const auto * sup = std::any_cast<const ISupplementaryData *>(supdata_);
	expect(sup, "sup holds a nullptr");
	const auto * gstats = sup->getGlobalStats();
	const auto * lpstats = sup->getLeftPlayerStats();
	const auto * rpstats = sup->getRightPlayerStats();
	const auto * mystats = gstats->getAttr(GA::BATTLE_SIDE) ? rpstats : lpstats;
	auto hexes = sup->getHexes();
	auto alogs = sup->getAttackLogs();

	const IStack * astack = nullptr;

	// find an active hex (i.e. with active stack on it)
	for(auto & row : hexes)
	{
		for(auto & hex : row)
		{
			const auto * const stack = hex->getStack();
			if(stack && stack->getFlag(SF1::IS_ACTIVE))
			{
				expect(!astack || astack == stack, "two active stacks found");
				astack = stack;
			}
		}
	}

	auto ended = gstats->getAttr(GA::BATTLE_WINNER) != S13::NULL_VALUE_UNENCODED;

	if(!astack && !ended)
		logAi->error("could not find an active stack (battle has not ended).");

	std::string nocol = "\033[0m";
	std::string redcol = "\033[31m"; // red
	std::string bluecol = "\033[34m"; // blue
	std::string darkcol = "\033[90m";
	std::string activemod = "\033[107m\033[7m"; // bold+reversed
	std::string ukncol = "\033[7m"; // white

	std::vector<std::stringstream> lines;

	//
	// 1. Add logs table:
	//
	// #1 attacks #5 for 16 dmg (1 killed)
	// #5 attacks #1 for 4 dmg (0 killed)
	// ...
	//
	for(auto & alog : alogs)
	{
		auto row = std::stringstream();
		auto attcol = ukncol;
		auto attalias = '?';
		auto defcol = ukncol;
		auto defalias = '?';

		if(alog->getAttacker())
		{
			attcol = (alog->getAttacker()->getAttr(SA::SIDE) == 0) ? redcol : bluecol;
			attalias = alog->getAttacker()->getAlias();
		}

		if(alog->getDefender())
		{
			defcol = (alog->getDefender()->getAttr(SA::SIDE) == 0) ? redcol : bluecol;
			defalias = alog->getDefender()->getAlias();
		}

		row << attcol << "#" << attalias << nocol;
		row << " attacks ";
		row << defcol << "#" << defalias << nocol;
		row << " for " << alog->getDamageDealt() << " dmg";
		row << " (kills: " << alog->getUnitsKilled() << ", value: " << alog->getValueKilled() << " / " << alog->getValueKilledPermille() << "‰)";

		lines.push_back(std::move(row));
	}

	//
	// 2. Build ASCII table
	//    (+populate aliveStacks var)
	//    NOTE: the contents below look mis-aligned in some editors.
	//          In (my) terminal, it all looks correct though.
	//
	//   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
	//  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃
	// ¹┨  1 ◌ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 1 ┠¹
	// ²┨ ◌ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠²
	// ³┨  ◌ ○ ○ ○ ○ ○ ○ ◌ ▦ ▦ ◌ ◌ ◌ ◌ ◌ ┠³
	// ⁴┨ ◌ ○ ○ ○ ○ ○ ○ ○ ▦ ▦ ▦ ◌ ◌ ◌ ◌  ┠⁴
	// ⁵┨  2 ◌ ○ ○ ▦ ▦ ◌ ○ ◌ ◌ ◌ ◌ ◌ ◌ 2 ┠⁵
	// ⁶┨ ◌ ○ ○ ○ ▦ ▦ ◌ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁶
	// ⁷┨  3 3 ○ ○ ○ ▦ ◌ ○ ○ ◌ ◌ ▦ ◌ ◌ 3 ┠⁷
	// ⁸┨ ◌ ○ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌  ┠⁸
	// ⁹┨  ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠⁹
	// ⁰┨ ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁰
	// ¹┨  4 ◌ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 4 ┠¹
	//  ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃
	//   ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕
	//

	// s=special; can be any number, slot is always 7 (SPECIAL), visualized A,B,C...

	auto tablestartrow = lines.size();

	lines.emplace_back() << "    ₀▏₁▏₂▏₃▏₄▏₅▏₆▏₇▏₈▏₉▏₀▏₁▏₂▏₃▏₄";
	lines.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

	static std::array<std::string, 10> nummap{"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

	bool addspace = true;

	auto seenstacks = std::map<const IStack *, IHex *>{};
	bool divlines = true;

	// y even "▏"
	// y odd "▕"

	for(int y = 0; y < 11; y++)
	{
		for(int x = 0; x < 15; x++)
		{
			auto sym = std::string("?");
			auto & hex = hexes.at(y).at(x);
			const auto * stack = hex->getStack();

			auto & row = (x == 0) ? (lines.emplace_back() << nummap.at(y % 10) << "┨" << (y % 2 == 0 ? " " : "")) : lines.back();

			if(addspace)
			{
				if(divlines && (x != 0))
				{
					row << darkcol << (y % 2 == 0 ? "▏" : "▕") << nocol;
				}
				else
				{
					row << " ";
				}
			}

			addspace = true;

			auto smask = HexStateMask(hex->getAttr(HA::STATE_MASK));
			auto col = nocol;

			// First put symbols based on hex state.
			// If there's a stack on this hex, symbol will be overriden.
			HexStateMask mpass = 1 << EI(HexState::PASSABLE);
			HexStateMask mstop = 1 << EI(HexState::STOPPING);
			HexStateMask mdmgl = 1 << EI(HexState::DAMAGING_L);
			HexStateMask mdmgr = 1 << EI(HexState::DAMAGING_R);
			HexStateMask mdefault = 0; // or mother :)

			std::vector<std::tuple<std::string, std::string, HexStateMask>> symbols{
				{"⨻", bluecol, mpass | mstop | mdmgl},
				{"⨻", redcol,  mpass | mstop | mdmgr},
				{"✶", bluecol, mpass | mdmgl        },
				{"✶", redcol,  mpass | mdmgr        },
				{"△", nocol,   mpass | mstop        },
				{"○", nocol,   mpass                }, // changed to "◌" if unreachable
				{"◼", nocol,   mdefault             }
			};

			for(auto & tuple : symbols)
			{
				const auto & [s, c, m] = tuple;
				if((smask & m) == m)
				{
					sym = s;
					col = c;
					break;
				}
			}

			auto amask = HexActMask(hex->getAttr(HA::ACTION_MASK));
			if(col == nocol && !amask.test(EI(HexAction::MOVE)))
			{ // || supdata->getIsBattleEnded()
				col = darkcol;
				sym = sym == "○" ? "◌" : sym;
			}

			if(stack)
			{
				auto seen = seenstacks.find(stack) != seenstacks.end();
				// MSVC mandates constexpr `n` here
				constexpr int n = std::get<2>(S13::HEX_ENCODING[EI(HA::STACK_FLAGS1)]);
				auto flags = std::bitset<n>(stack->getAttr(SA::FLAGS1));
				sym = std::string(1, stack->getAlias());
				col = stack->getAttr(SA::SIDE) ? bluecol : redcol;

				if((stack->getAttr(SA::QUEUE) & 1)) // && !supdata->getIsBattleEnded()
					col += activemod;

				if(flags.test(EI(SF1::IS_WIDE)) && !seen)
				{
					if(stack->getAttr(SA::SIDE) == 0)
					{
						sym += "↠";
						addspace = false;
					}
					else if(stack->getAttr(SA::SIDE) == 1 && hex->getAttr(HA::X_COORD) < 14)
					{
						sym += "↞";
						addspace = false;
					}
				}

				if(!seen)
					seenstacks.insert({stack, hex});
			}

			row << col << sym << nocol;

			if(x == 15 - 1)
			{
				row << (y % 2 == 0 ? " " : "  ") << "┠" << nummap.at(y % 10);
			}
		}
	}

	lines.emplace_back() << " ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃";
	lines.emplace_back() << "   ⁰▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴";

	//
	// 3. Add side table stuff
	//
	//   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
	//  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃         Player: RED
	// ₁┨  ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠₁    Last action:
	// ₂┨ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠₂      DMG dealt: 0
	// ₃┨  1 ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌ ◌ ◌ 1 ┠₃   Units killed: 0
	// ...

	for(int i = 0; i <= lines.size(); i++)
	{
		std::string name;
		std::string value;
		auto side = gstats->getAttr(GA::BATTLE_SIDE);

		switch(i)
		{
			case 1:
				name = "Player";
				if(ended)
					value = "";
				else
					value = side ? bluecol + "BLUE" + nocol : redcol + "RED" + nocol;
				break;
			case 2:
				name = "Last action";
				value = action ? action->name() + " [" + std::to_string(action->action) + "]" : "";
				break;
			case 3:
				name = "DMG dealt";
				value = boost::str(boost::format("%d (%d since start)") % mystats->getAttr(PA::DMG_DEALT_NOW_ABS) % mystats->getAttr(PA::DMG_DEALT_ACC_ABS));
				break;
			case 4:
				name = "DMG received";
				value =
					boost::str(boost::format("%d (%d since start)") % mystats->getAttr(PA::DMG_RECEIVED_NOW_ABS) % mystats->getAttr(PA::DMG_RECEIVED_ACC_ABS));
				break;
			case 5:
				name = "Value killed";
				value =
					boost::str(boost::format("%d (%d since start)") % mystats->getAttr(PA::VALUE_KILLED_NOW_ABS) % mystats->getAttr(PA::VALUE_KILLED_ACC_ABS));
				break;
			case 6:
				name = "Value lost";
				value = boost::str(boost::format("%d (%d since start)") % mystats->getAttr(PA::VALUE_LOST_NOW_ABS) % mystats->getAttr(PA::VALUE_LOST_ACC_ABS));
				break;
			case 7:
			{
				// XXX: if there's a draw, this text will be incorrect
				auto restext = gstats->getAttr(GA::BATTLE_WINNER) ? (bluecol + "BLUE WINS") : (redcol + "RED WINS");

				name = "Battle result";
				value = ended ? (restext + nocol) : "";
			}
			break;
			case 8:
				name = "Army value (L)";
				value = boost::str(
					boost::format("%d (%.0f‰ of current BF value)") % lpstats->getAttr(PA::ARMY_VALUE_NOW_ABS) % lpstats->getAttr(PA::ARMY_VALUE_NOW_REL)
				);
				break;
			case 9:
				name = "Army value (R)";
				value = boost::str(
					boost::format("%d (%.0f‰ of current BF value)") % rpstats->getAttr(PA::ARMY_VALUE_NOW_ABS) % rpstats->getAttr(PA::ARMY_VALUE_NOW_REL)
				);
				break;
			case 10:
				name = "Current BF value";
				value = boost::str(
					boost::format("%d (%.0f‰ of starting BF value)") % gstats->getAttr(GA::BFIELD_VALUE_NOW_ABS) % gstats->getAttr(GA::BFIELD_VALUE_NOW_REL0)
				);
				break;
			default:
				continue;
		}

		lines.at(tablestartrow + i) << PadLeft(name, 17, ' ') << ": " << value;
	}

	lines.emplace_back() << "";

	//
	// 5. Add stacks table:
	//
	//          Stack # |   0   1   2   3   4   5   6   A   B   C   0   1   2   3   4   5   6   A   B   C
	// -----------------+--------------------------------------------------------------------------------
	//              Qty |   0  34   0   0   0   0   0   0   0   0   0  17   0   0   0   0   0   0   0   0
	//           Attack |   0   8   0   0   0   0   0   0   0   0   0   6   0   0   0   0   0   0   0   0
	//    ...10 more... | ...
	// -----------------+--------------------------------------------------------------------------------
	//
	// table with 24 columns (1 header, 3 dividers, 10 stacks per side)
	// Each row represents a separate attribute

	using RowDef = std::tuple<StackAttribute, std::string>;

	// max to show
	constexpr int max_stacks_per_side = 10;

	// All cell text is aligned right
	auto colwidths = std::array<int, 4 + (2 * max_stacks_per_side)>{};
	colwidths.fill(5); // default col width
	colwidths.at(0) = 16; // header col

	// Divider column indexes
	auto divcolids = {1, max_stacks_per_side + 2, (2 * max_stacks_per_side) + 3};

	for(int i : divcolids)
		colwidths.at(i) = 2; // divider col

	// {Attribute, name, colwidth}
	const auto rowdefs = std::vector<RowDef>{
		RowDef{SA::FLAGS1,    "Stack #"         }, // stack alias (1..7, S or M)
		RowDef{SA::SIDE,      ""                }, // divider row
		RowDef{SA::QUANTITY,  "Qty"             },
		RowDef{SA::ATTACK,    "Attack"          },
		RowDef{SA::DEFENSE,   "Defense"         },
		RowDef{SA::SHOTS,     "Shots"           },
		RowDef{SA::DMG_MIN,   "Dmg (min)"       },
		RowDef{SA::DMG_MAX,   "Dmg (max)"       },
		RowDef{SA::HP,        "HP"              },
		RowDef{SA::HP_LEFT,   "HP left"         },
		RowDef{SA::SPEED,     "Speed"           },
		RowDef{SA::QUEUE,     "Queue"           },
		RowDef{SA::VALUE_ONE, "Value (one)"     },
		RowDef{SA::VALUE_REL, "       Value (‰)"}, // manually pad to 16 (unicode length issue)
		// RowDef{SA::ESTIMATED_DMG, "Est. DMG%"},
		RowDef{SA::FLAGS1,    "State"           }, // "WAR" = CAN_WAIT, WILL_ACT, CAN_RETAL
		RowDef{SA::FLAGS1,    "Attack mods"     }, // "DB" = Double, Blinding
		// 2 values per column to avoid too long table
		RowDef{SA::FLAGS1,    "Blocked/ing"     },
		RowDef{SA::FLAGS1,    "Fly/Sleep"       },
		RowDef{SA::FLAGS1,    "NoRetal/NoMelee" },
		RowDef{SA::FLAGS1,    "Wide/Breath"     },
		RowDef{SA::SIDE,      ""                }, // divider row
	};

	// Table with nrows and ncells, each cell a 3-element tuple
	// cell: color, width, txt
	using TableCell = std::tuple<std::string, int, std::string>;
	using TableRow = std::array<TableCell, colwidths.size()>;

	auto table = std::vector<TableRow>{};

	auto divrow = TableRow{};
	for(int i = 0; i < colwidths.size(); i++)
		divrow[i] = {nocol, colwidths.at(i), std::string(colwidths.at(i), '-')};

	for(int i : divcolids)
		divrow.at(i) = {nocol, colwidths.at(i), std::string(colwidths.at(i) - 1, '-') + "+"};

	int specialcounter = 0;

	// Attribute rows
	for(const auto & [a, aname] : rowdefs)
	{
		if(a == SA::SIDE)
		{ // divider row
			table.push_back(divrow);
			continue;
		}

		auto row = TableRow{};

		// Header col
		row.at(0) = {nocol, colwidths.at(0), aname};

		// Div cols
		for(int i : {1, 2 + max_stacks_per_side, static_cast<int>(colwidths.size() - 1)})
			row.at(i) = {nocol, colwidths.at(i), "|"};

		// Stack cols
		for(auto side : {0, 1})
		{
			auto sidestacks = std::array<std::pair<const IStack *, IHex *>, max_stacks_per_side>{};
			auto extracounter = 0;
			for(const auto & [stack, hex] : seenstacks)
			{
				if(stack->getAttr(SA::SIDE) == side)
				{
					int slot = stack->getAlias() >= '0' && stack->getAlias() <= '6' ? stack->getAlias() - '0' : 7 + extracounter;

					if(slot < max_stacks_per_side)
						sidestacks.at(slot) = {stack, hex};

					if(slot >= 7)
						extracounter += 1;
				}
			}

			for(int i = 0; i < sidestacks.size(); ++i)
			{
				auto & [stack, hex] = sidestacks.at(i);
				auto color = nocol;
				std::string value;

				if(stack)
				{
					// MSVC mandates constexpr `n` here
					constexpr int n1 = std::get<2>(S13::HEX_ENCODING[EI(HA::STACK_FLAGS1)]);
					constexpr int n2 = std::get<2>(S13::HEX_ENCODING[EI(HA::STACK_FLAGS2)]);
					auto flags1 = std::bitset<n1>(stack->getAttr(SA::FLAGS1));
					auto flags2 = std::bitset<n2>(stack->getAttr(SA::FLAGS2));

					color = stack->getAttr(SA::SIDE) ? bluecol : redcol;
					// } else if (a == SA::ACTSTATE) {
					//     switch(StackActState(stack->getAttr(a))) {
					//     break; case StackActState::READY: value = "R";
					//     break; case StackActState::WAITING: value = "W";
					//     break; case StackActState::DONE: value = "D";
					//     break; default:
					//         throw std::runtime_error("Unexpected actstate:: " + std::to_string(EI(stack->getAttr(a))));
					//     }
					if(a == SA::QUEUE)
					{
						auto qbits = std::bitset<S13::STACK_QUEUE_SIZE>(stack->getAttr(SA::QUEUE));
						for(int i = 0; i < S13::STACK_QUEUE_SIZE; ++i)
						{
							if(qbits.test(i))
							{
								value = std::to_string(i);
								break;
							}
						}
					}
					else if(a == SA::VALUE_ONE && stack->getAttr(a) >= 1000)
					{
						std::ostringstream oss;
						oss << std::fixed << std::setprecision(1) << (stack->getAttr(a) / 1000.0);
						value = oss.str();
						value[value.size() - 2] = 'k';
						if(value.rfind("K0") == (value.size() - 2))
							value.resize(value.size() - 1);
					}
					else if(a == SA::FLAGS1)
					{
						auto fmt = boost::format("%d/%d");

						// Y_COORD, "State"},  // "WAR" = CAN_WAIT, WILL_ACT, CAN_RETAL
						// Y_COORD, "Attack type"}, // "DB" = Double strike, Blind-like
						// Y_COORD, "Blocked/ing"},
						// Y_COORD, "Fly/Sleep"},
						// Y_COORD, "No Retal/Melee"},
						// Y_COORD, "Wide/Breath"},
						switch(specialcounter)
						{
							case 0:
								value = std::string(1, stack->getAlias());
								break;
							case 1:
							{
								value = std::string("");
								value += flags1.test(EI(SF1::CAN_WAIT)) ? "W" : " ";
								value += flags1.test(EI(SF1::WILL_ACT)) ? "A" : " ";
								value += flags1.test(EI(SF1::CAN_RETALIATE)) ? "R" : " ";
							}
							break;
							case 2:
							{
								value = std::string("");
								value += flags1.test(EI(SF1::ADDITIONAL_ATTACK)) ? "D" : " ";
								value += flags2.test(EI(SF2::BLIND_ATTACK)) ? "B" : " ";
							}
							break;
							case 3:
								value = boost::str(fmt % flags1.test(EI(SF1::BLOCKED)) % flags1.test(EI(SF1::BLOCKING)));
								break;
							case 4:
								value = boost::str(fmt % flags1.test(EI(SF1::FLYING)) % flags1.test(EI(SF1::SLEEPING)));
								break;
							case 5:
								value = boost::str(fmt % flags1.test(EI(SF1::BLOCKS_RETALIATION)) % flags1.test(EI(SF1::NO_MELEE_PENALTY)));
								break;
							case 6:
								value = boost::str(fmt % flags1.test(EI(SF1::IS_WIDE)) % flags1.test(EI(SF1::TWO_HEX_ATTACK_BREATH)));
								break;
							default:
								THROW_FORMAT("Unexpected specialcounter: %d", specialcounter);
						}
					}
					else
					{
						value = std::to_string(stack->getAttr(a));
					}

					if((stack->getAttr(SA::QUEUE) & 1) && !ended)
						color += activemod;
				}

				auto colid = 2 + i + side + (max_stacks_per_side * side);
				row.at(colid) = {color, colwidths.at(colid), value};
			}
		}

		if(a == SA::FLAGS1)
			++specialcounter;

		table.push_back(row);
	}

	for(auto & r : table)
	{
		auto line = std::stringstream();
		for(auto & [color, width, txt] : r)
			line << color << PadLeft(txt, width, ' ') << nocol;

		lines.push_back(std::move(line));
	}

	//
	// 7. Join rows into a single string
	//
	std::string res = lines[0].str();
	for(int i = 1; i < lines.size(); i++)
		res += "\n" + lines[i].str();

	return res;
}
}
