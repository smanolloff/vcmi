/*
 * unit.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "CCreatureHandler.h"
#include "CStack.h"

#include "BAI/v15/graph/nodes/base.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

#include <bitset>
#include <map>
#include <vector>

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

namespace detail
{
	using Unit_Traits = S15::EncodingTraits<S15::NodeEncoding_Unit>;
	using Unit_Base = Base<Unit_Traits>;
}

class Unit : public detail::Unit_Base
{
	using GA = S15::Graph::NodeAttributes::Global;
	using UA = S15::Graph::NodeAttributes::Unit;
	using StackFlag1 = S15::StackFlag1;
	using StackFlag2 = S15::StackFlag2;
	using StackFlags1 = S15::StackFlags1;
	using StackFlags2 = S15::StackFlags2;

	using BitQueue = std::bitset<S15::STACK_QUEUE_SIZE>;
	using CreatureValues = std::map<CreatureID, int>;

	static_assert(
		S15::STACK_QUEUE_SIZE < std::numeric_limits<int>::digits,
		"BitQueue must be convertible to int"
	);

public:
	using Queue = std::vector<uint32_t>;

	struct Stats
	{
		int dmgDealtNow = 0;
		int dmgDealtTotal = 0;
		int dmgReceivedNow = 0;
		int dmgReceivedTotal = 0;
		int valueKilledNow = 0;
		int valueKilledTotal = 0;
		int valueLostNow = 0;
		int valueLostTotal = 0;
	};

	struct StatsContainer
	{
		const int bfieldValueNow;
		const int bfieldValuePrev;
		const int bfieldValueStart;
		const int bfieldHpNow;
		const int bfieldHpPrev;
		const int bfieldHpStart;

		const Stats stackStats;
	};

	static int GetValue(const CCreature* creature);
	static std::pair<BitQueue, int> QBits(const CStack & cstack, const Queue& vec);

	Unit(
		const CStack & cstack,
		const Queue & q,
		const StatsContainer & statsContainer
	);

	int getFlag(StackFlag1 sf) const;
	int getFlag(StackFlag2 sf) const;

	char getAlias() const;

	bool flag(StackFlag1 f) const;
	bool flag(StackFlag2 f) const;

	const CStack & cstack;

	StackFlags1 flags1 = 0;
	StackFlags2 flags2 = 0;
	char alias = '\0';
	int shots = 0;
	int qposFirst = -1;

	static int CalculateSlot(const CStack & cstack);
	static char CalculateAlias(int slot);
private:
	static int CalculateValue(const CCreature* cr);
	static CreatureValues initCreatureValues();

	void setflag(StackFlag1 f);
	void setflag(StackFlag2 f);

	void finalize();
	void processBonuses();
};

}
