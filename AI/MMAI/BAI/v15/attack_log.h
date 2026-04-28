/*
 * attack_log.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/stack.h"
#include "common.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{

struct AttackLogData
{
	const std::shared_ptr<Stack> attacker; // XXX: can be nullptr if dmg is not from creature
	const std::shared_ptr<Stack> defender;
	const CStack * cattacker;
	const CStack * cdefender;
	const int dmg;
	const int dmgPermille;
	const int units;
	const int value;
	const int valuePermille;
};

class AttackLog : public Schema::V15::IAttackLog
{
public:
	explicit AttackLog(const AttackLogData & data) : data(data) {};

	const AttackLogData data;

	std::string getAttackerColor() const override
	{
		if (!data.attacker)
			return "?";

		return data.attacker->cstack->unitSide() == BattleSide::ATTACKER ? "red" : "blue";
	}

	std::string getAttackerAlias() const override
	{
		return data.attacker ? std::to_string(data.attacker->alias) : "?";
	}

	std::string getDefenderColor() const override
	{
		if (!data.defender)
			return "?";

		return data.defender->cstack->unitSide() == BattleSide::ATTACKER ? "red" : "blue";
	}

	std::string getDefenderAlias() const override
	{
		return data.defender ? std::to_string(data.defender->alias) : "?";
	}


	int getDamageDealt() const override
	{
		return data.dmg;
	}
	int getDamageDealtPermille() const override
	{
		return data.dmgPermille;
	}
	int getUnitsKilled() const override
	{
		return data.units;
	}
	int getValueKilled() const override
	{
		return data.value;
	}
	int getValueKilledPermille() const override
	{
		return data.valuePermille;
	}

	/*
	 * attacker dealing dmg might be our friendly fire
	 * If we look at Attacker POV, we would count our friendly fire as "dmg dealt"
	 * So we look at Defender POV, so our friendly fire is counted as "dmg received"
	 * This means that if the enemy does friendly fire dmg,
	 *  we would count it as our dmg dealt - that is OK (we have "tricked" the enemy!)
	 * => store only defender slot
	 */
};
}
