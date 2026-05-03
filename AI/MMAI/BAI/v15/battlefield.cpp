/*
 * battlefield.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "battle/IBattleInfoCallback.h"

#include "common.h"
#include "BAI/v15/battlefield.h"
#include "BAI/v15/hexaction.h"
#include "constants/Enumerations.h"
#include "entities/building/TownFortifications.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Battlefield
{

namespace
{
// using HA = HexAttribute;
// using LT = LinkType;
namespace S15 = Schema::V15;
using UA = S15::Graph::NodeAttributes::Unit;

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

// using LinkType = Schema::V15::LinkType;
// using AllLinks = std::map<LinkType, std::shared_ptr<Links>>;

using XY = std::pair<int, int>;

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

/*
 * Attacks from a moat hex are only allowed when the attacker already
 * stands in the moat.
 * E.g. in this scenario:
 *
 *  . . . . ~ . .            Legend:
 * . . . . ~ . .              o o   active stack (wide)
 *  . . x ~ . . .             x     enemy stack
 * . . . ~ . . .              ~     moat
 *  . . . . o o .             .     empty hex
 * . . . ~ . . .
 *  . . . ~ . . .
 * . . . . ~ . .
 *
 * The active stack can attack from the below positions only:
 *
 *  . . . . ~ .  |  . . . . ~ .  |  . . . . ~ .  |  . . . . ~ .  |
 * . . o o ~ . . | . o o . ~ . . | . . . . ~ . . | . . . . ~ . . |
 *  . . x ~ . .  |  . . x ~ . .  |  o o x ~ . .  |  . . x ~ . .  |
 * . . . ~ . . . | . . . ~ . . . | . . . ~ . . . | . o o ~ . . . |
 *  . . . . . .  |  . . . . . .  |  . . . . . .  |  . . . . . .  |
 * . . . ~ . . . | . . . ~ . . . | . . . ~ . . . | . . . ~ . . . |
 *  . . . ~ . .  |  . . . ~ . .  |  . . . ~ . .  |  . . . ~ . .  |
 *
 * However, in this scenario:
 *
 *  . . . . ~ . .
 * . . . o õ . .
 *  . . x ~ . . .
 * . . . ~ . . .
 *
 * An attack from the same position is also possible.
 *
 * NOTE: the above example uses moat, but in practice any STOPPING hex
 * 		 (i.e. quicksand) uses the same logic.
 *
 *
 * NOTE:
 * Currently, in VCMI this logic applies to both flyers and non-flyers.
 * However, in vanilla H3 flyers CAN move-and-attack onto a moat hex:
 * https://discord.com/channels/298106089885401090/298106089885401090/1485333577049833532
 * If VCMI gets updated to match H3 logic, fix this function accordingly.
 *
 *
 * Unfortunately, this can't be handled in the Hex() constructor, as it
 * requires knowledge of other hexes => set it here, after all hexes
 * are constructed.
 *
 */
// void FixMoatMasks(const CPlayerBattleCallback * battle, const Hexes * hexes, const Graph::Nodes::Unit * astack)
// {
// 	// e.g. at battle start there is no active stack
// 	if(!astack)
// 		return;

// 	// Mask out AMOVE actions (i.e. all actions except MOVE / SHOOT)
// 	static_assert(EI(HexAction::MOVE) == 12);
// 	static_assert(EI(HexAction::SHOOT) == 13);
// 	static_assert(EI(HexAction::_count) == 14);
// 	auto noAmove = Graph::Nodes::Hex::HexActionMask(0b11000000000000); // leftmost bit = SHOOT
// 	assert(noAmove.test(EI(HexAction::MOVE)));
// 	assert(noAmove.test(EI(HexAction::SHOOT)));

// 	for(int row = 0; row < 11; ++row)
// 	{
// 		for(int col = 0; col < 15; ++col)
// 		{
// 			const auto & hex = hexes->at(row).at(col);

// 			// Stacks already standing on a STOPPING hex can attack from it
// 			if(astack->cstack->getPosition() == hex->bhex)
// 				continue;

// 			// Flyers are not affected by STOPPING hexes
// 			// XXX: in VCMI, flyers are also affected. If this changes in
// 			// a future version, an if+continue check for flyers here is needed.

// 			// Convenience for disallowing AMOVE actions
// 			auto applyMask = [&noAmove, &hex]
// 			{
// 				hex->actmask &= noAmove;
// 				hex->finalize();
// 			};

// 			// Moving onto a stopping hex aborts the attack => disallow
// 			if(hex->statemask.test(EI(S15::HexState::STOPPING)))
// 			{
// 				applyMask();
// 				continue;
// 			}

