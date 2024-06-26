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

#include "../base.h"
#include "types.h"
#include "util.h"

namespace MMAI::Schema::V3 {
    constexpr int N_NONHEX_ACTIONS = 2;
    constexpr Action ACTION_RETREAT = 0;
    constexpr Action ACTION_WAIT = 1;
    constexpr int N_HEX_ACTIONS = EI(HexAction::_count);
    constexpr int N_ACTIONS = N_NONHEX_ACTIONS + 165 * N_HEX_ACTIONS;

    // Control actions (not part of the regular action space)
    constexpr Action ACTION_UNSET = -666;
    constexpr Action ACTION_RESET = -1;
    constexpr Action ACTION_RENDER_ANSI = -2;

    // Value used when masking NULL values during encoding
    constexpr int NULL_VALUE_ENCODED = -1;
    constexpr int NULL_VALUE_UNENCODED = -1;

    // Convenience definitions which do not need to be exported
    namespace {
        inline constexpr auto AE = Encoding::ACCUMULATING_EXPLICIT_NULL;
        inline constexpr auto AI = Encoding::ACCUMULATING_IMPLICIT_NULL;
        inline constexpr auto AM = Encoding::ACCUMULATING_MASKING_NULL;
        inline constexpr auto AS = Encoding::ACCUMULATING_STRICT_NULL;
        inline constexpr auto AZ = Encoding::ACCUMULATING_ZERO_NULL;

        inline constexpr auto BE = Encoding::BINARY_EXPLICIT_NULL;
        inline constexpr auto BM = Encoding::BINARY_MASKING_NULL;
        inline constexpr auto BS = Encoding::BINARY_STRICT_NULL;
        inline constexpr auto BZ = Encoding::BINARY_ZERO_NULL;

        inline constexpr auto CE = Encoding::CATEGORICAL_EXPLICIT_NULL;
        inline constexpr auto CI = Encoding::CATEGORICAL_IMPLICIT_NULL;
        inline constexpr auto CM = Encoding::CATEGORICAL_MASKING_NULL;
        inline constexpr auto CS = Encoding::CATEGORICAL_STRICT_NULL;
        inline constexpr auto CZ = Encoding::CATEGORICAL_ZERO_NULL;

        inline constexpr auto NE = Encoding::NORMALIZED_EXPLICIT_NULL;
        inline constexpr auto NM = Encoding::NORMALIZED_MASKING_NULL;
        inline constexpr auto NS = Encoding::NORMALIZED_STRICT_NULL;
        inline constexpr auto NZ = Encoding::NORMALIZED_ZERO_NULL;

        using HA = HexAttribute;
        using SA = StackAttribute;

        /*
         * The encoding schema `{a, e, n, vmax}`, where:
         * `a`=attribute, `e`=encoding, `n`=size, `vmax`=max_value.
         */
        using E4H = std::tuple<HexAttribute, Encoding, int, int>;
        using E4S = std::tuple<StackAttribute, Encoding, int, int>;
    }

    using HexEncoding = std::array<E4H, EI(HexAttribute::_count)>;
    using StackEncoding = std::array<E4S, EI(StackAttribute::_count)>;

    /*
     * Compile-time constructor for E4H and E4S tuples
     * https://stackoverflow.com/a/23784921
     */
    template<typename T>
    constexpr std::tuple<T, Encoding, int, int> E4(T a, Encoding e, int vmax) {
        switch(e) {
        // "0" is a value => vmax+1 values
        break; case AE: return {a, e, vmax+2, vmax};
        break; case AI: return {a, e, vmax+1, vmax};
        break; case AM: return {a, e, vmax+1, vmax};
        break; case AS: return {a, e, vmax+1, vmax};
        break; case AZ: return {a, e, vmax+1, vmax};

        // Log2(8)=3 (2^3), but if vmax=8 then 4 bits will be required
        // => Log2(9)=4
        break; case BE: return {a, e, static_cast<int>(Log2(vmax+1))+1, vmax};
        break; case BM: return {a, e, static_cast<int>(Log2(vmax+1)), vmax};
        break; case BS: return {a, e, static_cast<int>(Log2(vmax+1)), vmax};
        break; case BZ: return {a, e, static_cast<int>(Log2(vmax+1)), vmax};

        // "0" is a category => vmax+1 categories
        break; case CE: return {a, e, vmax+2, vmax};
        break; case CI: return {a, e, vmax+1, vmax};
        break; case CM: return {a, e, vmax+1, vmax};
        break; case CS: return {a, e, vmax+1, vmax};
        break; case CZ: return {a, e, vmax+1, vmax};

        break; case NE: return {a, e, 2, vmax};
        break; case NM: return {a, e, 1, vmax};
        break; case NS: return {a, e, 1, vmax};
        break; case NZ: return {a, e, 1, vmax};
        break; default:
            throw std::runtime_error("Unexpected encoding: " + std::to_string(EI(e)));
        }
    }

