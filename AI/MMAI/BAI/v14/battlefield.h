/*
 * battlefield.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v14/cache.h"
#include "battle/AccessibilityInfo.h"
#include "battle/CPlayerBattleCallback.h"

#include "BAI/v14/hex.h"
#include "BAI/v14/links.h"
#include "BAI/v14/stack.h"
#include "battle/ReachabilityInfo.h"

namespace MMAI::BAI::V14
{

using LinkType = Schema::V14::LinkType;
using Stacks = std::vector<std::shared_ptr<Stack>>;
using Hexes = std::array<std::array<std::unique_ptr<Hex>, 15>, 11>;
using AllLinks = std::map<LinkType, std::shared_ptr<Links>>;

using XY = std::pair<int, int>;

class Battlefield
{
public:
	static std::shared_ptr<const Battlefield> Create(
		std::shared_ptr<Cache> & cache,
		const CPlayerBattleCallback * battle,
		const CStack * acstack,
		const GlobalStats * oldgstats,
		const GlobalStats * gstats,
		std::map<const CStack *, Stack::Stats> & stacksStats,
		bool isMorale
	);

	Battlefield(
		const std::shared_ptr<Hexes> & hexes,
		const Stacks & stacks,
		const AllLinks & allLinks,
		const Stack * astack
	);

	const std::shared_ptr<Hexes> hexes;
	const Stacks stacks;
	const AllLinks allLinks;
	const Stack * const astack; // XXX: nullptr on battle start/end, or if army stacks > MAX_STACKS_PER_SIDE
private:
	static std::tuple<Stacks, Queue> InitStacks(
		std::shared_ptr<Cache> & cache,
		const CPlayerBattleCallback * battle,
		const CStack * astack,
		const GlobalStats * oldgstats,
		const GlobalStats * gstats,
		std::map<const CStack *, Stack::Stats> & stacksStats,
		bool isMorale
	);

	static std::tuple<std::shared_ptr<Hexes>, Stack *> InitHexes(
		std::shared_ptr<Cache> & cache,
		const CPlayerBattleCallback * battle,
		const CStack * acstack,
		const Stacks & stacks
	);

	static AllLinks InitAllLinks(const CPlayerBattleCallback * battle, const Stacks & stacks, const Queue & queue, const std::shared_ptr<Hexes> & hexes);

	static void
	LinkTwoHexes(AllLinks & allLinks, const CPlayerBattleCallback * battle, const Stacks & stacks, const Queue & queue, const Hex * src, const Hex * dst);

	static Queue GetQueue(const CPlayerBattleCallback * battle, const CStack * astack, bool isMorale);
};
}
