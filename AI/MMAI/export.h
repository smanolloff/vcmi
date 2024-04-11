// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#pragma once

#include <map>
#include <string>
#include <cassert>
#include <stdexcept>
#include <functional>
#include <cmath>

/*****
****** THIS FILE LIVES IN:
******
****** vcmi/AI/MMAI/export.h
******
*****/

namespace MMAI::Export {
    #ifndef DLL_EXPORT
    #define DLL_EXPORT __attribute__((visibility("default")))
    #endif

    #define EI(enum_value) static_cast<int>(enum_value)

    using ErrMask = uint16_t;
    enum class ErrType : int {
        ALREADY_WAITED,
        MOVE_SELF,
        HEX_UNREACHABLE,
        HEX_BLOCKED,
        HEX_MELEE_NA,
        STACK_NA,
        STACK_DEAD,
        STACK_INVALID,
        CANNOT_SHOOT,
        FRIENDLY_FIRE,
        INVALID_DIR,
    };

    static const std::map<const ErrType, std::tuple<const ErrMask, const std::string, const std::string>> ERRORS = {
        {ErrType::ALREADY_WAITED,    {ErrMask(1 << 0), "ERR_ALREADY_WAITED", "already waited this turn"}},
        {ErrType::MOVE_SELF,         {ErrMask(1 << 1), "ERR_MOVE_SELF", "cannot move to self unless attacking"}},
        {ErrType::HEX_UNREACHABLE,   {ErrMask(1 << 2), "ERR_HEX_UNREACHABLE", "target hex is unreachable"}},
        {ErrType::HEX_BLOCKED,       {ErrMask(1 << 3), "ERR_HEX_BLOCKED", "target hex is blocked"}},
        {ErrType::HEX_MELEE_NA,      {ErrMask(1 << 4), "ERR_HEX_MELEE_NA", "hex to perform the melee attack from is N/A"}},
        {ErrType::STACK_NA,          {ErrMask(1 << 5), "ERR_STACK_NA", "no stack at target"}},
        {ErrType::STACK_DEAD,        {ErrMask(1 << 6), "ERR_STACK_DEAD", "target stack is dead"}},
        {ErrType::STACK_INVALID,     {ErrMask(1 << 7), "ERR_STACK_INVALID", "target stack is invalid (turret?)"}},
        {ErrType::CANNOT_SHOOT,      {ErrMask(1 << 8), "ERR_CANNOT_SHOOT", "ranged attack not possible"}},
        {ErrType::FRIENDLY_FIRE,     {ErrMask(1 << 9), "ERR_FRIENDLY_FIRE", "attempted to attack a friendly stack"}},
        {ErrType::INVALID_DIR,       {ErrMask(1 << 10), "ERR_INVALID_DIR", "attempted to attack from a T/B direction with a 1-hex stack"}},
    };

    // Possible values for HEX_ATTACKABLE_BY_ENEMY_STACK_* attributes
    enum class DmgMod {
        ZERO,               // temp. state (e.g. can't reach, is blinded, etc.)
        HALF,
        FULL,
        _count
    };

    enum class Encoding : int {
        NUMERIC,            // see encodeNumeric
        NUMERIC_SQRT,       // see encodeNumericSqrt
        BINARY,             // see encodeBinary
        CATEGORICAL,        // see encodeCategorical
        FLOATING            // see encodeCategorical
    };

