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

namespace MMAI::Schema::V3 {
    enum class Encoding : int {
        NUMERIC,            // see Encoder::encodeNumeric
        NUMERIC_SQRT,       // see Encoder::encodeNumericSqrt
        BINARY,             // see Encoder::encodeBinary
        CATEGORICAL,        // see Encoder::encodeCategorical
        FLOATING            // see Encoder::encodeFloating
    };

    // Possible values for HEX_ATTACKABLE_BY_ENEMY_STACK_* attributes
    enum class DmgMod {
        ZERO,       // temp. state (e.g. can't reach, is blinded, etc.)
        HALF,       // blocked shooter (without NO_MELEE_PENALTY bonus)
        FULL,       // normal melee attack
        _count
    };

    // Possible values for HEX_SHOOT_DISTANCE_FROM_STACK_* attributes
    enum class ShootDistance {
        NA,         // can't shoot
        FAR,        // >10 hexes
        NEAR,       // <= 10 hexes (or has NO_RANGE_PENALTY bonus)
        _count
    };

    /*
     * Possible values for HEX_MELEE_DISTANCE_FROM_STACK_* attributes
     *
     * Diagram explanation:
     *      If an enemy stack covers hexes marked by "1" or "2",
     *      HEX_MELEE_DISTANCE_FROM_STACK_ for hex X would have
     *      the corresponding value (1 or 2)
     *
     *             FAR:        |    NEAR:
     *  . . . . . . . . . . .  |  . . . . . . . . .
     * . . . . . 1 . . 1 . . . | . . 2 2 . . . . .
     *  . . . X ~ 1 . 1 ~ X .  |  . 2 X 2 . . . . .
     * . . . . . 1 . . 1 . . . | . . 2 2 . . . . .
     *  . . . . . . . . . . .  |  . . . . . . . . .
     *
     * XXX: in case both are possible, NEAR is set, e.g.:
     *  . . . . . . . . . . .
     * . . . . . . 2 1 . . . .
     *  . . . . . X ~ . . . .
     * . . . . . . . . . . . .
     * => HEX_MELEE_DISTANCE_FROM_STACK_* for hex "X" will be "2" in this case.
     *
     * XXX: dragon breath is not taken into account, too hard to calculate.
     * . . . . . 3 . 3 . . . . (requires a friendly stack on the * hexes)
     *  . . . . . * * . . . .
     * . . . 3 * ~ X * 3 . . .
     *  . . . . . * * . . . .
     * . . . . . 3   3 . . . .
     * => HEX_MELEE_DISTANCE_FROM_STACK_* for hex "X" will be NA in this case.
     */

    enum class MeleeDistance {
        NA,
        FAR,
        NEAR,
        _count
    };

    enum class HexState : int {
        INVALID = -1,       // no hex
        FREE,
        STACK_FRONT,        // alive stack (front hex)
        STACK_BACK,         // alive stack (back hex)
        OBSTACLE,
        _count
    };

    enum class HexAction : int {
        AMOVE_TR,   // = Move to (*) + attack at hex 0..11:
        AMOVE_R,    //  . . . . . . . . . 5 0 . . . .
        AMOVE_BR,   // . 1-hex:  . . . . 4 * 1 . . .
        AMOVE_BL,   //  . . . . . . . . . 3 2 . . . .
        AMOVE_L,    // . . . . . . . . . . . . . . .
        AMOVE_TL,   //  . . . . . . . . . 5 0 6 . . .
        AMOVE_2TR,  // . 2-hex (R):  . . 4 * # 7 . .
        AMOVE_2R,   //  . . . . . . . . . 3 2 8 . . .
        AMOVE_2BR,  // . . . . . . . . . . . . . . .
        AMOVE_2BL,  //  . . . . . . . .11 5 0 . . . .
        AMOVE_2L,   // . 2-hex (L):  .10 # * 1 . . .
        AMOVE_2TL,  //  . . . . . . . . 9 3 2 . . . .
        MOVE,       // = Move to (defend if current hex)
        SHOOT,      // = shoot at
        _count
    };

