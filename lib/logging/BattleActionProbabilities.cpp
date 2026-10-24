/*
 * BattleActionProbabilities.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleActionProbabilities.h"

BattleActionProbabilityStore & battleActionProbabilities()
{
	static BattleActionProbabilityStore store;
	return store;
}

void BattleActionProbabilityStore::setBattleActionProbabilities(BattleActionProbabilities probabilities)
{
	std::lock_guard lock(mutex);
	battleActionProbabilities = std::move(probabilities);
	++battleActionProbabilitiesRevision;
}

void BattleActionProbabilityStore::clearBattleActionProbabilities()
{
	std::lock_guard lock(mutex);
	battleActionProbabilities.reset();
	++battleActionProbabilitiesRevision;
}

uint64_t BattleActionProbabilityStore::getBattleActionProbabilitiesRevision()
{
	std::lock_guard lock(mutex);
	return battleActionProbabilitiesRevision;
}

uint64_t BattleActionProbabilityStore::getBattleActionProbabilities(std::optional<BattleActionProbabilities> & output)
{
	std::lock_guard lock(mutex);
	output = battleActionProbabilities;
	return battleActionProbabilitiesRevision;
}