    constexpr int MAX_STACKS_PER_SIDE = std::tuple_size<Stacks::value_type>::value;
    constexpr int MAX_STACKS = 2 * MAX_STACKS_PER_SIDE;

    constexpr HexEncoding HEX_ENCODING {
        E4(HA::Y_COORD,     CS, 10),
        E4(HA::X_COORD,     CS, 14),
        E4(HA::STATE,       CS, EI(HexState::_count)-1),
        E4(HA::ACTION_MASK, BZ, (1<<N_HEX_ACTIONS)-1), // null on battle end
        E4(HA::STACK_ID,    CE, MAX_STACKS-1),
    };

    constexpr StackEncoding STACK_ENCODING {
        E4(SA::ID,                        CE, MAX_STACKS-1),
        E4(SA::Y_COORD,                   CE, 10),
        E4(SA::X_COORD,                   CE, 14),
        E4(SA::SIDE,                      CE, 1),        // 0=attacker, 1=defender
        E4(SA::QUANTITY,                  NE, 2000),
        E4(SA::ATTACK,                    NE, 80),
        E4(SA::DEFENSE,                   NE, 80),       // azure dragon is 60 when defending
        E4(SA::SHOTS,                     NE, 32),       // sharpshooter is 32; 0 if battleCanShoot() is false
        E4(SA::DMG_MIN,                   NE, 100),      // azure dragon is 80
        E4(SA::DMG_MAX,                   NE, 100),      // azure dragon is 80
        E4(SA::HP,                        NE, 1300),     // azure dragon + all artifacts is 1254
        E4(SA::HP_LEFT,                   NE, 1300),
        E4(SA::SPEED,                     NE, 30),       // at 19=full reach; max is... 37?
        E4(SA::WAITED,                    NE, 1),
        E4(SA::QUEUE_POS,                 NE, 15),       // 0..14, 0=active stack
        E4(SA::RETALIATIONS_LEFT,         NE, 2),        // inf is truncated to 2 (royal griffin)
        E4(SA::MAGIC_RESISTANCE,          NE, 100),      //
        E4(SA::IS_WIDE,                   NE, 1),
        E4(SA::AI_VALUE,                  NE, 40000),    // azure dragon is 78845, but is damped to 40K (using tanh())
        E4(SA::MORALE,                    NE, 7),        // -3..+3
        E4(SA::LUCK,                      NE, 7),        // -3..+3

        // Spells after attack (chance to cast in %)
        E4(SA::BLIND_LIKE_ATTACK,         NE, 100),      // unicorns (20), medusas (20), basilisks (20), scorpicores (20)
        E4(SA::WEAKENING_ATTACK,          NE, 100),      // dragon flies (100), zombies (20)
        E4(SA::DISPELLING_ATTACK,         NE, 100),      // serpent flies (100)
        E4(SA::POISONOUS_ATTACK,          NE, 100),      // wyverns (50)
        E4(SA::CURSING_ATTACK,            NE, 100),      // mummies (50), black knights (20)
        E4(SA::AGING_ATTACK,              NE, 100),      // ghost dragon (20)
        E4(SA::ACID_ATTACK,               NE, 100),      // rust dragon (100)
        E4(SA::BINDING_ATTACK,            NE, 100),      // dendroids (100)
        E4(SA::LIGHTNING_ATTACK,          NE, 100),      // thunderbirds (20)

        E4(SA::AREA_ATTACK,               NE, 2),       // thunderbirds (20)

        // Hate (dmg bonus in %)
        E4(SA::HATES_ANGELS,              NE, 100),
        E4(SA::HATES_DEVILS,              NE, 100),
        E4(SA::HATES_TITANS,              NE, 100),
        E4(SA::HATES_BLACK_DRAGONS,       NE, 100),
        E4(SA::HATES_GENIES,              NE, 100),
        E4(SA::HATES_EFREET,              NE, 100),
        E4(SA::HATES_AIR_ELEMENTALS,      NE, 100),
        E4(SA::HATES_EARTH_ELEMENTALS,    NE, 100),
        E4(SA::HATES_WATER_ELEMENTALS,    NE, 100),
        E4(SA::HATES_FIRE_ELEMENTALS,     NE, 100),

        // Passives
        E4(SA::FREE_SHOOTING,             NE, 1),
        E4(SA::FLYING,                    NE, 1),
        E4(SA::SHOOTER,                   NE, 1),
        E4(SA::ADDITIONAL_ATTACK,         NE, 1),
        E4(SA::NO_MELEE_PENALTY,          NE, 1),
        E4(SA::JOUSTING,                  NE, 1),
        E4(SA::SPELL_RESISTANCE_AURA,     NE, 100),      // unicorns, value - resistance bonus in % for adjacent creatures*/
        E4(SA::LEVEL_SPELL_IMMUNITY,      NE, 5),        // levels 1-5
        E4(SA::FIRE_DAMAGE_REDUCTION,     NE, 100),      // 100% = immune
        E4(SA::WATER_DAMAGE_REDUCTION,    NE, 100),
        E4(SA::AIR_DAMAGE_REDUCTION,      NE, 100),
        E4(SA::EARTH_DAMAGE_REDUCTION,    NE, 100),
        E4(SA::TWO_HEX_ATTACK_BREATH,     NE, 1),
        E4(SA::NO_WALL_PENALTY,           NE, 1),
        E4(SA::NON_LIVING,                NE, 1),
        E4(SA::BLOCKS_RETALIATION,        NE, 1),
        E4(SA::THREE_HEADED_ATTACK,       NE, 1),
        E4(SA::MIND_IMMUNITY,             NE, 1),
        E4(SA::FIRE_SHIELD,               NE, 100),
        E4(SA::LIFE_DRAIN,                NE, 100),
        E4(SA::DOUBLE_DAMAGE_CHANCE,      NE, 100),
        E4(SA::RETURN_AFTER_STRIKE,       NE, 1),
        E4(SA::DEFENSIVE_STANCE,          NE, 1),
        E4(SA::ATTACKS_ALL_ADJACENT,      NE, 1),
        E4(SA::NO_DISTANCE_PENALTY,       NE, 1),
        E4(SA::HYPNOTIZED,                NE, 5),  // turns left
        E4(SA::MAGIC_MIRROR,              NE, 100),
        E4(SA::ATTACKS_NEAREST_CREATURE,  NE, 1),
        E4(SA::SLEEPING,                  NE, 5),  // turns left
        E4(SA::DEATH_STARE,               NE, 1),
        E4(SA::POISON,                    NE, 1),
        E4(SA::REBIRTH,                   NE, 1),
        E4(SA::ENEMY_DEFENCE_REDUCTION,   NE, 100),
        E4(SA::MELEE_DAMAGE_REDUCTION,    NE, 100),
        E4(SA::RANGED_DAMAGE_REDUCTION,   NE, 100),
        E4(SA::MELEE_ATTACK_REDUCTION,    NE, 100),
        E4(SA::RANGED_ATTACK_REDUCTION,   NE, 100),
    };