// 			// Cases below are for wide creatures only
// 			if(!astack->cstack->doubleWide())
// 				continue;

// 			bool isAttacker = astack->cstack->unitSide() == BattleSide::ATTACKER;

// 			// When moving wide stacks, we must check if their tail
// 			// will land on a STOPPING hex, as it will also abort the attack
// 			// => disallow
// 			if(isAttacker && col > 0)
// 			{
// 				const auto & nbh = hexes->at(row).at(col - 1);
// 				if(nbh->statemask.test(EI(S15::HexState::STOPPING)))
// 					applyMask();
// 			}
// 			else if(!isAttacker && col < 14)
// 			{
// 				const auto & nbh = hexes->at(row).at(col + 1);
// 				if(nbh->statemask.test(EI(S15::HexState::STOPPING)))
// 					applyMask();
// 			}
// 		}
// 	}
// }

void InitHexes(
	const CPlayerBattleCallback & battle,
	const CStack * acstack,
	Graph::Graph & G
)
{
	auto hexstacks = std::map<BattleHex, std::shared_ptr<Graph::Nodes::Unit>>{};
	auto hexobstacles = std::array<std::vector<std::shared_ptr<const CObstacleInstance>>, 165>{};
	const auto & ainfo = cache.getAccessibility();

	for(const auto & stack : G.getAll<Graph::Nodes::Unit>())
	{
		for(const auto & bh : stack->cstack->getHexes())
			if(bh.isAvailable())
				hexstacks.try_emplace(bh, stack);
	}

	for(const auto & obstacle : battle->battleGetAllObstacles())
		for(const auto & bh : obstacle->getAffectedTiles())
			if(bh.isAvailable())
				hexobstacles.at(Graph::Nodes::Hex::CalcId(bh)).push_back(obstacle);

	auto gatestate = battle->battleGetGateState();
	bool isGateOpen = battle->battleGetFortifications().wallsHealth > 0
		&& (gatestate == EGateState::OPENED || gatestate == EGateState::DESTROYED);

	for(int y = 0; y < 11; ++y)
	{
		for(int x = 0; x < 15; ++x)
		{
			auto i = (y * 15) + x;
			auto bh = BattleHex(x + 1, y);
			auto it = hexstacks.find(bh);

			G.add(std::make_shared<Graph::Nodes::Hex>(
				bh,
				ainfo.at(bh.toInt()),
				hexobstacles.at(i),
				it == hexstacks.end() ? nullptr : it->second->cstack,
				acstack ? cache.isRUFR(acstack, bh) : false,
				WallHP(battle, bh),
				isGateOpen
			));
		}
	}
};

// // static
// AllLinks Battlefield::InitAllLinks(const CPlayerBattleCallback * battle, const Stacks & stacks, const Queue & queue, const std::shared_ptr<Hexes> & hexes)
// {
// 	auto allLinks = AllLinks();

// 	for(auto i = 0; i < EI(LT::_count); ++i)
// 		allLinks[static_cast<LT>(i)] = std::make_shared<Links>();

// 	for(const auto & srcrow : *hexes)
// 	{
// 		for(const auto & srchex : srcrow)
// 		{
// 			for(const auto & dstrow : *hexes)
// 			{
// 				for(const auto & dsthex : dstrow)
// 				{
// 					LinkTwoHexes(allLinks, battle, stacks, queue, srchex.get(), dsthex.get());
// 				}
// 			}
// 		}
// 	}

// 	return allLinks;
// }

namespace
{
	float calculateRangeMod(const CPlayerBattleCallback * battle, const CStack * cstack, const BattleHex & src, const BattleHex & dst)
	{
		float rangemod = 1;
		if(battle->battleHasDistancePenalty(cstack, src, dst))
			rangemod *= 0.5;
		if(battle->battleHasWallPenalty(cstack, src, dst))
			rangemod *= 0.5;
		return rangemod;
	}
}

// void Battlefield::LinkTwoHexes(
// 	AllLinks & allLinks,
// 	const CPlayerBattleCallback * battle,
// 	const Stacks & stacks,
// 	const Queue & queue,
// 	const Hex * src,
// 	const Hex * dst
// )
// {
// 	static const auto adjmap = InitAdjMap();
// 	bool neighbour = adjmap.contains({src->bhex.toInt(), dst->bhex.toInt()});
// 	bool reachable = false;
// 	float rangemod = 0;
// 	float rangedDmgFrac = 0;
// 	float meleeDmgFrac = 0;
// 	float retalDmgFrac = 0;
// 	int actsBefore = 0;

