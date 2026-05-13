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

#include "BAI/v15/graph/nodes/base.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "battle/CObstacleInstance.h"
#include "common.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <vector>

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

namespace detail
{
	using Hex_Traits = S15::EncodingTraits<S15::Graph::NodeAttributes::Hex>;
	using Hex_Base = Base<Hex_Traits>;
}

class Hex : public detail::Hex_Base
{
	using HA = S15::Graph::NodeAttributes::Hex;
	using HS = S15::HexState;
	using HexAction = S15::HexAction;
	using HexActMask = std::bitset<EI(HexAction::_count)>;
public:
	using HexStateMask = std::bitset<EU(HS::_count)>;
	using HexActionHex = std::array<BattleHex, 12>;

	struct extra_index_type {
		using result_type = int16_t;
		result_type operator()(const std::shared_ptr<const Hex> & hex) const {
			return hex->bhex.toInt();
		}
	};

	static int CalcId(const BattleHex& bh);
	static std::pair<int, int> CalcXY(const BattleHex& bh);
	static HexActionHex NearbyBattleHexes(const BattleHex& bh);

	Hex(
		const BattleHex & bhex,
		EAccessibility accessibility,
		BattleSide side,
		const std::vector<std::shared_ptr<const CObstacleInstance>> & obstacles,
		int wallHP,
		bool isGateOpen
	);

	std::string name() const;
	void finalize();

	const BattleHex bhex;
	const int id;
	HexStateMask statemask = 0;

private:
	void setStateMask(
		EAccessibility accessibility,
		const std::vector<std::shared_ptr<const CObstacleInstance>>& obstacles,
		BattleSide side,
		bool isGateOpen
	);
};

}