    // Dedining encodings for each attribute by hand is error-prone
    // The below compile-time asserts are essential.
    static_assert(UninitializedEncodingAttributes(HEX_ENCODING) == 0, "Found uninitialized elements");
    static_assert(UninitializedEncodingAttributes(STACK_ENCODING) == 0, "Found uninitialized elements");
    static_assert(DisarrayedEncodingAttributeIndex(HEX_ENCODING) == -1, "Found wrong element at this index");
    static_assert(DisarrayedEncodingAttributeIndex(STACK_ENCODING) == -1, "Found wrong element at this index");
    static_assert(MiscalculatedBinaryAttributeIndex(HEX_ENCODING) == -1, "Found miscalculated binary vmax element at this index");
    static_assert(MiscalculatedBinaryAttributeIndex(STACK_ENCODING) == -1, "Found miscalculated binary vmax element at this index");
    static_assert(MiscalculatedBinaryAttributeUnusedValues(HEX_ENCODING) == 0, "Number of unused values in the binary attribute is not 0");
    static_assert(MiscalculatedBinaryAttributeUnusedValues(STACK_ENCODING) == 0, "Number of unused values in the binary attribute is not 0");

    constexpr int BATTLEFIELD_STATE_SIZE_ONE_HEX = StateSizeOneElement(HEX_ENCODING);
    constexpr int BATTLEFIELD_STATE_SIZE_ALL_HEXES = 165 * BATTLEFIELD_STATE_SIZE_ONE_HEX;
    constexpr int BATTLEFIELD_STATE_SIZE_ONE_STACK = StateSizeOneElement(STACK_ENCODING);
    constexpr int BATTLEFIELD_STATE_SIZE_ALL_STACKS = MAX_STACKS * BATTLEFIELD_STATE_SIZE_ONE_STACK;
    constexpr int BATTLEFIELD_STATE_SIZE = BATTLEFIELD_STATE_SIZE_ALL_HEXES + BATTLEFIELD_STATE_SIZE_ALL_STACKS;
}
