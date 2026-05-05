#include "BAI/v15/graph/nodes/unit.h"

#include "GameLibrary.h"
#include "AI/MMAI/common.h"
#include "bonuses/BonusEnum.h"
#include "constants/EntityIdentifiers.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace MMAI::BAI::V15::Graph::Nodes
{

int Unit::GetValue(const CCreature* creature)
{
    static const CreatureValues CREATURE_VALUES = initCreatureValues();

    if(!creature)
        throw std::runtime_error("GetValue: nullptr given");

    const auto& it = CREATURE_VALUES.find(creature->getId());

    if(it == CREATURE_VALUES.end())
        throw std::runtime_error(
            "GetValue: no value for creature with ID=" +
            std::to_string(creature->getIndex())
        );

    return it->second;
}

Unit::Unit(
    const CStack & cstack,
    const StatsContainer & statsContainer,
    bool isActive
) : cstack(cstack)
{
    const auto & stackStats = statsContainer.stackStats;

    int slot = CalculateSlot(cstack);
    alias = CalculateAlias(slot);

    processBonuses();

#ifdef ENABLE_ML
    if(cstack.creatureId().num > S15::CREATURE_ID_MAX)
        throw std::runtime_error("unknown creature id: " + std::to_string(cstack.creatureId().num));
#endif

    if(cstack.willMove())
    {
        setflag(StackFlag1::WILL_ACT);
        if(!cstack.waitedThisTurn)
            setflag(StackFlag1::CAN_WAIT);
    }

    if(cstack.ableToRetaliate())
        setflag(StackFlag1::CAN_RETALIATE);

    if(cstack.occupiedHex().isAvailable())
        setflag(StackFlag1::IS_WIDE);

    if(isActive)
        setflag(StackFlag1::IS_ACTIVE);

    shots = cstack.shots.available();

    auto valueOne = GetValue(cstack.unitType());

    if(cstack.isClone())
        valueOne *= 5;
    else if(cstack.unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER)
        valueOne = static_cast<int>(valueOne * 0.2);

    auto permille = [](int v1, int v2)
    {
        return static_cast<int>((1000LL * v1) / v2);
    };

    auto value = valueOne * cstack.getCount();

    setattr(UA::SIDE, EU(cstack.unitSide()));
    setattr(UA::SLOT, slot);
    setattr(UA::QUANTITY, cstack.getCount());
    setattr(UA::ATTACK, cstack.getAttack(shots > 0));
    setattr(UA::DEFENSE, cstack.getDefense(false));
    setattr(UA::SHOTS, shots);
    setattr(UA::DMG_MIN, cstack.getMinDamage(shots > 0));
    setattr(UA::DMG_MAX, cstack.getMaxDamage(shots > 0));
    setattr(UA::HP, cstack.getMaxHealth());
    setattr(UA::HP_LEFT, cstack.getFirstHPleft());
    setattr(UA::SPEED, cstack.getMovementRange());
    setattr(UA::VALUE_ONE, valueOne);
    setattr(UA::VALUE_REL, permille(value, statsContainer.bfieldValueNow));
    setattr(UA::VALUE_REL0, permille(value, statsContainer.bfieldValueStart));
    setattr(UA::VALUE_KILLED_REL, permille(stackStats.valueKilledNow, statsContainer.bfieldValuePrev));
    setattr(UA::VALUE_KILLED_ACC_REL0, permille(stackStats.valueKilledTotal, statsContainer.bfieldValueStart));
    setattr(UA::VALUE_LOST_REL, permille(stackStats.valueLostNow, statsContainer.bfieldValuePrev));
    setattr(UA::VALUE_LOST_ACC_REL0, permille(stackStats.valueLostTotal, statsContainer.bfieldValueStart));
    setattr(UA::DMG_DEALT_REL, permille(stackStats.dmgDealtNow, statsContainer.bfieldHpPrev));
    setattr(UA::DMG_DEALT_ACC_REL0, permille(stackStats.dmgDealtTotal, statsContainer.bfieldHpStart));
    setattr(UA::DMG_RECEIVED_REL, permille(stackStats.dmgReceivedNow, statsContainer.bfieldHpPrev));
    setattr(UA::DMG_RECEIVED_ACC_REL0, permille(stackStats.dmgReceivedTotal, statsContainer.bfieldHpStart));

    static_assert(EU(UA::_count) == 25, "whistleblower in case attributes change");

    finalize();
}

int Unit::getFlag(StackFlag1 sf) const
{
    return flag(sf);
}

int Unit::getFlag(StackFlag2 sf) const
{
    return flag(sf);
}

char Unit::getAlias() const
{
    return alias;
}

bool Unit::flag(StackFlag1 f) const
{
    return flags1.test(EU(f));
}

bool Unit::flag(StackFlag2 f) const
{
    return flags2.test(EU(f));
}

int Unit::CalculateSlot(const CStack & cstack)
{
    int slot = cstack.unitSlot();

    if(slot >= 0 && slot < 7)
        return slot;

    if(slot == SlotID::WAR_MACHINES_SLOT)
        return S15::STACK_SLOT_WARMACHINES;

    return S15::STACK_SLOT_SPECIAL;
}

// static
char Unit::CalculateAlias(int slot)
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

int Unit::CalculateValue(const CCreature* cr)
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

    for(const auto& bonus : *bonuses)
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

    /*
     * Some examples:
     *
     * Peasant=8            Gremlin=20          Imp=21             Pixie=24
     * Medusa=260           OgreMage=270        Crusader=293       Monk=308
     * BoneDragon=1425      Giant=1432          Hydra=1752         Devil=2344
     * ArchDevil=4484       GoldDragon=4518     Archangel=4763     Titan=5341
     * CrystalDragon=11347  RustDragon=12166    AzureDragon=17988
     *
     */
    auto res = static_cast<int>(std::round((a + b) * c * d));

    if(isMMAIVerbose())
    {
        std::cout
            << "MMAI_VERBOSE: " << res << " "
            << cr->getId().toEntity(LIBRARY)->getJsonKey()
            << " (a=" << a << ", b=" << b << ", c=" << c
            << ", d=" << d << ")\n";
    }

    return res;
}

Unit::CreatureValues Unit::initCreatureValues()
{
    CreatureValues values;

    for(const auto& creature : LIBRARY->creh->objects)
    {
        if(creature)
            values.try_emplace(creature->getId(), CalculateValue(creature.get()));
    }

    return values;
}

void Unit::setflag(StackFlag1 f)
{
    flags1.set(EU(f));
}

void Unit::setflag(StackFlag2 f)
{
    flags2.set(EU(f));
}

void Unit::finalize()
{
    setattr(UA::FLAGS1, static_cast<int>(flags1.to_ulong()));
    setattr(UA::FLAGS2, static_cast<int>(flags2.to_ulong()));
}

void Unit::processBonuses()
{
    auto bonuses = cstack.getAllBonuses(Selector::all);

    for(const auto& bonus : *bonuses)
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
                if(cstack.unitType()->getId() != CreatureID::AMMO_CART)
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

}
