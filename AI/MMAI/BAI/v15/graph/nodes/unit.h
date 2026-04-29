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
#include "GameLibrary.h"
#include "battle/IBattleInfoCallback.h"
#include "battle/ReachabilityInfo.h"
#include "bonuses/BonusEnum.h"
#include "constants/EntityIdentifiers.h"

#include "BAI/v15/graph/nodes/global.h"
#include "AI/MMAI/common.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using GA = S15::Graph::NodeAttributes::Global;
using UA = S15::Graph::NodeAttributes::Unit;
using StackFlag1 = S15::StackFlag1;
using StackFlag2 = S15::StackFlag2;
using StackFlags1 = S15::StackFlags1;
using StackFlags2 = S15::StackFlags2;

using Queue = std::vector<uint32_t>; // item=unit id
using BitQueue = std::bitset<S15::STACK_QUEUE_SIZE>;
using CreatureValues = std::map<CreatureID, int>;

static_assert(1 << S15::STACK_QUEUE_SIZE < std::numeric_limits<int>::max(), "BitQueue must be convertible to int");

class Unit : public Element<S15::EncodingTraits<Schema::V15::UnitEncoding>, S15::Graph::INode>
{
public:
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
		const Global * oldGlobal;
		const Global * global;
		const Stats stackStats;
	};

	static int GetValue(const CCreature * creature)
	{
		static const CreatureValues CREATURE_VALUES = initCreatureValues();

		if(!creature)
			throw std::runtime_error("GetValue: nullptr given");

		const auto & it = CREATURE_VALUES.find(creature->getId());

		if(it == CREATURE_VALUES.end())
			throw std::runtime_error("GetValue: no value for creature with ID=" + std::to_string(creature->getIndex()));

		return it->second;
	}

	static std::pair<BitQueue, int> QBits(const CStack * cstack, const Queue & vec)
	{
		BitQueue q;
		int pos = -1;
		if(vec.size() != S15::STACK_QUEUE_SIZE)
			throw std::runtime_error("Unexpected queue size: " + std::to_string(vec.size()));

		for(size_t i = 0; i < vec.size(); ++i)
		{
			if(vec[i] == cstack->unitId())
			{
				q.set(i);
				if(pos < 0)
					pos = static_cast<int>(i);
			}
		}

		return {q, pos};
	}

	Unit(
		int index,
		const CStack * cstack_,
		const Queue & q,
		const StatsContainer & statsContainer,
		const ReachabilityInfo & rinfo_,
		bool blocked,
		bool blocking,
		const DamageEstimation & estdmg
	)
		: index_(index)
		, cstack(cstack_)
		, rinfo(rinfo_)
	{
		(void)estdmg;

		const auto & oldGlobal = statsContainer.oldGlobal;
		const auto & global = statsContainer.global;
		const auto & stackStats = statsContainer.stackStats;

		int slot = calculateSlot(cstack);
		alias = calculateAlias(slot);

		auto [qbits, pos] = QBits(cstack, q);
		qposFirst = pos;

		processBonuses();

#ifdef ENABLE_ML
		if(cstack->creatureId().num > S15::CREATURE_ID_MAX)
			throw std::runtime_error("unknown creature id: " + std::to_string(cstack->creatureId().num));
#endif

		if(cstack->willMove())
		{
			setflag(StackFlag1::WILL_ACT);
			if(!cstack->waitedThisTurn)
				setflag(StackFlag1::CAN_WAIT);
		}

		if(cstack->ableToRetaliate())
			setflag(StackFlag1::CAN_RETALIATE);

		if(blocked)
			setflag(StackFlag1::BLOCKED);

		if(blocking)
			setflag(StackFlag1::BLOCKING);

		if(cstack->occupiedHex().isAvailable())
			setflag(StackFlag1::IS_WIDE);

		if(qbits.test(0))
			setflag(StackFlag1::IS_ACTIVE);

		shots = cstack->shots.available();

		auto valueOne = GetValue(cstack->unitType());

		if(cstack->isClone())
			valueOne *= 5;
		else if(cstack->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER)
			valueOne = static_cast<int>(valueOne * 0.2);

		auto permille = [](int v1, int v2)
		{
			return static_cast<int>((1000LL * v1) / v2);
		};

		auto bfValueNow = global->attr(GA::BFIELD_VALUE_NOW_ABS);
		auto bfValuePrev = oldGlobal->attr(GA::BFIELD_VALUE_NOW_ABS);
		auto bfValueStart = global->attr(GA::BFIELD_VALUE_START_ABS);
		auto bfHpPrev = oldGlobal->attr(GA::BFIELD_HP_NOW_ABS);
		auto bfHpStart = global->attr(GA::BFIELD_HP_START_ABS);
		auto value = valueOne * cstack->getCount();

		setattr(UA::SIDE, EU(cstack->unitSide()));
		setattr(UA::SLOT, slot);
		setattr(UA::QUANTITY, cstack->getCount());
		setattr(UA::ATTACK, cstack->getAttack(shots > 0));
		setattr(UA::DEFENSE, cstack->getDefense(false));
		setattr(UA::SHOTS, shots);
		setattr(UA::DMG_MIN, cstack->getMinDamage(shots > 0));
		setattr(UA::DMG_MAX, cstack->getMaxDamage(shots > 0));
		setattr(UA::HP, cstack->getMaxHealth());
		setattr(UA::HP_LEFT, cstack->getFirstHPleft());
		setattr(UA::SPEED, cstack->getMovementRange());
		setattr(UA::QUEUE, static_cast<int>(qbits.to_ulong()));
		setattr(UA::VALUE_ONE, valueOne);
		setattr(UA::VALUE_REL, permille(value, bfValueNow));
		setattr(UA::VALUE_REL0, permille(value, bfValueStart));
		setattr(UA::VALUE_KILLED_REL, permille(stackStats.valueKilledNow, bfValuePrev));
		setattr(UA::VALUE_KILLED_ACC_REL0, permille(stackStats.valueKilledTotal, bfValueStart));
		setattr(UA::VALUE_LOST_REL, permille(stackStats.valueLostNow, bfValuePrev));
		setattr(UA::VALUE_LOST_ACC_REL0, permille(stackStats.valueLostTotal, bfValueStart));
		setattr(UA::DMG_DEALT_REL, permille(stackStats.dmgDealtNow, bfHpPrev));
		setattr(UA::DMG_DEALT_ACC_REL0, permille(stackStats.dmgDealtTotal, bfHpStart));
		setattr(UA::DMG_RECEIVED_REL, permille(stackStats.dmgReceivedNow, bfHpPrev));
		setattr(UA::DMG_RECEIVED_ACC_REL0, permille(stackStats.dmgReceivedTotal, bfHpStart));

		static_assert(EU(UA::_count) == 25, "whistleblower in case attributes change");

		finalize();
	}

    int nodeIndex() const override {
        return index_;
    }

	int getFlag(StackFlag1 sf) const
	{
		return flag(sf);
	}

	int getFlag(StackFlag2 sf) const
	{
		return flag(sf);
	}

	char getAlias() const
	{
		return alias;
	}

	bool flag(StackFlag1 f) const
	{
		return flags1.test(EU(f));
	}

	bool flag(StackFlag2 f) const
	{
		return flags2.test(EU(f));
	}

	const CStack * const cstack;
	const ReachabilityInfo rinfo;
	std::array<int, EU(UA::_count)> attrs = {};
	StackFlags1 flags1 = 0;
	StackFlags2 flags2 = 0;
	char alias = '\0';
	int shots = 0;
	int qposFirst = -1;

