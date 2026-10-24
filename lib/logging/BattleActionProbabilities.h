/*
 * BattleActionProbabilities.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../battle/BattleHex.h"
#include "../battle/BattleSide.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>

struct DLL_LINKAGE BattleActionProbabilities
{
	std::map<BattleHex, float> attackTargets;
	std::map<BattleHex, float> moves;
	std::map<BattleHex, std::array<std::optional<float>, 6>> meleeAttacks;
	std::map<BattleHex, std::array<std::optional<BattleHex>, 6>> meleeAttackTargets;
	float wait = 0.0f;
	float defend = 0.0f;
	float value = 0.0f;
	BattleSide side = BattleSide::NONE;

	float maximum() const
	{
		float result = std::max(wait, defend);
		for(const auto & entry : attackTargets)
			result = std::max(result, entry.second);
		for(const auto & entry : moves)
			result = std::max(result, entry.second);
		for(const auto & entry : meleeAttacks)
			for(const auto & probability : entry.second)
				if(probability)
					result = std::max(result, *probability);
		return result;
	}
};

class DLL_LINKAGE BattleActionProbabilityStore
{
	std::mutex mutex;
	std::optional<BattleActionProbabilities> battleActionProbabilities;
	uint64_t battleActionProbabilitiesRevision = 0;

public:
	void setBattleActionProbabilities(BattleActionProbabilities probabilities);
	void clearBattleActionProbabilities();
	uint64_t getBattleActionProbabilitiesRevision();
	uint64_t getBattleActionProbabilities(std::optional<BattleActionProbabilities> & output);
};

DLL_LINKAGE BattleActionProbabilityStore & battleActionProbabilities();
