/*
 * stack.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "CCreatureHandler.h"
#include "battle/IBattleInfoCallback.h"
#include "bonuses/BonusEnum.h"
#include "constants/EntityIdentifiers.h"

#include "BAI/v13/stack.h"
#include "schema/v13/constants.h"
#include "schema/v13/types.h"
#include <cmath>

namespace MMAI::BAI::V13 {
    using A = Schema::V13::StackAttribute;
    using F1 = Schema::V13::StackFlag1;
    using F2 = Schema::V13::StackFlag2;

    auto ValueCache = std::map<const CCreature*, float> {};

    // static
    int Stack::CalcValue(const CCreature* cr) {
        auto it = ValueCache.find(cr);
        if (it != ValueCache.end())
            return it->second;

        // Formula:
        // 10 * (A + B) * C * D1 * D2 * ... * Dn
        //

        // A = <offensive factor>
        // B = <defensive factor>
        // C = <speed factor>
        // D* = <bonus factor>

        auto att = cr->getBaseAttack();
        auto def = cr->getBaseDefense();
        auto dmg = (cr->getBaseDamageMax() + cr->getBaseDamageMin()) / 2.0;
        auto hp = cr->getBaseHitPoints();
        auto spd = cr->getBaseSpeed();
        auto shooter = cr->hasBonusOfType(BonusType::SHOOTER);
        auto bonuses = cr->getAllBonuses(Selector::all);

        auto a = 3*dmg * (1 + std::min(4.0, 0.05*att));
        auto b = hp / (1 - std::min(0.7, 0.025*def));
        auto c = spd ? std::log(spd*2) : 0.5;
        auto d = shooter ? 1.5 : 1.0;

        for (auto &bonus : *bonuses) {
            switch (bonus->type) {
            break; case BonusType::ADDITIONAL_ATTACK:           d += (shooter ? 0.5 : 0.3);
            break; case BonusType::ADDITIONAL_RETALIATION:      d += (bonus->val * 0.1);
            break; case BonusType::ATTACKS_ALL_ADJACENT:        d += 0.2;
            break; case BonusType::BLOCKS_RETALIATION:          d += 0.3;
            break; case BonusType::DEATH_STARE:                 d += (bonus->val * 0.02);   // 10% = 0.2
            break; case BonusType::DOUBLE_DAMAGE_CHANCE:        d += (bonus->val * 0.005);  // 20% = 0.1
            break; case BonusType::FLYING:                      d += 0.1;
            break; case BonusType::NO_MELEE_PENALTY:            d += 0.1;
            break; case BonusType::THREE_HEADED_ATTACK:         d += 0.05;
            break; case BonusType::TWO_HEX_ATTACK_BREATH:       d += 0.1;
            break; case BonusType::UNLIMITED_RETALIATIONS:      d += 0.2;
            break; case BonusType::ENEMY_DEFENCE_REDUCTION:     d += (bonus->val * 0.0025); // 40% = 0.1
            break; case BonusType::FIRE_SHIELD:                 d += (bonus->val * 0.003);  // 20% = 0.1
            break; case BonusType::LIFE_DRAIN:                  d += (bonus->val * 0.003);  // 100% = 0.3
            break; case BonusType::NO_DISTANCE_PENALTY:         d += 0.5;
            break; case BonusType::SPELL_LIKE_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                break; case SpellID::DEATH_CLOUD:               d += 0.2;
                // break; case SpellID::FIREBALL:
                }
            break; case BonusType::SPELL_AFTER_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                break; case SpellID::BLIND:
                       case SpellID::STONE_GAZE:
                       case SpellID::PARALYZE:                  d += (bonus->val * 0.01);   // 20% = 0.2
                break; case SpellID::BIND:                      d += (bonus->val * 0.001);  // 100% = 0.1
                break; case SpellID::WEAKNESS:                  d += (bonus->val * 0.001);  // 100% = 0.1
                break; case SpellID::AGE:                       d += (bonus->val * 0.005);  // 20% = 0.1
                break; case SpellID::CURSE:                     d += (bonus->val * 0.0025); // 20% = 0.05
                // break; case SpellID::DISEASE:
                // break; case SpellID::DISPEL:
                // break; case SpellID::POISON:
                // break; case SpellID::THUNDERBOLT:
                }
            // break; case BonusType::ACID_BREATH:
            // break; case BonusType::CHARGE_IMMUNITY:
            // break; case BonusType::CREATURE_ENCHANT_POWER:
            // break; case BonusType::CREATURE_SPELL_POWER:
            // break; case BonusType::DRAGON_NATURE:
            // break; case BonusType::ENCHANTER:
            // break; case BonusType::FEAR:
            // break; case BonusType::FEARLESS:
            // break; case BonusType::GARGOYLE:
            // break; case BonusType::HATE:
            // break; case BonusType::HEALER:
            // break; case BonusType::HP_REGENERATION:
            // break; case BonusType::KING:
            // break; case BonusType::LEVEL_SPELL_IMMUNITY:
            // break; case BonusType::LUCK:
            // break; case BonusType::MAGIC_MIRROR:
            // break; case BonusType::MAGIC_RESISTANCE:
            // break; case BonusType::MANA_CHANNELING:
            // break; case BonusType::MANA_DRAIN:
            // break; case BonusType::MIND_IMMUNITY:
            // break; case BonusType::MORALE:
            // break; case BonusType::MORE_DAMAGE_FROM_SPELL:
            // break; case BonusType::NO_LUCK:
            // break; case BonusType::NO_TERRAIN_PENALTY:
            // break; case BonusType::NO_WALL_PENALTY:
            // break; case BonusType::NON_LIVING:
            // break; case BonusType::NOT_ACTIVE:
            // break; case BonusType::RANDOM_SPELLCASTER:
            // break; case BonusType::REBIRTH:
            // break; case BonusType::RETURN_AFTER_STRIKE:
            // break; case BonusType::SHOOTER:
            // break; case BonusType::SIEGE_WEAPON:
            // break; case BonusType::SPECIAL_CRYSTAL_GENERATION:
            // break; case BonusType::SPECIFIC_SPELL_POWER:
            // break; case BonusType::SPELL_DAMAGE_REDUCTION:
            // break; case BonusType::SPELL_IMMUNITY:
            // break; case BonusType::SPELL_RESISTANCE_AURA:
            // break; case BonusType::SPELL_SCHOOL_IMMUNITY:
            // break; case BonusType::SPELLCASTER:
            // break; case BonusType::UNDEAD:
            // break; case BonusType::VISIONS:
            }
        }

        // Multiply by 10 to reduce the integer rounding for weak units
        // (e.g. peasant 7.48 => 8 is a lot, 74.8 => 75 is OK)
        ValueCache[cr] = static_cast<int>(std::round(10 * (a + b) * c * d));
        // std::cout << "\n" << ValueCache[cr] << " " << cr->getNameSingularTextID() << "(a=" << a << ", b=" << b << ", c=" << c << ", d=" << d << ")\n";
        return ValueCache[cr];
    }

    // static
    std::pair<BitQueue, int> Stack::QBits(const CStack* cstack, const Queue& vec) {
        BitQueue q;
        int pos = -1;
        if (vec.size() != STACK_QUEUE_SIZE)
            throw std::runtime_error("Unexpected queue size: " + std::to_string(vec.size()));

        for (auto i = 0; i < vec.size(); ++i) {
            if (vec[i] == cstack->unitId()) {
                q.set(i);
                if (pos < 0) pos = i;
            }
        }

        return {q, pos};
    }


    Stack::Stack(
        const CStack* cstack_,
        Queue &q,
        const GlobalStats* ogstats,
        const GlobalStats* gstats,
        const Stats stats,
        const ReachabilityInfo rinfo_,
        bool blocked,
        bool blocking,
        DamageEstimation estdmg
    ) : cstack(cstack_)
      , rinfo(rinfo_)
    {
        // XXX: NULL attrs are used only for non-existing stacks
        // => don't fill with null here (as opposed to attrs in Hex)

        int slot = cstack->unitSlot();
        if (slot >= 0 && slot < 7) {
            alias = slot + '0';
        } else if (slot == SlotID::WAR_MACHINES_SLOT) {
            // "machine" slot
            alias = 'M';
            slot = STACK_SLOT_WARMACHINES;
        } else {
            // "special" slot
            // SlotID::SUMMONED_SLOT_PLACEHOLDER
            // SlotID::COMMANDER_SLOT_PLACEHOLDER
            alias = 'S';
            slot = STACK_SLOT_SPECIAL;
        }

        // queue needs to be set first to determine if stack is active
        auto [qbits, pos] = QBits(cstack, q);
        qposFirst = pos; // for comparing two positions

        auto bonuses = cstack->getAllBonuses(Selector::all);

        // XXX: config/creatures/<faction>.json is misleading
        //      (for example, no creature has NO_MELEE_PENALTY bonus there)
        //      The source of truth is the original H3 data files
        //      (referred to as CRTRAITS.TXT file, see CCreatureHandler::loadLegacyData)
        for (auto &bonus : *bonuses) {
            switch (bonus->type) {
            /* 100% */
            // break; case BonusType::LUCK: {
            //     if (bonus->valType == BonusValueType::INDEPENDENT_MAX)
            //         minluck = bonus->val;
            //     else if (bonus->valType == BonusValueType::INDEPENDENT_MIN)
            //         maxluck = bonus->val;  // e.g. hourglass of the evil hour
            //     else
            //         addattr(A::LUCK, bonus->val);
            // }

            /* 86% (127 creatures) */
            // break; case BonusType::MORALE: {
            //     if (bonus->valType == BonusValueType::INDEPENDENT_MAX)
            //         minmorale = bonus->val;  // e.g. minotaur
            //     else if (bonus->valType == BonusValueType::INDEPENDENT_MIN)
            //         maxmorale = bonus->val;  // e.g. spirit of oppression
            //     else
            //         addattr(A::MORALE, bonus->val);
            // }

            // break; case BonusType::NO_LUCK: minluck = maxluck = 0;
            // break; case BonusType::NO_MORALE: minmorale = maxmorale = 0;

            /* 28% (42 creatures) */
            break; case BonusType::FLYING: setflag(F1::FLYING);

            /* 20% (29 creatures), but overlaps with SHOTS */
            break; case BonusType::SHOOTER: setflag(F1::SHOOTER);

            /* 10% (15 creatures) undead */
            /* +4% (6 creatures) unliving */
            /* +all war machines */
            break; case BonusType::UNDEAD: setflag(F1::NON_LIVING);
            break; case BonusType::NON_LIVING: setflag(F1::NON_LIVING);
            break; case BonusType::SIEGE_WEAPON: setflag(F1::WAR_MACHINE);

            /* 8.8% (13 creatures) */
            break; case BonusType::BLOCKS_RETALIATION: setflag(F1::BLOCKS_RETALIATION);

            /* 5.4% (8 creatures) */
            break; case BonusType::NO_MELEE_PENALTY: setflag(F1::NO_MELEE_PENALTY);

            /* 5.4% (8 creatures) */
            break; case BonusType::TWO_HEX_ATTACK_BREATH: setflag(F1::TWO_HEX_ATTACK_BREATH);

            /* 2.7% (4 creatures) */
            break; case BonusType::ADDITIONAL_ATTACK: setflag(F1::ADDITIONAL_ATTACK);

            break; case BonusType::SPELL_AFTER_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                /* 1.4% (2 creatures) blind */
                /* +2.7% (4 creatures) petrify */
                /* +0.7% (1 creature) paralyze */
                break; case SpellID::BLIND: setflag(F2::BLIND_ATTACK);
                break; case SpellID::PARALYZE: setflag(F2::BLIND_ATTACK);
                break; case SpellID::STONE_GAZE: setflag(F2::PETRIFY_ATTACK);

                /* 1.4% (2 creatures) bind */
                break; case SpellID::BIND: setflag(F2::BIND_ATTACK);

                /* 1.4% (2 creatures) */
                break; case SpellID::WEAKNESS: setflag(F2::WEAKNESS_ATTACK);

                /* 1.4% (2 creatures) */
                // break; case SpellID::DISEASE: setflag(F2::DISEASE_ATTACK);

                /* 1.4% (2 creatures) */
                break; case SpellID::DISPEL: setflag(F2::DISPEL_ATTACK);
                break; case SpellID::DISPEL_HELPFUL_SPELLS: setflag(F2::DISPEL_ATTACK);

                /* 0.7% (1 creature) */
                break; case SpellID::POISON: setflag(F2::POISON_ATTACK);

                /* 2% (3 creatures) */
                break; case SpellID::CURSE: setflag(F2::CURSE_ATTACK);

                /* 0.7% (1 creature) */
                break; case SpellID::AGE: setflag(F2::AGE_ATTACK);

                /* 0.7% (1 creature) */
                // break; case SpellID::THUNDERBOLT:
                }

            /* 4% (6 creatures); but requires extra "cast" action */
            // break; case BonusType::SPELLCASTER:

            /* 2% (3 creatures) */
            break; case BonusType::SPELL_LIKE_ATTACK:
                switch(bonus->subtype.as<SpellID>()) {
                break; case SpellID::FIREBALL: setflag(F1::FIREBALL);
                break; case SpellID::DEATH_CLOUD: setflag(F1::DEATH_CLOUD);
                }

            /* 2% (3 creatures) */
            // XXX: being near a unicorn does NOT not give MAGIC_RESISTSNCE
            //      bonus -- must check neighbouring hexes manually and add
            //      the percentage here -- not worth the effort
            // break; case BonusType::MAGIC_RESISTANCE:

            /* 1.4% (2 creatures) all-around attack */
            /* +0.7% (1 creature) 3-headed attack */
            break; case BonusType::THREE_HEADED_ATTACK: setflag(F1::THREE_HEADED_ATTACK);
            break; case BonusType::ATTACKS_ALL_ADJACENT: setflag(F1::ALL_AROUND_ATTACK);

            /* 1.4% (2 creatures) */
            // break; case BonusType::CHARGE_IMMUNITY:

            /* 1.4% (2 creatures) */
            // break; case BonusType::JOUSTING:

            /* 1.4% (2 creatures) */
            // break; case BonusType::NO_WALL_PENALTY:

            /* 1.4% (2 creatures) */
            break; case BonusType::RETURN_AFTER_STRIKE: setflag(F1::RETURN_AFTER_STRIKE);

            /* 0.02% (2 creatures per hate (1.4%) but only vs. 2 specific creatures (1.4%)) */
            // break; case BonusType::HATE:

            /* 0.7% (1 creature), or via artifact */
            // break; case BonusType::FREE_SHOOTING:

            /* 0.3% (4 creatures (2.7%) but only vs. 18 caster creatures (12%)) */
            // break; case BonusType::SPELL_RESISTANCE_AURA:

            /* 0.5% (6 creatures (4%) but only vs. 18 caster creatures (12%)) */
            // break; case BonusType::LEVEL_SPELL_IMMUNITY:

            /* <0.1% (4 creatures (2.7%) but only vs. 2 dmg-casting creatures (1.4%)) */
            // break; case BonusType::SPELL_DAMAGE_REDUCTION:

            /* <0.1% (2 creatures (1.4%) but only vs 2 mind-casting creatures (1.4%)) */
            // break; case BonusType::MIND_IMMUNITY:

            /* 1.4% (2 creatures) */
            break; case BonusType::ENEMY_DEFENCE_REDUCTION: setflag(F1::ENEMY_DEFENCE_REDUCTION);

            /* 0.7% (1 creature) */
            // break; case BonusType::FIRE_SHIELD: setflag(F2::FIRE_SHIELD);

            /* 0.7% (1 creature) */
            break; case BonusType::LIFE_DRAIN: setflag(F1::LIFE_DRAIN);

            /* 0.7% (1 creature) */
            break; case BonusType::DOUBLE_DAMAGE_CHANCE: setflag(F1::DOUBLE_DAMAGE_CHANCE);

            /* 0.7% (1 creature); but requires extra "cast" action */
            // break; case BonusType::RANDOM_SPELLCASTER:

            /* 0.7% (1 creature) */
            // break; case BonusType::NO_DISTANCE_PENALTY:

            /* 0.1% (7 creatures can cast this spell with a 20% chance). Overlaps with QUEUE_POS */
            break; case BonusType::NOT_ACTIVE:
                if (cstack->unitType()->getId() != CreatureID::AMMO_CART)
                    setflag(F1::SLEEPING);


            /* <1% (petrified units, but only 2 other creatures may cast petrify wuth a small chance)) */
            /* + <0.1% (shielded units, but only 1 other creature may cast shield with a small chance)) */
            /* ? commanding hero with armorer skill */
            // break; case BonusType::GENERAL_DAMAGE_REDUCTION: {
            //     auto st = bonus->subtype.as<BonusCustomSubtype>();
            //     if (st == BonusCustomSubtype::damageTypeAll) {
            //         addattr(A::MELEE_DAMAGE_REDUCTION, bonus->val);
            //         addattr(A::RANGED_DAMAGE_REDUCTION, bonus->val);
            //     } else if (st == BonusCustomSubtype::damageTypeRanged) {
            //         addattr(A::RANGED_DAMAGE_REDUCTION, bonus->val);
            //     } else if (st == BonusCustomSubtype::damageTypeMelee) {
            //         addattr(A::MELEE_DAMAGE_REDUCTION, bonus->val);
            //     }
            // }

            /* <1% (blinded/paralyzed units, but only 3 other creature may cast petrify with a small chance)) */
            /* <0.1% (shielded units, but only 1 other creature may cast (air) shield with a small chance)) */
            // break; case BonusType::GENERAL_ATTACK_REDUCTION: {
            //     auto st = bonus->subtype.as<BonusCustomSubtype>();
            //     if (st == BonusCustomSubtype::damageTypeAll) {
            //         addattr(A::MELEE_ATTACK_REDUCTION, bonus->val);
            //         addattr(A::RANGED_ATTACK_REDUCTION, bonus->val);
            //     } else if (st == BonusCustomSubtype::damageTypeRanged) {
            //         addattr(A::RANGED_ATTACK_REDUCTION, bonus->val);
            //     } else if (st == BonusCustomSubtype::damageTypeMelee) {
            //         addattr(A::MELEE_ATTACK_REDUCTION, bonus->val);
            //     }
            // }

            /* ? any defending unit, but it's mostly overlapping with defense skill */
            // break; case BonusType::DEFENSIVE_STANCE:

            /* no creature casts this spell */
            // break; case BonusType::HYPNOTIZED:

            /* <1% (blinded/paralyzed units, but only 3 other creatures may cast petrify with a small chance)) */
            // break; case BonusType::NO_RETALIATION:

            /* <1% only 1 creature may cast this spell with a small chance */
            // break; case BonusType::MAGIC_MIRROR:

            /* no creature casts this spell (berserk) */
            // break; case BonusType::ATTACKS_NEAREST_CREATURE:

            /* no creature casts this spell */
            // break; case BonusType::FORGETFULL:

            /* 0.7% (1 creature) */
            break; case BonusType::DEATH_STARE: setflag(F1::DEATH_STARE);

            /* <0.1% only 1 creature may cast this spell with a small chance */
            // break; case BonusType::POISON: setflag(F2::POISONED);

            /* 0.7% (1 creature) */
            // break; case BonusType::ACID_BREATH:

            /* 0.7% (1 creature) */
            // break; case BonusType::REBIRTH:

            /* 0.1% (<5% per school, but only vs. <5 caster creatures per scoll) */
            // break; case BonusType::SPELL_SCHOOL_IMMUNITY: {
            //     auto st = bonus->subtype.as<SpellSchool>();
            //     if (st == SpellSchool::ANY) {
            //         addattr(A::AIR_DAMAGE_REDUCTION, 100);
            //         addattr(A::WATER_DAMAGE_REDUCTION, 100);
            //         addattr(A::FIRE_DAMAGE_REDUCTION, 100);
            //         addattr(A::EARTH_DAMAGE_REDUCTION, 100);
            //     }
            //     else if (st == SpellSchool::AIR) addattr(A::AIR_DAMAGE_REDUCTION, 100);
            //     else if (st == SpellSchool::WATER) addattr(A::WATER_DAMAGE_REDUCTION, 100);
            //     else if (st == SpellSchool::FIRE) addattr(A::FIRE_DAMAGE_REDUCTION, 100);
            //     else if (st == SpellSchool::EARTH) addattr(A::EARTH_DAMAGE_REDUCTION, 100);
            // }
            }

            if (bonus->source == BonusSource::SPELL_EFFECT) {
                switch (bonus->sid.as<SpellID>()) {
                // break; case SpellID::ACID_BREATH_DAMAGE: setflag();
                // break; case SpellID::ACID_BREATH_DEFENSE: setflag();
                break; case SpellID::AGE: setflag(F2::AGE);
                // break; case SpellID::AIR_SHIELD: setflag();
                // break; case SpellID::ANTI_MAGIC: setflag();
                // break; case SpellID::BERSERK: setflag();
                break; case SpellID::BIND: setflag(F2::BIND);
                // break; case SpellID::BLESS: setflag();
                break; case SpellID::BLIND: setflag(F2::BLIND);
                // break; case SpellID::BLOODLUST: setflag();
                // break; case SpellID::CLONE: setflag();
                break; case SpellID::CURSE: setflag(F2::CURSE);
                // break; case SpellID::DISEASE: setflag();
                // break; case SpellID::HASTE: setflag();
                break; case SpellID::PARALYZE: setflag(F2::BLIND);
                break; case SpellID::POISON: setflag(F2::POISON);
                // break; case SpellID::PRAYER: setflag();
                // break; case SpellID::SHIELD: setflag();
                // break; case SpellID::SLOW: setflag();
                break; case SpellID::STONE_GAZE: setflag(F2::PETRIFY);
                // break; case SpellID::STONE_SKIN: setflag();
                break; case SpellID::WEAKNESS: setflag(F2::WEAKNESS);
                }
            }
        }

        // double avgdmg = 0.5*(estdmg.damage.max + estdmg.damage.min);
        // auto dmgPermilleHP = std::clamp<int>(std::round(1000ll * avgdmg / cstack->getAvailableHealth()), 0, 1000);

        if (cstack->willMove()) {
            setflag(F1::WILL_ACT);
            // XXX: do NOT use cstack->waited()
            //      (it returns cstack->waiting, which becomes false after acting)
            if (!cstack->waitedThisTurn)
                setflag(F1::CAN_WAIT);
        }

        if (cstack->ableToRetaliate())
            setflag(F1::CAN_RETALIATE);

        if (blocked)
            setflag(F1::BLOCKED);

        if (blocking)
            setflag(F1::BLOCKING);

        if (cstack->occupiedHex().isAvailable())
            setflag(F1::IS_WIDE);

        // std::bitset's public semantics are architecture-independent.
        // operator<< prints bits from MSB to LSB,
        // e.g. std::bitset<8>(1) prints 00000001
        // b[i] indexes from the least-significant bit: b[0] == 1, b[1..7] == 0
        if (qbits.test(0))
            setflag(F1::IS_ACTIVE);

        shots = cstack->shots.available();

        int cid = cstack->creatureId().num;
        if (cid > Schema::V13::CREATURE_ID_MAX) {
            // This would trigger every time a non-vanilla H3 creature becomes active for MMAI
            // logAi->debug("MMAI was not trained with this creature: id=%d (%s)", cid, cstack->getDescription());
            #ifdef ENABLE_ML
                throw std::runtime_error("unknown creature id: " + std::to_string(cid));
            #endif
            cid = 122; // this is a "NOT USED (1)" creature
        }

        auto valueOne = CalcValue(cstack->unitType());

        // std::cout << "[" << cstack->unitType()->getNameSingularTextID() << "] VALUE_ONE:" << valueOne << ", VALUE: " << value << ", bf_valueNow: " << bf_valueNow << "\n";
        auto permille = [](int v1, int v2) {
            // ll (long long) ensures long is 64-bit even on 32-bit systems
            return (1000ll * v1) / v2;
        };

        // std::cout << "[" << cstack->unitType()->getNameSingularTextID() << "] lgstats->valueNow:" << lgstats->valueNow << ", rgstats->valueNow: " << rgstats->valueNow << "\n";
        auto bf_valueNow = gstats->attr(GA::BFIELD_VALUE_NOW_ABS);
        auto bf_valuePrev = ogstats->attr(GA::BFIELD_VALUE_NOW_ABS);
        auto bf_valueStart = gstats->attr(GA::BFIELD_VALUE_START_ABS);
        auto bf_hpPrev = ogstats->attr(GA::BFIELD_HP_NOW_ABS);
        auto bf_hpStart = gstats->attr(GA::BFIELD_HP_START_ABS);
        auto value = valueOne * cstack->getCount();

        setattr(A::SIDE, EI(cstack->unitSide()));
        setattr(A::SLOT, slot);
        setattr(A::QUANTITY, cstack->getCount());
        setattr(A::ATTACK, cstack->getAttack(shots > 0));
        setattr(A::DEFENSE, cstack->getDefense(false));
        setattr(A::SHOTS, shots);
        setattr(A::DMG_MIN, cstack->getMinDamage(shots > 0));
        setattr(A::DMG_MAX, cstack->getMaxDamage(shots > 0));
        setattr(A::HP, cstack->getMaxHealth());
        setattr(A::HP_LEFT, cstack->getFirstHPleft());
        setattr(A::SPEED, cstack->getMovementRange());
        setattr(A::QUEUE, qbits.to_ulong());
        setattr(A::VALUE_ONE, valueOne);
        setattr(A::VALUE_REL,             permille(value, bf_valueNow));
        setattr(A::VALUE_REL0,            permille(value, bf_valueStart));
        setattr(A::VALUE_KILLED_REL,      permille(stats.valueKilledNow, bf_valuePrev));
        setattr(A::VALUE_KILLED_ACC_REL0, permille(stats.valueKilledTotal, bf_valueStart));
        setattr(A::VALUE_LOST_REL,        permille(stats.valueLostNow, bf_valuePrev));
        setattr(A::VALUE_LOST_ACC_REL0,   permille(stats.valueLostTotal, bf_valueStart));
        setattr(A::DMG_DEALT_REL,         permille(stats.dmgDealtNow, bf_hpPrev));
        setattr(A::DMG_DEALT_ACC_REL0,    permille(stats.dmgDealtTotal, bf_hpStart));
        setattr(A::DMG_RECEIVED_REL,      permille(stats.dmgReceivedNow, bf_hpPrev));
        setattr(A::DMG_RECEIVED_ACC_REL0, permille(stats.dmgReceivedTotal, bf_hpStart));

        // The attrs set above must match the total count -2 (which are the FLAGS1 and FLAGS2)
        static_assert(EI(A::_count) == 23 + 2, "whistleblower in case attributes change");

        // setattr(A::CREATURE_ID, cid);
        // setattr(A::ESTIMATED_DMG, dmgPermilleHP);

        finalize();
    }

    const StackAttrs& Stack::getAttrs() const {
        return attrs;
    }

    char Stack::getAlias() const {
        return alias;
    }

    int Stack::getAttr(StackAttribute a) const {
        return attr(a);
    }

    int Stack::getFlag(StackFlag1 sf) const {
        return flag(sf);
    }

    int Stack::getFlag(StackFlag2 sf) const {
        return flag(sf);
    }

    bool Stack::flag(StackFlag1 f) const {
        return flags1.test(EI(f));
    };

    bool Stack::flag(StackFlag2 f) const {
        return flags2.test(EI(f));
    };

    int Stack::attr(StackAttribute a) const {
        return attrs.at(EI(a));
    };

    void Stack::setflag(StackFlag1 f) {
        flags1.set(EI(f));
    };

    void Stack::setflag(StackFlag2 f) {
        flags2.set(EI(f));
    };

    void Stack::setattr(StackAttribute a, int value) {
        attrs.at(EI(a)) = value;
    };

    void Stack::addattr(StackAttribute a, int value) {
        attrs.at(EI(a)) += value;
    };

    std::map<const CCreature*, int> warned {};

    void Stack::finalize() {
        setattr(A::FLAGS1, flags1.to_ulong());
        setattr(A::FLAGS2, flags2.to_ulong());
    }
}
