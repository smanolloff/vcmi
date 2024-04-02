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
        ZERO,  // temp. state (e.g. can't reach, is blinded, etc.)
        HALF,
        FULL,
        _count
    };

    enum class Encoding : int {
        NUMERIC,            // see encodeNumeric
        NUMERIC_SQRT,       // see encodeNumericSqrt
        BINARY,             // see encodeBinary
        CATEGORICAL         // see encodeCategorical
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
        HEX_MELEEABLE_BY_ENEMY_STACK_0,         // can enemy stack0 melee attack hex? (0=no, 1=half, 2=full dmg)
        HEX_MELEEABLE_BY_ENEMY_STACK_1,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_2,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_3,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_4,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_5,         //
        HEX_MELEEABLE_BY_ENEMY_STACK_6,         //
        HEX_SHOOTABLE_BY_ACTIVE_STACK,          // can active stack shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_SHOOTABLE_BY_ENEMY_STACK_0,         // can enemy stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        HEX_SHOOTABLE_BY_ENEMY_STACK_1,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_2,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_3,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_4,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_5,         //
        HEX_SHOOTABLE_BY_ENEMY_STACK_6,         //
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

    using A = Attribute;
    using E = Encoding;

    // compile-time container of {a, n, e} (see OneHot)
    constexpr int HEX_ENCODING[] = {
        EI(A::HEX_Y_COORD),                       EI(E::CATEGORICAL),  11,
        EI(A::HEX_X_COORD),                       EI(E::CATEGORICAL),  15,
        EI(A::HEX_STATE),                         EI(E::CATEGORICAL),  3,      // see hexstate.h
        EI(A::HEX_REACHABLE_BY_ACTIVE_STACK),     EI(E::CATEGORICAL),  2,      // can active stack reach it?
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_0), EI(E::CATEGORICAL),  2,      // can friendly stack0 reach it?
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_1), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_2), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_3), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_4), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_5), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_FRIENDLY_STACK_6), EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_0),    EI(E::CATEGORICAL),  2,      // can enemy stack0 reach it?
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_1),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_2),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_3),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_4),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_5),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_REACHABLE_BY_ENEMY_STACK_6),    EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_MELEEABLE_BY_ACTIVE_STACK),     EI(E::NUMERIC),      3,      // can active stack melee at this hex? (0=no, 1=half, 2=full dmg)
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_0),    EI(E::NUMERIC),      3,      // can enemy stack0 melee attack hex? (0=no, 1=half, 2=full dmg)
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_1),    EI(E::NUMERIC),      3,      // XXX: MELEEABLE hex does mean there's a stack there (could even be an obstacle)
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_2),    EI(E::NUMERIC),      3,      // XXX: MELEEABLE hex does mean there's a stack there (could even be an obstacle)
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_3),    EI(E::NUMERIC),      3,      //      It's all about whether the stack can reach a NEARBY hex
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_4),    EI(E::NUMERIC),      3,      //      Should it be false for obstacles?
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_5),    EI(E::NUMERIC),      3,      //
        EI(A::HEX_MELEEABLE_BY_ENEMY_STACK_6),    EI(E::NUMERIC),      3,      //
        EI(A::HEX_SHOOTABLE_BY_ACTIVE_STACK),     EI(E::NUMERIC),      3,      // can active stack shoot at this hex? (0=no, 1=half, 2=full dmg)
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_0),    EI(E::NUMERIC),      3,      // can enemy stack0 shoot at this hex? (0=no, 1=half, 2=full dmg)
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_1),    EI(E::NUMERIC),      3,      // XXX: SHOOTABLE hex does mean there's a stack there (could even be an obstacle)
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_2),    EI(E::NUMERIC),      3,      // XXX: SHOOTABLE hex does mean there's a stack there (could even be an obstacle)
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_3),    EI(E::NUMERIC),      3,      //      It's all about the distance between the shooter and this
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_4),    EI(E::NUMERIC),      3,      //      Should it be false for obstacles?
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_5),    EI(E::NUMERIC),      3,      //
        EI(A::HEX_SHOOTABLE_BY_ENEMY_STACK_6),    EI(E::NUMERIC),      3,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_0),      EI(E::CATEGORICAL),  2,      // is friendly stack0 on a nearby hex?
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_1),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_2),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_3),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_4),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_5),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_FRIENDLY_STACK_6),      EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_0),         EI(E::CATEGORICAL),  2,      // is enemy stack0 on a nearby hex?
        EI(A::HEX_NEXT_TO_ENEMY_STACK_1),         EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_2),         EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_3),         EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_4),         EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_5),         EI(E::CATEGORICAL),  2,      //
        EI(A::HEX_NEXT_TO_ENEMY_STACK_6),         EI(E::CATEGORICAL),  2,      //
        EI(A::STACK_QUANTITY),                    EI(E::NUMERIC_SQRT), 32,     // sqrt(1024)
        EI(A::STACK_ATTACK),                      EI(E::NUMERIC_SQRT), 8,      // sqrt(64)
        EI(A::STACK_DEFENSE),                     EI(E::NUMERIC_SQRT), 8,      // sqrt(64)
        EI(A::STACK_SHOTS),                       EI(E::NUMERIC_SQRT), 5,      // sqrt(25)
        EI(A::STACK_DMG_MIN),                     EI(E::NUMERIC_SQRT), 8,      // sqrt(64)
        EI(A::STACK_DMG_MAX),                     EI(E::NUMERIC_SQRT), 8,      // sqrt(64)
        EI(A::STACK_HP),                          EI(E::NUMERIC_SQRT), 32,     // sqrt(1024)
        EI(A::STACK_HP_LEFT),                     EI(E::NUMERIC_SQRT), 32,     // sqrt(1024)
        EI(A::STACK_SPEED),                       EI(E::NUMERIC),      20,     //
        EI(A::STACK_WAITED),                      EI(E::CATEGORICAL),  2,      //
        EI(A::STACK_QUEUE_POS),                   EI(E::NUMERIC),      14,     // 0=active stack
        EI(A::STACK_RETALIATIONS_LEFT),           EI(E::NUMERIC),      3,      //
        EI(A::STACK_SIDE),                        EI(E::CATEGORICAL),  2,      // 0=attacker, 1=defender
        EI(A::STACK_SLOT),                        EI(E::CATEGORICAL),  6,      // 0..6
        EI(A::STACK_CREATURE_TYPE),               EI(E::CATEGORICAL),  150,    // 0..144
        EI(A::STACK_AI_VALUE_TENTH),              EI(E::NUMERIC_SQRT), 63,     // sqrt(3969), real value is x10
        EI(A::STACK_IS_ACTIVE),                   EI(E::CATEGORICAL),  2,      //
        EI(A::STACK_FLYING),                      EI(E::CATEGORICAL),  2,      //
        EI(A::STACK_NO_MELEE_PENALTY),            EI(E::CATEGORICAL),  2,      // TODO
        EI(A::STACK_TWO_HEX_ATTACK_BREATH),       EI(E::CATEGORICAL),  2,      // TODO
        EI(A::STACK_BLOCKS_RETALIATION),          EI(E::CATEGORICAL),  2,      // TODO
        EI(A::STACK_DEFENSIVE_STANCE),            EI(E::CATEGORICAL),  2       // TODO? means temp def bonus
    };

    constexpr int STATE_SIZE_ONE_HEX = 540; // sum of every 3rd elem in HEX_ENCODING

    /*
     * Begin of compile-time magic
     */

    // STATE_SIZE_ONE_HEX value is hard-coded for readability
    // However, we must ensure it is consistent with HEX_ENCODING
    // Static asserts here serve as a whistleblower for breaking changes

    // 1. Get the length of a constexpr array and check if
    //      the number of HEX_ENCODING triplets match the number of Attributes.
    //      If this fails => one of the lists has missing or extra attributes.
    template<std::size_t N>
    constexpr std::size_t arrayLength(const int (&)[N]) { return N; }
    static_assert(arrayLength(HEX_ENCODING) == EI(Attribute::_count) * 3);

    // 2. Sum every 3rd element of HEX_ENCODING and check if STATE_SIZE_ONE_HEX matches.
    //      If this fails => STATE_SIZE_ONE_HEX must be updated to match.
    constexpr int stateSizeEncoded() {
        int ret = 0;
        for (int i = 0; i < EI(Attribute::_count); i++)
            ret += HEX_ENCODING[i*3 + 2];
        return ret;
    }
    static_assert(STATE_SIZE_ONE_HEX == stateSizeEncoded());

    /*
     * End of compile-time magic
     */

    constexpr int STATE_SIZE = 165 * STATE_SIZE_ONE_HEX;

    constexpr int N_UNSET = -1;
    constexpr int VALUE_NA = -1;
    constexpr int VALUE_OH_NA = -1e9;

    // Arbitary int value that can be one-hot encoded
    extern "C" struct DLL_EXPORT OneHot {
        Attribute a;
        Encoding e;
        int n;
        int v;

        // v=VALUE_NA means NULL (valid case, e.g. no such stack in the battle)
        OneHot(Attribute a_)
        : a(a_),
          e(Encoding(HEX_ENCODING[EI(a)+1])),
          n(HEX_ENCODING[EI(a)+2]),
          v(VALUE_NA) {};

        OneHot(Attribute a_, int v_)
        : a(a_),
          e(Encoding(HEX_ENCODING[EI(a)+1])),
          n(HEX_ENCODING[EI(a)+2]),
          v(v_) {};

        void encode(std::vector<int> vec) {
            if (n == N_UNSET)
                throw std::runtime_error("Trying to encode an uninitialized OneHot");

            if (v == VALUE_NA) {
                vec.insert(vec.end(), n, VALUE_OH_NA);
                return;
            }

            switch (e) {
            case Encoding::NUMERIC_SQRT: encodeNumericSqrt(vec); break;
            case Encoding::NUMERIC: encodeNumeric(vec); break;
            case Encoding::BINARY: encodeBinary(vec); break;
            case Encoding::CATEGORICAL: encodeCategorical(vec); break;
            default:
                throw std::runtime_error("Unexpected Encoding: " + std::to_string(EI(e)));
            }
        }

        // Example: v=2, n=3
        //  Add v=2 ones and 3-2=1 zero
        void encodeNumeric(std::vector<int> vec) {
            int n_ones = v;

            if (n_ones >= n)
                throw std::runtime_error("Categorical encoding failed: v=" + std::to_string(v) + ", n=" + std::to_string(n));

            // Throwing error instead
            // n_ones = std::min(n_ones, n);

            vec.insert(vec.end(), n_ones, 1);
            vec.insert(vec.end(), n_ones - v, 0);
        }

        // Example: v=10, n=4
        //  Add int(sqrt(10))=3 ones and 4-3=1 zero
        //  => add [1,1,1,0]
        void encodeNumericSqrt(std::vector<int> vec) {
            int n_ones = int(sqrt(v));

            if (n_ones > n)
                throw std::runtime_error("ExpNumeric encoding failed: v=" + std::to_string(v) + ", n=" + std::to_string(n));

            // Throwing error instead
            // n_ones = std::min(n_ones, n);

            vec.insert(vec.end(), n_ones, 1);
            vec.insert(vec.end(), n - n_ones, 0);
        }

        // Example: v=5, n=4
        //  Represent 5 as a 4-bit binary (LSB first)
        //  => add [1,0,1,0]
        void encodeBinary(std::vector<int> vec) {
            int vtmp = v;
            for (int i=0; i < n; i++) {
                vec.push_back(vtmp % 2);
                vtmp /= 2;
            }

            if (vtmp > 0)
                throw std::runtime_error("Binary encoding failed: v=" + std::to_string(v) + ", n=" + std::to_string(n));
        }

        // Example: v=1, n=5
        //  => add [0,1,0,0,0]
        void encodeCategorical(std::vector<int> vec) {
            if (v >= n)
                throw std::runtime_error("Categorical encoding failed: v=" + std::to_string(v) + ", n=" + std::to_string(n));

            for (int i=0; i < n; i++)
                vec.push_back(i == v);
        }
    };

    using EncodedState = std::array<int, STATE_SIZE>;
    using State = std::vector<OneHot>;
    using Action = int16_t;

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
        Result(State state_, ActMask actmask_, Side side_, int dmgDealt_, int dmgReceived_,
            int unitsLost_, int unitsKilled_, int valueLost_, int valueKilled_,
            int side0ArmyValue_, int side1ArmyValue_
        ) : type(ResultType::REGULAR),
            state(state_),
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
          state(other.state),
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
        const State state = {};
        const EncodedState encodedState = {};
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
        bool printBattleResults;
    };
}