private:
    const int index_;

	static int calculateSlot(const CStack * cstack)
	{
		int slot = cstack->unitSlot();
		if(slot >= 0 && slot < 7)
			return slot;
		if(slot == SlotID::WAR_MACHINES_SLOT)
			return S15::STACK_SLOT_WARMACHINES;
		return S15::STACK_SLOT_SPECIAL;
	}

	static char calculateAlias(int slot)
	{
		switch(slot)
		{
			case S15::STACK_SLOT_SPECIAL:
				return 'S';
			case S15::STACK_SLOT_WARMACHINES:
				return 'M';
			default:
				return static_cast<char>('0' + slot);
		}
	}

	static int calculateValue(const CCreature * cr)
	{
		auto att = cr->getBaseAttack();
		auto def = cr->getBaseDefense();
		auto dmg = (cr->getBaseDamageMax() + cr->getBaseDamageMin()) / 2.0;
		auto hp = cr->getBaseHitPoints();
		auto spd = cr->getBaseSpeed();
		auto shooter = cr->hasBonusOfType(BonusType::SHOOTER);
		auto bonuses = cr->getAllBonuses(Selector::all);

		auto a = 3 * dmg * (1 + std::min(4.0, 0.05 * att));
		auto b = hp / (1 - std::min(0.7, 0.025 * def));
		auto c = spd ? std::log(spd * 2) : 0.5;
		auto d = shooter ? 1.5 : 1.0;

		for(const auto & bonus : *bonuses)
		{
			switch(bonus->type)
			{
				case BonusType::ADDITIONAL_ATTACK:
					d += (shooter ? 0.5 : 0.3);
					break;
				case BonusType::ADDITIONAL_RETALIATION:
					d += (bonus->val * 0.1);
					break;
				case BonusType::ATTACKS_ALL_ADJACENT:
					d += 0.2;
					break;
				case BonusType::BLOCKS_RETALIATION:
					d += 0.3;
					break;
				case BonusType::DEATH_STARE:
					d += (bonus->val * 0.02);
					break;
				case BonusType::DOUBLE_DAMAGE_CHANCE:
					d += (bonus->val * 0.005);
					break;
				case BonusType::ENEMY_DEFENCE_REDUCTION:
					d += (bonus->val * 0.0025);
					break;
				case BonusType::FIRE_SHIELD:
					d += (bonus->val * 0.003);
					break;
				case BonusType::FLYING:
					d += 0.1;
					break;
				case BonusType::LIFE_DRAIN:
					d += (bonus->val * 0.003);
					break;
				case BonusType::NO_DISTANCE_PENALTY:
					d += 0.5;
					break;
				case BonusType::NO_MELEE_PENALTY:
					d += 0.1;
					break;
				case BonusType::THREE_HEADED_ATTACK:
					d += 0.05;
					break;
				case BonusType::TWO_HEX_ATTACK_BREATH:
					d += 0.1;
					break;
				case BonusType::UNLIMITED_RETALIATIONS:
					d += 0.2;
					break;
				case BonusType::SPELL_LIKE_ATTACK:
					if(bonus->subtype.as<SpellID>() == SpellID::DEATH_CLOUD)
						d += 0.2;
					break;
				case BonusType::SPELL_AFTER_ATTACK:
					switch(bonus->subtype.as<SpellID>())
					{
						case SpellID::BLIND:
						case SpellID::STONE_GAZE:
						case SpellID::PARALYZE:
							d += (bonus->val * 0.01);
							break;
						case SpellID::BIND:
						case SpellID::WEAKNESS:
							d += (bonus->val * 0.001);
							break;
						case SpellID::AGE:
							d += (bonus->val * 0.005);
							break;
						case SpellID::CURSE:
							d += (bonus->val * 0.0025);
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}
		}

		auto res = static_cast<int>(std::round(10 * (a + b) * c * d));

		if(isMMAIVerbose())
		{
			std::cout << "MMAI_VERBOSE: " << res << " " << cr->getId().toEntity(LIBRARY)->getJsonKey() << " (a=" << a << ", b=" << b << ", c=" << c
					  << ", d=" << d << ")\n";
		}

		return res;
	}

	static CreatureValues initCreatureValues()
	{
		CreatureValues values;

		for(const auto & creature : LIBRARY->creh->objects)
			if(creature)
				values.try_emplace(creature->getId(), calculateValue(creature.get()));

		return values;
	}

	void setflag(StackFlag1 f)
	{
		flags1.set(EU(f));
	}

	void setflag(StackFlag2 f)
	{
		flags2.set(EU(f));
	}

	void finalize()
	{
		setattr(UA::FLAGS1, static_cast<int>(flags1.to_ulong()));
		setattr(UA::FLAGS2, static_cast<int>(flags2.to_ulong()));
	}

	void processBonuses()
	{
		auto bonuses = cstack->getAllBonuses(Selector::all);

		for(const auto & bonus : *bonuses)
		{
			switch(bonus->type)
			{
				case BonusType::FLYING:
					setflag(StackFlag1::FLYING);
					break;
				case BonusType::SHOOTER:
					setflag(StackFlag1::SHOOTER);
					break;
				case BonusType::UNDEAD:
				case BonusType::NON_LIVING:
					setflag(StackFlag1::NON_LIVING);
					break;
				case BonusType::SIEGE_WEAPON:
					setflag(StackFlag1::WAR_MACHINE);
					break;
				case BonusType::BLOCKS_RETALIATION:
					setflag(StackFlag1::BLOCKS_RETALIATION);
					break;
				case BonusType::NO_MELEE_PENALTY:
					setflag(StackFlag1::NO_MELEE_PENALTY);
					break;
				case BonusType::TWO_HEX_ATTACK_BREATH:
					setflag(StackFlag1::TWO_HEX_ATTACK_BREATH);
					break;
				case BonusType::ADDITIONAL_ATTACK:
					setflag(StackFlag1::ADDITIONAL_ATTACK);
					break;
				case BonusType::SPELL_AFTER_ATTACK:
					switch(bonus->subtype.as<SpellID>())
					{
						case SpellID::BLIND:
						case SpellID::PARALYZE:
							setflag(StackFlag2::BLIND_ATTACK);
							break;
						case SpellID::STONE_GAZE:
							setflag(StackFlag2::PETRIFY_ATTACK);
							break;
						case SpellID::BIND:
							setflag(StackFlag2::BIND_ATTACK);
							break;
						case SpellID::WEAKNESS:
							setflag(StackFlag2::WEAKNESS_ATTACK);
							break;
						case SpellID::DISPEL:
						case SpellID::DISPEL_HELPFUL_SPELLS:
							setflag(StackFlag2::DISPEL_ATTACK);
							break;
						case SpellID::POISON:
							setflag(StackFlag2::POISON_ATTACK);
							break;
						case SpellID::CURSE:
							setflag(StackFlag2::CURSE_ATTACK);
							break;
						case SpellID::AGE:
							setflag(StackFlag2::AGE_ATTACK);
							break;
						default:
							break;
					}
					break;
				case BonusType::SPELL_LIKE_ATTACK:
					switch(bonus->subtype.as<SpellID>())
					{
						case SpellID::FIREBALL:
							setflag(StackFlag1::FIREBALL);
							break;
						case SpellID::DEATH_CLOUD:
							setflag(StackFlag1::DEATH_CLOUD);
							break;
						default:
							break;
					}
					break;
				case BonusType::THREE_HEADED_ATTACK:
					setflag(StackFlag1::THREE_HEADED_ATTACK);
					break;
				case BonusType::ATTACKS_ALL_ADJACENT:
					setflag(StackFlag1::ALL_AROUND_ATTACK);
					break;
				case BonusType::RETURN_AFTER_STRIKE:
					setflag(StackFlag1::RETURN_AFTER_STRIKE);
					break;
				case BonusType::ENEMY_DEFENCE_REDUCTION:
					setflag(StackFlag1::ENEMY_DEFENCE_REDUCTION);
					break;
				case BonusType::LIFE_DRAIN:
					setflag(StackFlag1::LIFE_DRAIN);
					break;
				case BonusType::DOUBLE_DAMAGE_CHANCE:
					setflag(StackFlag1::DOUBLE_DAMAGE_CHANCE);
					break;
				case BonusType::DEATH_STARE:
					setflag(StackFlag1::DEATH_STARE);
					break;
				case BonusType::NOT_ACTIVE:
					if(cstack->unitType()->getId() != CreatureID::AMMO_CART)
						setflag(StackFlag1::SLEEPING);
					break;
				default:
					break;
			}

			if(bonus->source == BonusSource::SPELL_EFFECT)
			{
				switch(bonus->sid.as<SpellID>())
				{
					case SpellID::AGE:
						setflag(StackFlag2::AGE);
						break;
					case SpellID::BIND:
						setflag(StackFlag2::BIND);
						break;
					case SpellID::BLIND:
					case SpellID::PARALYZE:
						setflag(StackFlag2::BLIND);
						break;
					case SpellID::CURSE:
						setflag(StackFlag2::CURSE);
						break;
					case SpellID::POISON:
						setflag(StackFlag2::POISON);
						break;
					case SpellID::STONE_GAZE:
						setflag(StackFlag2::PETRIFY);
						break;
					case SpellID::WEAKNESS:
						setflag(StackFlag2::WEAKNESS);
						break;
					default:
						break;
				}
			}
		}
	}
};
}