    enum class Attribute : int {
        HEX_Y_COORD,                            // 0..10
        HEX_X_COORD,                            // 0..14
        HEX_STATE,                              // see HexState
        HEX_REACHABLE_BY_ACTIVE_STACK,          // can active stack reach it?
        HEX_REACHABLE_BY_FRIENDLY_STACK_0,      // can friendly stack0 reach it?
        HEX_REACHABLE_BY_FRIENDLY_STACK_1,      //
        HEX_REACHABLE_BY_FRIENDLY_STACK_2,      //
        HEX_REACHABLE_BY_FRIENDLY_STACK_3,      //
        HEX_REACHABLE_BY_FRIENDLY_STACK_4,      //
        HEX_REACHABLE_BY_FRIENDLY_STACK_5,      //
        HEX_REACHABLE_BY_FRIENDLY_STACK_6,      //
        HEX_REACHABLE_BY_ENEMY_STACK_0,         // can enemy stack0 reach it?
        HEX_REACHABLE_BY_ENEMY_STACK_1,         //
        HEX_REACHABLE_BY_ENEMY_STACK_2,         //
        HEX_REACHABLE_BY_ENEMY_STACK_3,         //
        HEX_REACHABLE_BY_ENEMY_STACK_4,         //
        HEX_REACHABLE_BY_ENEMY_STACK_5,         //
        HEX_REACHABLE_BY_ENEMY_STACK_6,         //
        HEX_MELEEABLE_BY_ACTIVE_STACK,          // can active stack melee attack hex? (0=no, 1=half, 2=full dmg)
        HEX_MELEEABLE_BY_FRIENDLY_STACK_0,      // can friendly stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_MELEEABLE_BY_FRIENDLY_STACK_1,      //
        HEX_MELEEABLE_BY_FRIENDLY_STACK_2,      //
        HEX_MELEEABLE_BY_FRIENDLY_STACK_3,      //
        HEX_MELEEABLE_BY_FRIENDLY_STACK_4,      //
        HEX_MELEEABLE_BY_FRIENDLY_STACK_5,      //
        HEX_MELEEABLE_BY_FRIENDLY_STACK_6,      //
        HEX_MELEEABLE_BY_ENEMY_STACK_0,         // can enemy stack0 melee attack hex? (0=no, 1=half, 2=full dmg)
        HEX_MELEEABLE_BY_ENEMY_STACK_1,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_2,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_3,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_4,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_5,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_6,         //
        HEX_SHOOTABLE_BY_ACTIVE_STACK,          // can active stack shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_0,      // can friendly stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_1,      //
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_2,      //
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_3,      //
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_4,      //
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_5,      //
        HEX_SHOOTABLE_BY_FRIENDLY_STACK_6,      //
        HEX_SHOOTABLE_BY_ENEMY_STACK_0,         // can enemy stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_SHOOTABLE_BY_ENEMY_STACK_1,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_2,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_3,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_4,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_5,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_6,         //
        HEX_NEXT_TO_ACTIVE_STACK,               // is active stack on a nearby hex?
        HEX_NEXT_TO_FRIENDLY_STACK_0,           // is friendly stack0 on a nearby hex?
        HEX_NEXT_TO_FRIENDLY_STACK_1,           //
        HEX_NEXT_TO_FRIENDLY_STACK_2,           //
        HEX_NEXT_TO_FRIENDLY_STACK_3,           //
        HEX_NEXT_TO_FRIENDLY_STACK_4,           //
        HEX_NEXT_TO_FRIENDLY_STACK_5,           //
        HEX_NEXT_TO_FRIENDLY_STACK_6,           //
        HEX_NEXT_TO_ENEMY_STACK_0,              // is enemy stack0 on a nearby hex?
        HEX_NEXT_TO_ENEMY_STACK_1,              //
        HEX_NEXT_TO_ENEMY_STACK_2,              //
        HEX_NEXT_TO_ENEMY_STACK_3,              //
        HEX_NEXT_TO_ENEMY_STACK_4,              //
        HEX_NEXT_TO_ENEMY_STACK_5,              //
        HEX_NEXT_TO_ENEMY_STACK_6,              //
        STACK_QUANTITY,                         //
        STACK_ATTACK,                           //
        STACK_DEFENSE,                          //
        STACK_SHOTS,                            //
        STACK_DMG_MIN,                          //
        STACK_DMG_MAX,                          //
        STACK_HP,                               //
        STACK_HP_LEFT,                          //
        STACK_SPEED,                            //
        STACK_WAITED,                           //
        STACK_QUEUE_POS,                        // 0=active stack
        STACK_RETALIATIONS_LEFT,                //
        STACK_SIDE,                             // 0=attacker, 1=defender
        STACK_SLOT,                             // 0..6
        STACK_CREATURE_TYPE,                    // 0..144
        STACK_AI_VALUE_TENTH,                   // divided by 10 (ie. imp=5, not 50)
        STACK_IS_ACTIVE,                        //
        STACK_IS_WIDE,                          // 1=2-hex
        STACK_FLYING,                           // TODO
        STACK_NO_MELEE_PENALTY,                 // TODO
        STACK_TWO_HEX_ATTACK_BREATH,            // TODO
        STACK_BLOCKS_RETALIATION,               // TODO
        STACK_DEFENSIVE_STANCE,                 // TODO? means temp def bonus
        // STACK_MORALE,                           // not used
        // STACK_LUCK,                             // not used
        // STACK_FREE_SHOOTING,                    // not used
        // STACK_CHARGE_IMMUNITY,                  // not used
        // STACK_ADDITIONAL_ATTACK,                // not used
        // STACK_UNLIMITED_RETALIATIONS,           // not used
        // STACK_JOUSTING,                         // not used
        // STACK_HATE,                             // not used
        // STACK_KING,                             // not used
        // STACK_MAGIC_RESISTANCE,                 // not used
        // STACK_SPELL_RESISTANCE_AURA,            // not used
        // STACK_LEVEL_SPELL_IMMUNITY,             // not used
        // STACK_SPELL_DAMAGE_REDUCTION,           // not used
        // STACK_NON_LIVING,                       // not used
        // STACK_RANDOM_SPELLCASTER,               // not used
        // STACK_SPELL_IMMUNITY,                   // not used
        // STACK_THREE_HEADED_ATTACK,              // not used
        // STACK_FIRE_IMMUNITY,                    // not used
        // STACK_WATER_IMMUNITY,                   // not used
        // STACK_EARTH_IMMUNITY,                   // not used
        // STACK_AIR_IMMUNITY,                     // not used
        // STACK_MIND_IMMUNITY,                    // not used
        // STACK_FIRE_SHIELD,                      // not used
        // STACK_UNDEAD,                           // not used
        // STACK_HP_REGENERATION,                  // not used
        // STACK_LIFE_DRAIN,                       // not used
        // STACK_DOUBLE_DAMAGE_CHANCE,             // not used
        // STACK_RETURN_AFTER_STRIKE,              // not used
        // STACK_ENEMY_DEFENCE_REDUCTION,          // not used
        // STACK_GENERAL_DAMAGE_REDUCTION,         // not used
        // STACK_GENERAL_ATTACK_REDUCTION,         // not used
        // STACK_ATTACKS_ALL_ADJACENT,             // not used
        // STACK_NO_DISTANCE_PENALTY,              // not used
        // STACK_NO_RETALIATION,                   // not used
        // STACK_NOT_ACTIVE,                       // not used
        // STACK_DEATH_STARE,                      // not used
        // STACK_POISON,                           // not used
        // STACK_ACID_BREATH,                      // not used
        // STACK_REBIRTH,                          // not used
        _count
    };

