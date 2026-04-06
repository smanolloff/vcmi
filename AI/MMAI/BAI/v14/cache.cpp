#include "cache.h"
#include "CStack.h"
#include "battle/AccessibilityInfo.h"

namespace MMAI::BAI::V14
{
Cache::Cache(const CPlayerBattleCallback * battle)
	: battle(battle) {};

// XXX: the only difference between regular and per-stack accessibility
// is that the stack's own hexes are ACCESSIBLE instead of ALIVE_STACK
// For the purposes of MMAI observation we always want them as ALIVE_STACK
AccessibilityInfo Cache::getAccessibility()
{
	if(!acache)
		acache = std::make_unique<AccessibilityInfo>(battle->getAccessibility());

	return *acache;
}

ReachabilityInfo Cache::getReachability(const CStack * cstack)
{
	if(rcache.contains(cstack))
		return rcache[cstack];

	auto rinfo = battle->getReachability(cstack);
	auto dists = rinfo.distances;  // must not mutate rinfo => copy
	auto attacker = cstack->unitSide() == BattleSide::ATTACKER;

	if (cstack->doubleWide()) {
		for (int i=0; i<dists.size(); ++i) {
			const auto rhex = BattleHex(i);
			if(!rhex.isAvailable())
				continue;

			const auto fhex = rhex.cloneInDirection(attacker ? BattleHex::RIGHT : BattleHex::LEFT, false);
			if(!fhex.isAvailable())
				continue;

			// RUFR logic (Rear-Unreachable-with-Front-Reachable)
			// VCMI does not allow moving onto such hexes.
			// MMAI explicitly allows it, treating it as a MOVE to the front hex.
			// This is done to simplify AMOVE actions via the stack's "tail".
			if(!rinfo.isReachable(rhex.toInt()) && rinfo.isReachable(fhex.toInt())) {
				dists[rhex.toInt()] = dists[fhex.toInt()];
				rufrHexes[cstack][rhex.toInt()] = true;
			}
		}
	}

	rcache.try_emplace(cstack, rinfo);
	return rcache[cstack];
}

bool Cache::isRUFR(const CStack * cstack, const BattleHex & bh)
{
	if(!(cstack->doubleWide() && bh.isAvailable()))
		return false;

	getReachability(cstack);  // force init
	return rufrHexes[cstack].at(bh.toInt());
}
}
