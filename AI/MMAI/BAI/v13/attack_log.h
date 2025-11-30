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

#include "BAI/v13/stack.h"
#include "schema/v13/types.h"

namespace MMAI::BAI::V13
{
class AttackLog : public Schema::V13::IAttackLog
{
public:
	AttackLog(
		std::shared_ptr<Stack> attacker_,
		std::shared_ptr<Stack> defender_,
		const CStack * cattacker_,
		const CStack * cdefender_,
		int dmg_,
		int dmgPermille_,
		int units_,
		int value_,
		int valuePermille_
	)
		: attacker(attacker_)
		, defender(defender_)
		, cattacker(cattacker_)
		, cdefender(cdefender_)
		, dmg(dmg_)
		, dmgPermille(dmgPermille_)
		, units(units_)
		, value(value_)
		, valuePermille(valuePermille_)
	{
	}

	// IAttackLog impl
	Stack * getAttacker() const override
	{
		return attacker.get();
	}
	Stack * getDefender() const override
	{
		return defender.get();
	}
	int getDamageDealt() const override
	{
		return dmg;
	}
	int getDamageDealtPermille() const override
	{
		return dmgPermille;
	}
	int getUnitsKilled() const override
	{
		return units;
	}
	int getValueKilled() const override
	{
		return value;
	}
	int getValueKilledPermille() const override
	{
		return valuePermille;
	}

	/*
	 * attacker dealing dmg might be our friendly fire
	 * If we look at Attacker POV, we would count our friendly fire as "dmg dealt"
	 * So we look at Defender POV, so our friendly fire is counted as "dmg received"
	 * This means that if the enemy does friendly fire dmg,
	 *  we would count it as our dmg dealt - that is OK (we have "tricked" the enemy!)
	 * => store only defender slot
	 */

	const std::shared_ptr<Stack> attacker; // XXX: can be nullptr if dmg is not from creature
	const std::shared_ptr<Stack> defender;
	const CStack * cattacker;
	const CStack * cdefender;
	const int dmg;
	const int dmgPermille;
	const int units;

	// NOTE: "value" is hard-coded in original H3 and can be found online:
	// https://heroes.thelazy.net/index.php/List_of_creatures
	const int value;
	const int valuePermille;
};
}