    using E4 = std::tuple<Attribute, Encoding, int, int>;


    /*
     * Begin compile-time magic
     */

    // Compile time int(sqrt(x))
    // https://stackoverflow.com/a/27709195
    template <typename T> constexpr T Sqrt(T x, T lo, T hi) {
        if (lo == hi) return lo;
        const T mid = (lo + hi + 1) / 2;
        return (x / mid < mid) ? Sqrt<T>(x, lo, mid - 1) : Sqrt(x, mid, hi);
    }
    template <typename T> constexpr T CTSqrt(T x) { return Sqrt<T>(x, 0, x / 2 + 1); }

    // Compile time int(log(x, 2))
    // https://stackoverflow.com/a/23784921
    constexpr unsigned Log2(unsigned n) {
        return n <= 1 ? 0 : 1 + Log2((n + 1) / 2);
    }

    constexpr E4 ToE4(Attribute a, Encoding e, int vmax) {
        switch(e) {
        break; case Encoding::NUMERIC:      return E4{a, e, vmax, vmax};
        break; case Encoding::NUMERIC_SQRT: return E4{a, e, static_cast<int>(CTSqrt(vmax)), vmax};
        break; case Encoding::BINARY:       return E4{a, e, static_cast<int>(Log2(vmax)), vmax};
        break; case Encoding::CATEGORICAL:  return E4{a, e, vmax, vmax};
        break; case Encoding::FLOATING:     return E4{a, e, 1, vmax};
        break; default:
            throw std::runtime_error("Unexpected encoding: " + std::to_string(EI(e)));
        }
    }

    /*
     * End compile-time magic
     */

    using A = Attribute;
    using E = Encoding;
    using HexEncoding = std::array<E4, EI(Attribute::_count)>;