// 	if(src->stack && !src->getAttr(HA::IS_REAR) && !src->stack->flag(StackFlag1::SLEEPING))
// 	{
// 		reachable = src->stack->rinfo.distances.at(dst->bhex.toInt()) <= src->stack->attr(SA::SPEED);

// 		// rangemod is set even if dst is free
// 		if(src->stack->cstack->canShoot() && !src->stack->cstack->coversPos(dst->bhex) && !src->stack->flag(StackFlag1::BLOCKED) && !neighbour)
// 		{
// 			rangemod = calculateRangeMod(battle, src->stack->cstack, src->bhex, dst->bhex);
// 		}

// 		// *dmgFracs are set only between opposing stacks
// 		if(dst->stack && (dst->stack->cstack->unitSide() != src->stack->cstack->unitSide()))
// 		{
// 			if(rangemod > 0)
// 			{
// 				auto estdmg = battle->calculateDmgRange(BattleAttackInfo(src->stack->cstack, dst->stack->cstack, 0, true));
// 				auto avgdmg = 0.5 * (estdmg.damage.max + estdmg.damage.min);
// 				// negate the rangemod in the dmg calc (i.e. report the "base" dmg)
// 				avgdmg *= 1 / rangemod;
// 				rangedDmgFrac = avgdmg / dst->stack->cstack->getAvailableHealth();
// 			}

// 			auto bai = BattleAttackInfo(src->stack->cstack, dst->stack->cstack, 0, false);
// 			auto retdmg = DamageEstimation{};
// 			auto estdmg = battle->battleEstimateDamage(bai, &retdmg);
// 			auto avgdmg = 0.5 * (estdmg.damage.max + estdmg.damage.min);
// 			meleeDmgFrac = avgdmg / dst->stack->cstack->getAvailableHealth();

// 			if(retdmg.damage.max > 0)
// 			{
// 				auto avgret = 0.5 * (retdmg.damage.max + retdmg.damage.min);
// 				retalDmgFrac = avgret / src->stack->cstack->getAvailableHealth();
// 			}
// 		}
// 	}

// 	if(src->stack && dst->stack && src->id != dst->id)
// 	{
// 		auto srcpos = src->stack->qposFirst;
// 		auto dstpos = dst->stack->qposFirst;
// 		if(srcpos < dstpos)
// 		{
// 			ASSERT(dstpos <= queue.size(), "dstpos exceeds queue size");
// 			actsBefore = true;
// 		}
// 	}

// 	//
// 	// Build links
// 	//

// 	if(neighbour)
// 	{
// 		auto it = adjmap.find({src->bhex.toInt(), dst->bhex.toInt()});
// 		assert(it != adjmap.end());
// 		assert(it->second < 6);
// 		auto attrs = std::vector<float>{};
// 		Encoder::EncodeCategoricalStrictNull(it->second, 6, attrs);
// 		allLinks[LT::ADJACENT]->add(src->id, dst->id, attrs);
// 	}

// 	if(reachable)
// 		allLinks[LT::REACH]->add(src->id, dst->id, 1);

// 	if(actsBefore)
// 		allLinks[LT::ACTS_BEFORE]->add(src->id, dst->id, std::min<int>(2, actsBefore));

// 	if(rangemod)
// 		allLinks[LT::RANGED_MOD]->add(src->id, dst->id, std::min<float>(2, rangemod));

// 	if(rangedDmgFrac)
// 		allLinks[LT::RANGED_DMG_REL]->add(src->id, dst->id, std::min<float>(2, rangedDmgFrac));

// 	if(meleeDmgFrac)
// 		allLinks[LT::MELEE_DMG_REL]->add(src->id, dst->id, std::min<float>(2, meleeDmgFrac));

// 	if(retalDmgFrac)
// 		allLinks[LT::RETAL_DMG_REL]->add(src->id, dst->id, std::min<float>(2, retalDmgFrac));
// }
}

void Init(
	const CPlayerBattleCallback & battle,
	const CStack * acstack,
	const Graph::Graph & oldG,
	Graph::Graph & G,
	std::unordered_map<const CStack *, Graph::Nodes::Unit::Stats> & stacksStats,
	bool isMorale
)
{
	InitStacks(battle, acstack, oldG, G, stacksStats, isMorale);
	InitHexes(battle, acstack, G);

	// FixMoatMasks(battle, hexes.get(), astack);

	// auto links = InitAllLinks(battle, stacks, queue, hexes);
	// return std::make_shared<const Battlefield>(hexes, stacks, links, astack);
}

}
