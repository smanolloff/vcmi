/*
 * hex.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/nodes/unit.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "battle/CObstacleInstance.h"
#include "battle/ReachabilityInfo.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include "vcmi/spells/Service.h"
#include "vcmi/spells/Spell.h"

#include "AI/MMAI/common.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using HA = S15::Graph::NodeAttributes::Hex;
using HS = S15::HexState;
using UA = S15::Graph::NodeAttributes::Unit;
using HexAction = S15::HexAction;

using HexActionMask = std::bitset<EU(HexAction::_count)>;
using HexStateMask = std::bitset<EU(HS::_count)>;
using HexActionHex = std::array<BattleHex, 12>;

struct ActiveStackInfo
{
	const Unit * stack;
	const bool canshoot;
	const std::shared_ptr<ReachabilityInfo> rinfo;

	ActiveStackInfo(const Unit * stack_, bool canshoot_, const std::shared_ptr<ReachabilityInfo> & rinfo_)
		: stack(stack_)
		, canshoot(canshoot_)
		, rinfo(rinfo_)
	{
	}
};

class Hex : public Element<S15::EncodingTraits<Schema::V15::HexEncoding>, S15::Graph::INode>
{
public:
	static int CalcId(const BattleHex & bh)
	{
		ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.toInt()));
		return bh.getX() - 1 + (bh.getY() * 15);
	}

	static std::pair<int, int> CalcXY(const BattleHex & bh)
	{
		return {bh.getX() - 1, bh.getY()};
	}

	static HexActionHex NearbyBattleHexes(const BattleHex & bh)
	{
		static_assert(EU(HexAction::AMOVE_TR) == 0);
		static_assert(EU(HexAction::AMOVE_R) == 1);
		static_assert(EU(HexAction::AMOVE_BR) == 2);
		static_assert(EU(HexAction::AMOVE_BL) == 3);
		static_assert(EU(HexAction::AMOVE_L) == 4);
		static_assert(EU(HexAction::AMOVE_TL) == 5);
		static_assert(EU(HexAction::AMOVE_2TR) == 6);
		static_assert(EU(HexAction::AMOVE_2R) == 7);
		static_assert(EU(HexAction::AMOVE_2BR) == 8);
		static_assert(EU(HexAction::AMOVE_2BL) == 9);
		static_assert(EU(HexAction::AMOVE_2L) == 10);
		static_assert(EU(HexAction::AMOVE_2TL) == 11);

		auto nbhR = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
		auto nbhL = bh.cloneInDirection(BattleHex::EDir::LEFT, false);

		return HexActionHex{
			bh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
			nbhR,
			bh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),
			bh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
			nbhL,
			bh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false),
			nbhR.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
			nbhR.cloneInDirection(BattleHex::EDir::RIGHT, false),
			nbhR.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),
			nbhL.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
			nbhL.cloneInDirection(BattleHex::EDir::LEFT, false),
			nbhL.cloneInDirection(BattleHex::EDir::TOP_LEFT, false)
		};
	}

	Hex(
		const BattleHex & bhex_,
		EAccessibility accessibility,
		const std::vector<std::shared_ptr<const CObstacleInstance>> & obstacles,
		const std::map<BattleHex, std::shared_ptr<Unit>> & hexstacks,
		const std::shared_ptr<ActiveStackInfo> & astackinfo,
		bool isRUFR_,
		int wallHP,
		bool isGateOpen
	)
		: bhex(bhex_)
		, id(CalcId(bhex_))
		, isRUFR(isRUFR_)
		, moveDestHex(isRUFR_ ? frontOf(bhex_, astackinfo) : bhex_)
	{
		attrs.fill(S15::NULL_VALUE_UNENCODED);

		auto [x, y] = CalcXY(bhex);
		auto it = hexstacks.find(bhex);
		stack = it == hexstacks.end() ? nullptr : it->second;

		setattr(HA::Y_COORD, y);
		setattr(HA::X_COORD, x);
		setattr(HA::IS_REAR, stack && bhex == stack->cstack->occupiedHex());
		setattr(HA::IS_RUFR, isRUFR);
		setattr(HA::WALL_HEALTH, wallHP);

		if(astackinfo)
		{
			setStateMask(accessibility, obstacles, astackinfo->stack->cstack->unitSide(), isGateOpen);
			setActionMask(astackinfo, hexstacks);
		}
		else
		{
			setStateMask(accessibility, obstacles, BattleSide::ATTACKER, isGateOpen);
		}

		finalize();
	}

    int nodeIndex() const override {
        return id;  // always exactly 165 Hex nodes
    }

	const Unit * getStack() const
	{
		return stack.get();
	}

	std::string name() const
	{
		return "(" + std::to_string(attr(HA::Y_COORD)) + "," + std::to_string(attr(HA::X_COORD)) + ")";
	}

	void finalize()
	{
		attrs.at(EU(HA::ACTION_MASK)) = static_cast<int>(actmask.to_ulong());
		attrs.at(EU(HA::STATE_MASK)) = static_cast<int>(statemask.to_ulong());
	}

	const BattleHex bhex;
	const int id;
	std::shared_ptr<const Unit> stack = nullptr;
	std::array<int, EU(HA::_count)> attrs = {};
	HexActionMask actmask = 0;
	HexStateMask statemask = 0;
	bool isRUFR = false;
	const BattleHex moveDestHex;

private:
	static BattleHex frontOf(const BattleHex & bhex, const std::shared_ptr<ActiveStackInfo> & astackinfo)
	{
		if(!astackinfo)
			return bhex;

		const auto attacker = astackinfo->stack->cstack->unitSide() == BattleSide::ATTACKER;
		return bhex.cloneInDirection(attacker ? BattleHex::RIGHT : BattleHex::LEFT, true);
	}

	void setattr(HA a, int value)
	{
		attrs.at(EU(a)) = value;
	}

	void setStateMask(
		EAccessibility accessibility,
		const std::vector<std::shared_ptr<const CObstacleInstance>> & obstacles,
		BattleSide side,
		bool isGateOpen
	)
	{
		for(const auto & obstacle : obstacles)
		{
			switch(obstacle->obstacleType)
			{
				case CObstacleInstance::USUAL:
				case CObstacleInstance::ABSOLUTE_OBSTACLE:
					statemask.reset(EU(HS::PASSABLE));
					break;
				case CObstacleInstance::MOAT:
					if(!(bhex == BattleHex::GATE_BRIDGE && isGateOpen))
					{
						statemask.set(EU(HS::STOPPING));
						statemask.set(EU(HS::DAMAGING_L));
						statemask.set(EU(HS::DAMAGING_R));
					}
					break;
				case CObstacleInstance::SPELL_CREATED:
					switch(SpellID(obstacle->ID))
					{
						case SpellID::QUICKSAND:
							statemask.set(EU(HS::STOPPING));
							break;
						case SpellID::LAND_MINE:
						{
							auto casterSide = dynamic_cast<const SpellCreatedObstacle *>(obstacle.get())->casterSide;
							if(side == casterSide)
								statemask.set(EU(side == BattleSide::DEFENDER ? HS::DAMAGING_L : HS::DAMAGING_R));
							else
								statemask.set(EU(side == BattleSide::DEFENDER ? HS::DAMAGING_R : HS::DAMAGING_L));
							break;
						}
						default:
							break;
					}
					break;
				default:
					THROW_FORMAT("Unexpected obstacle type: %d", EU(obstacle->obstacleType));
			}
		}

		switch(accessibility)
		{
			case EAccessibility::ACCESSIBLE:
				ASSERT(!stack, "accessibility is ACCESSIBLE, but a stack was found on hex");
				statemask.set(EU(HS::PASSABLE));
				break;
			case EAccessibility::OBSTACLE:
			case EAccessibility::ALIVE_STACK:
			case EAccessibility::DESTRUCTIBLE_WALL:
			case EAccessibility::UNAVAILABLE:
				statemask.reset(EU(HS::PASSABLE));
				break;
			case EAccessibility::GATE:
				side == BattleSide::DEFENDER ? statemask.set(EU(HS::PASSABLE)) : statemask.reset(EU(HS::PASSABLE));
				break;
			default:
				THROW_FORMAT("Unexpected hex accessibility for bhex %d: %d", bhex.toInt() % EU(accessibility));
		}

		if(bhex == BattleHex::GATE_INNER || bhex == BattleHex::GATE_OUTER)
			statemask.set(EU(HS::SIEGE_GATE));
		else if(bhex == BattleHex::GATE_BRIDGE)
			statemask.set(EU(HS::SIEGE_BRIDGE));
		else if(bhex == BattleHex::DESTRUCTIBLE_WALL_1
			|| bhex == BattleHex::DESTRUCTIBLE_WALL_2
			|| bhex == BattleHex::DESTRUCTIBLE_WALL_3
			|| bhex == BattleHex::DESTRUCTIBLE_WALL_4)
			statemask.set(EU(HS::SIEGE_WALL));
	}

	void setActionMask(const std::shared_ptr<ActiveStackInfo> & astackinfo, const std::map<BattleHex, std::shared_ptr<Unit>> & hexstacks)
	{
		const auto * astack = astackinfo->stack;

		if(astackinfo->canshoot && stack && stack->cstack->unitSide() != astack->cstack->unitSide())
			actmask.set(EU(HexAction::SHOOT));

		if(astackinfo->rinfo->distances.at(moveDestHex.toInt()) <= astack->attr(UA::SPEED))
			actmask.set(EU(HexAction::MOVE));
		else
			return;

		const auto & nbhexes = NearbyBattleHexes(bhex);
		const auto * const aCstack = astack->cstack;

		for(int i = 0; i < static_cast<int>(nbhexes.size()); ++i)
		{
			const auto & nBhex = nbhexes.at(i);
			if(!nBhex.isAvailable())
				continue;

			auto it = hexstacks.find(nBhex);
			if(it == hexstacks.end())
				continue;

			const auto & nCstack = it->second->cstack;
			auto hexAction = static_cast<HexAction>(i);

			if(nCstack->unitSide() == aCstack->unitSide())
				continue;

			if(hexAction <= HexAction::AMOVE_TL)
			{
				ASSERT(CStack::isMeleeAttackPossible(aCstack, nCstack, moveDestHex), "vcmi says melee attack is IMPOSSIBLE [1]");
				actmask.set(i);
			}
			else if(isRUFR)
			{
				continue;
			}
			else if(hexAction <= HexAction::AMOVE_2BR)
			{
				if(aCstack->unitSide() == BattleSide::DEFENDER && aCstack->doubleWide())
				{
					ASSERT(CStack::isMeleeAttackPossible(aCstack, nCstack, moveDestHex), "vcmi says melee attack is IMPOSSIBLE [2]");
					actmask.set(i);
				}
			}
			else if(aCstack->unitSide() == BattleSide::ATTACKER && aCstack->doubleWide())
			{
				ASSERT(CStack::isMeleeAttackPossible(aCstack, nCstack, moveDestHex), "vcmi says melee attack is IMPOSSIBLE");
				actmask.set(i);
			}
		}
	}
};
}