    // Container of {a, e, n, vmax} tuples (see OneHot)
    //
    // XXX: It contains tuple primitives instead of OneHot objects, because
    //      this enables compile-time checks for preventing human errors.
    constexpr HexEncoding HEX_ENCODING {
        ToE4(A::HEX_Y_COORD,                       E::CATEGORICAL,  11),    //
        ToE4(A::HEX_X_COORD,                       E::CATEGORICAL,  15),    //
        ToE4(A::HEX_STATE,                         E::CATEGORICAL,  3),     // see hexstate.h
        ToE4(A::HEX_REACHABLE_BY_ACTIVE_STACK,     E::CATEGORICAL,  2),     // can active stack reach it?
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0, E::CATEGORICAL,  2),     // can friendly stack0 reach it?
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_1, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_2, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_3, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_4, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_5, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_FRIENDLY_STACK_6, E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_0,    E::CATEGORICAL,  2),     // can enemy stack0 reach it?
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_1,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_2,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_3,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_4,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_5,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_REACHABLE_BY_ENEMY_STACK_6,    E::CATEGORICAL,  2),     //
        ToE4(A::HEX_MELEEABLE_BY_ACTIVE_STACK,     E::CATEGORICAL,  3),     // can active stack melee at this hex? (0=no, 1=half, 2=full dmg)
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_0, E::CATEGORICAL,  3),     // can friendly stack0 melee attack hex? (0=no, 1=half, 2=full dmg)
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_1, E::CATEGORICAL,  3),     // XXX: MELEEABLE hex does NOT mean there's a stack there (could even be an obstacle)
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_2, E::CATEGORICAL,  3),     //      It's all about whether the stack can reach a NEARBY hex
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_3, E::CATEGORICAL,  3),     //      Should it be false for obstacles?
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_4, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_5, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_FRIENDLY_STACK_6, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_0,    E::CATEGORICAL,  3),     // can enemy stack0 melee attack hex? XXX: see note above
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_1,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_2,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_3,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_4,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_5,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_MELEEABLE_BY_ENEMY_STACK_6,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ACTIVE_STACK,     E::CATEGORICAL,  3),     // can active stack shoot at this hex? (0=no, 1=half, 2=full dmg)
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_0, E::CATEGORICAL,  3),     // can friendly stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_1, E::CATEGORICAL,  3),     // XXX: SHOOTABLE hex does mean there's a stack there (could even be an obstacle)
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_2, E::CATEGORICAL,  3),     //      It's all about the distance between the shooter and this
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_3, E::CATEGORICAL,  3),     //      Should it be false for obstacles?
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_4, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_5, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_FRIENDLY_STACK_6, E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_0,    E::CATEGORICAL,  3),     // can enemy stack0 shoot at this hex? XXX: see note above
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_1,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_2,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_3,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_4,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_5,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_SHOOTABLE_BY_ENEMY_STACK_6,    E::CATEGORICAL,  3),     //
        ToE4(A::HEX_NEXT_TO_ACTIVE_STACK,          E::CATEGORICAL,  2),     // is active stack0 on a nearby hex?
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_0,      E::CATEGORICAL,  2),     // is friendly stack0 on a nearby hex?
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_1,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_2,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_3,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_4,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_5,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_FRIENDLY_STACK_6,      E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_0,         E::CATEGORICAL,  2),     // is enemy stack0 on a nearby hex?
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_1,         E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_2,         E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_3,         E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_4,         E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_5,         E::CATEGORICAL,  2),     //
        ToE4(A::HEX_NEXT_TO_ENEMY_STACK_6,         E::CATEGORICAL,  2),     //
        ToE4(A::STACK_QUANTITY,                    E::NUMERIC_SQRT, 1023),  // max for n=31 (32^2=1024)
        ToE4(A::STACK_ATTACK,                      E::NUMERIC_SQRT, 63),    // max for n=7 (8^2=64)
        ToE4(A::STACK_DEFENSE,                     E::NUMERIC_SQRT, 63),    // max for n=7 (8^2=64) - crystal dragon is 48 when defending
        ToE4(A::STACK_SHOTS,                       E::NUMERIC_SQRT, 35),    // max for n=5 (6^2=36) - sharpshooter is 32
        ToE4(A::STACK_DMG_MIN,                     E::NUMERIC_SQRT, 80),    // max for n=8 (9^2=81)
        ToE4(A::STACK_DMG_MAX,                     E::NUMERIC_SQRT, 80),    // max for n=8 (9^2=81)
        ToE4(A::STACK_HP,                          E::NUMERIC_SQRT, 840),   // max for n=28 (29^2=841) - crystal dragon is 800
        ToE4(A::STACK_HP_LEFT,                     E::NUMERIC_SQRT, 840),   // max for n=28 (29^2=841) - crystal dragon is 800
        ToE4(A::STACK_SPEED,                       E::NUMERIC,      23),    // phoenix is 22
        ToE4(A::STACK_WAITED,                      E::CATEGORICAL,  2),     //
        ToE4(A::STACK_QUEUE_POS,                   E::NUMERIC,      15),    // 0..14, 0=active stack
        ToE4(A::STACK_RETALIATIONS_LEFT,           E::NUMERIC,      3),     // inf is truncated to 2 (royal griffin)
        ToE4(A::STACK_SIDE,                        E::CATEGORICAL,  2),     // 0=attacker, 1=defender
        ToE4(A::STACK_SLOT,                        E::CATEGORICAL,  7),     // 0..6
        ToE4(A::STACK_CREATURE_TYPE,               E::CATEGORICAL,  145),   // 0..144 (incl.)
        ToE4(A::STACK_AI_VALUE_TENTH,              E::NUMERIC_SQRT, 3968),  // max for n=62 (63^2=3969) - crystal dragon is 3933
        ToE4(A::STACK_IS_ACTIVE,                   E::CATEGORICAL,  2),     //
        ToE4(A::STACK_IS_WIDE,                     E::CATEGORICAL,  2),     // is this a two-hex stack?
        ToE4(A::STACK_FLYING,                      E::CATEGORICAL,  2),     //
        ToE4(A::STACK_NO_MELEE_PENALTY,            E::CATEGORICAL,  2),     //
        ToE4(A::STACK_TWO_HEX_ATTACK_BREATH,       E::CATEGORICAL,  2),     //
        ToE4(A::STACK_BLOCKS_RETALIATION,          E::CATEGORICAL,  2),     //
        ToE4(A::STACK_DEFENSIVE_STANCE,            E::CATEGORICAL,  2),     //
    };

    /*
     * Begin compile-time magic
     */

    // The static asserts below are used below as a compile-time whistleblower
    // for breaking changes.

    // 1. Ensure all elements in HEX_ENCODING have been initialized.
    //    The index of the uninitialized element is returned.
    constexpr int UninitializedHexEncodingElements() {
        for (int i = 0; i < EI(Attribute::_count); i++) {
            if (HEX_ENCODING.at(i) == E4{}) return i;
        }
        return -1;
    }
    static_assert(UninitializedHexEncodingElements() == -1, "Uninitialized element at this index in HEX_ENCODING");

    // 2. Ensure HEX_ENCODING elements follow the Attribute enum value order.
    //    The index at which the order is violated is returned.
    constexpr int DisarrayedHexEncodingAttributes() {
        if (UninitializedHexEncodingElements() != -1) return -1;

        for (int i = 0; i < EI(Attribute::_count); i++)
            if (std::get<0>(HEX_ENCODING.at(i)) != Attribute(i)) return i;
        return -1;
    }
    static_assert(DisarrayedHexEncodingAttributes() == -1, "Attribute out of order at this index in HEX_ENCODING");

    // 3. Calculate proper ENCODED_STATE_SIZE_ONE_HEX literal
    constexpr int EncodedStateSizeOneHex() {
        int ret = 0;
        for (int i = 0; i < EI(Attribute::_count); i++)
            ret += std::get<2>(HEX_ENCODING[i]);
        return ret;
    }

    /*
     * End of compile-time magic
     */

    constexpr int STATE_SIZE_UNENCODED_ONE_HEX = EI(A::_count);
    constexpr int STATE_SIZE_UNENCODED = 165 * STATE_SIZE_UNENCODED_ONE_HEX;

    constexpr int STATE_SIZE_DEFAULT_ONE_HEX = EncodedStateSizeOneHex();
    constexpr int STATE_SIZE_DEFAULT = 165 * STATE_SIZE_DEFAULT_ONE_HEX;

    constexpr int STATE_SIZE_FLOAT_ONE_HEX = EI(A::_count); // 1 float per attribute
    constexpr int STATE_SIZE_FLOAT = 165 * STATE_SIZE_FLOAT_ONE_HEX;
    constexpr float STATE_VALUE_NA = -1;

    constexpr int N_UNSET = -1;
    constexpr int VALUE_NA_UNENCODED = -1;

    using State = std::vector<float>;
    using Action = int;

    // Indicates how state will be encoded:
    // * DEFAULT means use the encodings as specified in HexEncoding
    // * FLOAT means always encode as float (disregarding HexEncoding)
    // Needed in order to allow switching between the two types without
    // having to recompile.
    constexpr auto STATE_ENCODING_DEFAULT = "default";
    constexpr auto STATE_ENCODING_FLOAT = "float";

    // Arbitary int value that can be one-hot encoded
    extern "C" struct DLL_EXPORT OneHot {
        Attribute a;
        Encoding e;
        int n;
        int vmax;
        int v;

        // v=VALUE_NA_UNENCODED means NULL (valid case, e.g. no such stack in the battle)
        OneHot(Attribute a_)
        : a(a_),
          e(std::get<1>(HEX_ENCODING.at(EI(a_)))),
          n(std::get<2>(HEX_ENCODING.at(EI(a_)))),
          vmax(std::get<3>(HEX_ENCODING.at(EI(a_)))),
          v(VALUE_NA_UNENCODED) {};

        OneHot(Attribute a_, int v_)
        : a(a_),
          e(std::get<1>(HEX_ENCODING.at(EI(a_)))),
          n(std::get<2>(HEX_ENCODING.at(EI(a_)))),
          vmax(std::get<3>(HEX_ENCODING.at(EI(a_)))),
          v(v_) {
            if (v > vmax)
                throw std::runtime_error(
                    "Cannot encode value: " + std::to_string(v) \
                    + "(vmax=" + std::to_string(vmax) \
                    + ", a=" + std::to_string(EI(a)) \
                    + ", n=" + std::to_string(n) + ")"
                );
          };

        void encode(State &vec) const {
            if (v == VALUE_NA_UNENCODED) {
                if (e == Encoding::FLOATING)
                    vec.push_back(STATE_VALUE_NA);
                else
                    vec.insert(vec.end(), n, STATE_VALUE_NA);

                return;
            }

            switch (e) {
            break; case Encoding::NUMERIC_SQRT: encodeNumericSqrt(vec);
            break; case Encoding::NUMERIC: encodeNumeric(vec);
            break; case Encoding::BINARY: encodeBinary(vec);
            break; case Encoding::CATEGORICAL: encodeCategorical(vec);
            break; case Encoding::FLOATING: encodeFloating(vec);
            break; default:
                throw std::runtime_error("Unexpected Encoding: " + std::to_string(EI(e)));
            }
        }

        void validate(std::string ident) {

        }

        // Example: v=2, n=3
        //  Add v=2 ones and 3-2=1 zero
        void encodeNumeric(State &vec) const {
            int n_ones = v;
            vec.insert(vec.end(), n_ones, 1);
            vec.insert(vec.end(), n - n_ones, 0);
        }

        // Example: v=10, n=4
        //  Add int(sqrt(10))=3 ones and 4-3=1 zero
        //  => add [1,1,1,0]
        void encodeNumericSqrt(State &vec) const {
            int n_ones = int(std::sqrt(v));
            vec.insert(vec.end(), n_ones, 1);
            vec.insert(vec.end(), n - n_ones, 0);
        }

        // Example: v=5, n=4
        //  Represent 5 as a 4-bit binary (LSB first)
        //  => add [1,0,1,0]
        void encodeBinary(State &vec) const {
            int vtmp = v;
            for (int i=0; i < n; i++) {
                vec.push_back(vtmp % 2);
                vtmp /= 2;
            }
        }

        // Example: v=1, n=5
        //  => add [0,1,0,0,0]
        void encodeCategorical(State &vec) const {
            for (int i=0; i < n; i++)
                vec.push_back(i == v);
        }

        // Example: v=1, n=5
        //  => add 0.2
        void encodeFloating(State &vec) const {
            vec.push_back(encode2Floating());
        }

        // A separate function in cases where the caller needs to
        // explicitly encode as a float regardless of actual encoding
        // (in which case it makes no sense to accept a vector)
        float encode2Floating() const {
            // XXX: this is a simplified version for 0..1, vmin=0, vmax=n-1
            return static_cast<float>(v) / static_cast<float>(n-1);
        }
    };

    using StateUnencoded = std::vector<OneHot>;

    /**
     * 2 non-hex actions:
     * retreat, wait
     */
    constexpr int N_NONHEX_ACTIONS = 2;
    constexpr Action ACTION_RETREAT = 0;
    constexpr Action ACTION_WAIT = 1;

    /**
     * 8 hex actions:
     * move, move+att1, ... move+att7
     */
    constexpr int N_HEX_ACTIONS = 14;
    constexpr int N_ACTIONS = N_NONHEX_ACTIONS + 165 * N_HEX_ACTIONS;

    using ActMask = std::array<bool, N_ACTIONS>;

    // Control actions(not part of the regular action space)
    constexpr Action ACTION_UNSET = INT16_MIN;
    constexpr Action ACTION_RESET = -1;
    constexpr Action ACTION_RENDER_ANSI = -2;

    enum ResultType {REGULAR, ANSI_RENDER, UNSET};

    enum class Side : int {ATTACKER, DEFENDER};

    struct Result {
        Result() {};

        // Constructor 1: rendering
        Result(std::string ansiRender_, Side side_)
        : type(ResultType::ANSI_RENDER), ansiRender(ansiRender_), side(side_) {};

        // Constructor 1: regular result
        Result(
            StateUnencoded state_, ActMask actmask_, Side side_,
            int dmgDealt_, int dmgReceived_,
            int unitsLost_, int unitsKilled_,
            int valueLost_, int valueKilled_,
            int side0ArmyValue_, int side1ArmyValue_
        ) : type(ResultType::REGULAR),
            stateUnencoded(state_),
            actmask(actmask_),
            side(side_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_),
            side0ArmyValue(side0ArmyValue_),
            side1ArmyValue(side1ArmyValue_) {};

        // Constructor 2 (move constructor): regular result (battle ended)
        Result(Result &&other, bool victory_)
        : type(ResultType::REGULAR),
          stateUnencoded(other.stateUnencoded),
          actmask(other.actmask),
          side(other.side),
          dmgDealt(other.dmgDealt),
          dmgReceived(other.dmgReceived),
          unitsLost(other.unitsLost),
          unitsKilled(other.unitsKilled),
          valueLost(other.valueLost),
          valueKilled(other.valueKilled),
          side0ArmyValue(other.side0ArmyValue),
          side1ArmyValue(other.side1ArmyValue),
          ended(true),
          victory(victory_) {};


        const ResultType type = ResultType::UNSET;
        const StateUnencoded stateUnencoded = {};
        const ActMask actmask = {};
        const Side side = Side::ATTACKER;
        const int dmgDealt = 0;
        const int dmgReceived = 0;
        const int unitsLost = 0;
        const int unitsKilled = 0;
        const int valueLost = 0;
        const int valueKilled = 0;
        const int side0ArmyValue = 0;
        const int side1ArmyValue = 0;
        const std::string ansiRender = "";
        const bool ended = false;
        const bool victory = false;

        // This is dynamically set to avoid re-creating Result{}
        // on each getAction() if when actions as mostly errors
        // (ie. when nothing changes except the errors)
        ErrMask errmask = 0;

    };

    // F_Sys is a CPP function returned by pyclient's `init`
    // GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
    using F_Sys = std::function<void(std::string cmd)>;

    // An F_GetAction type function is called by BAI on each "activeStack()" call.
    // Such a function is usually be:
    // - libconnector's getAction (when VCMI is started as a Gym env)
    //   (VcmiEnv->PyConnector->Connector::initBaggage())
    // - a dummy getAction (when VCMI is started as a standalone program)
    using F_GetAction = std::function<Action(const Result * result)>;

    // The CB functions above are all bundled into Baggage struct
    // whose purpose is to be seamlessly transportable through VCMI code
    // as a std::any object, then cast back to Baggage in the AI constructor
    struct DLL_EXPORT Baggage {
        Baggage() = delete;
        Baggage(F_GetAction f)
        : f_getAction(f), f_getActionAttacker(f), f_getActionDefender(f) {}
        const F_GetAction f_getAction;

        // Set during vcmi_init(...) to "MMAI", "BattleAI" or "StupidAI"
        std::string attackerBattleAIName;
        std::string defenderBattleAIName;

        // Optionally set during vcmi_init(...) to a pre-trained model func
        F_GetAction f_getActionAttacker;
        F_GetAction f_getActionDefender;

        // Cmd-line option for evaluating maps
        // Contains a result counter (win=1, lose=-1) for each possible hero
        // XXX: this assumes heroes have `id` between 0 and 63
        std::map<int, int> battleResults;
        std::string map;
        int mapEval;
    };
}