    // For description on each attribute, see the comments for HEX_ENCODING
    enum class HexAttribute : int {
        PERCENT_CUR_TO_START_TOTAL_VALUE,
        HEX_Y_COORD,
        HEX_X_COORD,
        HEX_STATE,
        HEX_ACTION_MASK_FOR_ACT_STACK,
        HEX_ACTION_MASK_FOR_L_STACK_0,
        HEX_ACTION_MASK_FOR_L_STACK_1,
        HEX_ACTION_MASK_FOR_L_STACK_2,
        HEX_ACTION_MASK_FOR_L_STACK_3,
        HEX_ACTION_MASK_FOR_L_STACK_4,
        HEX_ACTION_MASK_FOR_L_STACK_5,
        HEX_ACTION_MASK_FOR_L_STACK_6,
        HEX_ACTION_MASK_FOR_R_STACK_0,
        HEX_ACTION_MASK_FOR_R_STACK_1,
        HEX_ACTION_MASK_FOR_R_STACK_2,
        HEX_ACTION_MASK_FOR_R_STACK_3,
        HEX_ACTION_MASK_FOR_R_STACK_4,
        HEX_ACTION_MASK_FOR_R_STACK_5,
        HEX_ACTION_MASK_FOR_R_STACK_6,
        HEX_MELEEABLE_BY_ACT_STACK,
        HEX_MELEEABLE_BY_L_STACK_0,
        HEX_MELEEABLE_BY_L_STACK_1,
        HEX_MELEEABLE_BY_L_STACK_2,
        HEX_MELEEABLE_BY_L_STACK_3,
        HEX_MELEEABLE_BY_L_STACK_4,
        HEX_MELEEABLE_BY_L_STACK_5,
        HEX_MELEEABLE_BY_L_STACK_6,
        HEX_MELEEABLE_BY_R_STACK_0,
        HEX_MELEEABLE_BY_R_STACK_1,
        HEX_MELEEABLE_BY_R_STACK_2,
        HEX_MELEEABLE_BY_R_STACK_3,
        HEX_MELEEABLE_BY_R_STACK_4,
        HEX_MELEEABLE_BY_R_STACK_5,
        HEX_MELEEABLE_BY_R_STACK_6,
        HEX_SHOOT_DISTANCE_FROM_ACT_STACK,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_0,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_1,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_2,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_3,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_4,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_5,
        HEX_SHOOT_DISTANCE_FROM_L_STACK_6,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_0,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_1,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_2,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_3,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_4,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_5,
        HEX_SHOOT_DISTANCE_FROM_R_STACK_6,
        HEX_MELEE_DISTANCE_FROM_ACT_STACK,
        HEX_MELEE_DISTANCE_FROM_L_STACK_0,
        HEX_MELEE_DISTANCE_FROM_L_STACK_1,
        HEX_MELEE_DISTANCE_FROM_L_STACK_2,
        HEX_MELEE_DISTANCE_FROM_L_STACK_3,
        HEX_MELEE_DISTANCE_FROM_L_STACK_4,
        HEX_MELEE_DISTANCE_FROM_L_STACK_5,
        HEX_MELEE_DISTANCE_FROM_L_STACK_6,
        HEX_MELEE_DISTANCE_FROM_R_STACK_0,
        HEX_MELEE_DISTANCE_FROM_R_STACK_1,
        HEX_MELEE_DISTANCE_FROM_R_STACK_2,
        HEX_MELEE_DISTANCE_FROM_R_STACK_3,
        HEX_MELEE_DISTANCE_FROM_R_STACK_4,
        HEX_MELEE_DISTANCE_FROM_R_STACK_5,
        HEX_MELEE_DISTANCE_FROM_R_STACK_6,
        STACK_QUANTITY,
        STACK_ATTACK,
        STACK_DEFENSE,
        STACK_SHOTS,
        STACK_DMG_MIN,
        STACK_DMG_MAX,
        STACK_HP,
        STACK_HP_LEFT,
        STACK_SPEED,
        STACK_WAITED,
        STACK_QUEUE_POS,
        STACK_RETALIATIONS_LEFT,
        STACK_SIDE,
        STACK_SLOT,
        STACK_CREATURE_TYPE,
        STACK_AI_VALUE_TENTH,
        STACK_IS_ACTIVE,
        STACK_IS_WIDE,
        STACK_FLYING,
        STACK_NO_MELEE_PENALTY,
        STACK_TWO_HEX_ATTACK_BREATH,
        STACK_BLOCKS_RETALIATION,
        STACK_DEFENSIVE_STANCE,
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

    enum class ErrorCode : int {
        OK,
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

    using HexAttrs = std::array<int, static_cast<int>(HexAttribute::_count)>;

    class IHex {
    public:
        virtual const HexAttrs& getAttrs() const = 0;
        virtual int getAttr(HexAttribute) const = 0;
        virtual ~IHex() = default;
    };

    using Hexes = std::array<std::array<IHex*, 15>, 11>;

    class IAttackLog {
    public:
        virtual int getAttackerSlot() const = 0;
        virtual int getDefenderSlot() const = 0;
        virtual int getDefenderSide() const = 0;
        virtual int getDamageDealt() const = 0;
        virtual int getUnitsKilled() const = 0;
        virtual int getValueKilled() const = 0;
        virtual ~IAttackLog() = default;
    };

    using AttackLogs = std::vector<IAttackLog*>;

    enum class Side : int {LEFT, RIGHT}; // corresponds to BattleSide::Type

    // This is returned as std::any by IState
    // => MMAI_LINKAGE is needed to ensure std::any_cast sees the same symbol
    class MMAI_LINKAGE ISupplementaryData {
    public:
        enum class Type : int {REGULAR, ANSI_RENDER};

        virtual Type getType() const = 0;
        virtual Side getSide() const = 0;
        virtual std::string getColor() const = 0;
        virtual ErrorCode getErrorCode() const = 0;
        virtual int getDmgDealt() const = 0;
        virtual int getDmgReceived() const = 0;
        virtual int getUnitsLost() const = 0;
        virtual int getUnitsKilled() const = 0;
        virtual int getValueLost() const = 0;
        virtual int getValueKilled() const = 0;
        virtual int getSide0ArmyValue() const = 0;
        virtual int getSide1ArmyValue() const = 0;
        virtual bool getIsBattleEnded() const = 0;
        virtual bool getIsVictorious() const = 0;
        virtual const Hexes getHexes() const = 0;
        virtual const AttackLogs getAttackLogs() const = 0;
        virtual const std::string getAnsiRender() const = 0;
        virtual ~ISupplementaryData() = default;
    };

    /*
     * E4 is a tuple of {a, e, n, vmax} tuples
     * where a=attribute, e=encoding, n=size, vmax=max expected value
     */
    using E4 = std::tuple<HexAttribute, Encoding, int, int>;
    using HexEncoding = std::array<E4, EI(HexAttribute::_count)>;
}
