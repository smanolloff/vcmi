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
        /*
         * Represent `v` as `n` bits, where `bits[1..v+1]=1`.
         * If `v=-1` (a.k.a. "NULL"), only the bit at index 0 will be `1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,1,1,1,1]`
         * * `v=0`,  `n=5` => `[0,1,0,0,0]`
         * * `v=-1`, `n=5` => `[1,0,0,0,0]`
         */
        ACCUMULATING_EXPLICIT_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[0..v]=1`.
         * If `v=-1` (a.k.a. "NULL"), all bits will be `0`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,1,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[0,0,0,0,0]`
         */
        ACCUMULATING_IMPLICIT_NULL,
        /*
         * Represent `v` as `n` bits, where `bits[0..v]=1`.
         * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,1,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
         */
        ACCUMULATING_MASKING_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[0..v]=1`.
         * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,1,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => (error)
         */
        ACCUMULATING_STRICT_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[0..v]=1`.
         * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,1,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[1,0,0,0,0]`
         */
        ACCUMULATING_ZERO_NULL,

        /*
         * Represent `v<<1` as `n`-bit binary (unsigned, LSB at index 0).
         * If `v=-1` (a.k.a. "NULL"), only the bit at index 0 will be `1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,1,1,0,0]`
         * * `v=0`,  `n=5` => `[0,0,0,0,0]`
         * * `v=-1`, `n=5` => `[1,0,0,0,0]`
         */
        BINARY_EXPLICIT_NULL,

        /*
         * Represent `v` as an `n`-bit binary (LSB at index 0)
         * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,0,0,0]`
         * * `v=0`,  `n=5` => `[0,0,0,0,0]`
         * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
         */
        BINARY_MASKING_NULL,

        /*
         * Represent `v` as an `n`-bit binary (LSB at index 0)
         * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,0,0,0]`
         * * `v=0`,  `n=5` => `[0,0,0,0,0]`
         * * `v=-1`, `n=5` => (error)
         */
        BINARY_STRICT_NULL,

        /*
         * Represent `v` as `n`-bit binary (unsigned, LSB at index 0).
         * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[1,1,0,0,0]`
         * * `v=0`,  `n=5` => `[0,0,0,0,0]`
         * * `v=-1`, `n=5` => `[0,0,0,0,0]`
         */
        BINARY_ZERO_NULL,
        // XXX: BINARY_ZERO_NULL obsoletes BINARY_IMPLICIT_NULL

        /*
         * Represent `v` as `n` bits, where `bits[v+1]=1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,0,0,0,1]`
         * * `v=0`,  `n=5` => `[0,1,0,0,0]`
         * * `v=-1`, `n=5` => `[1,0,0,0,0]`
         */
        CATEGORICAL_EXPLICIT_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[v]=1`.
         * If `v=-1` (a.k.a. "NULL"), all bits will be `0`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,0,0,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[0,0,0,0,0]`
         */
        CATEGORICAL_IMPLICIT_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[v]=1`.
         * If `v=-1` (a.k.a. "NULL"), all bits will be `-1`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,0,0,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[-1,-1,-1,-1,-1]`
         */
        CATEGORICAL_MASKING_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[v]=1`.
         * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,0,0,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => (error)
         */
        CATEGORICAL_STRICT_NULL,

        /*
         * Represent `v` as `n` bits, where `bits[v]=1`.
         * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
         *
         * Examples:
         * * `v=3`,  `n=5` => `[0,0,0,1,0]`
         * * `v=0`,  `n=5` => `[1,0,0,0,0]`
         * * `v=-1`, `n=5` => `[1,0,0,0,0]`
         */
        CATEGORICAL_ZERO_NULL,

        /*
         * Represent `v` as `[0, vnorm(0, vmax)]`.
         * If `v=-1` (a.k.a. "NULL"), the result will be `[1, 0]
         *
         * Examples:
         * * `v=3`,  `vmax=10` => `[0, 0.3]`
         * * `v=0`,  `vmax=10` => `[0, 0]`
         * * `v=-1`, `vmax=10` => `[1, 0]`
         */
        NORMALIZED_EXPLICIT_NULL,

        /*
         * Normalize `v` in the range (0, vmax)
         * If `v=-1` (a.k.a. "NULL"), it will not be normalized.
         *
         * Examples:
         * * `v=3`,  `vmax=10` => `0.3`
         * * `v=0`,  `vmax=10` => `0`
         * * `v=-1`, `vmax=10` => `-1`
         */
        NORMALIZED_MASKING_NULL,

        /*
         * Normalize `v` in the range (0, vmax)
         * If `v=-1` (a.k.a. "NULL"), an error will be thrown.
         *
         * Examples:
         * * `v=3`,  `vmax=10` => `0.3`
         * * `v=0`,  `vmax=10` => `0`
         * * `v=-1`, `vmax=10` => (error)
         */
        NORMALIZED_STRICT_NULL,

        /*
         * Normalize `v` in the range (0, vmax)
         * If `v=-1` (a.k.a. "NULL"), it will be treated as `0`.
         *
         * Examples:
         * * `v=3`,  `vmax=10` => `0.6`
         * * `v=0`,  `vmax=10` => `0`
         * * `v=-1`, `vmax=10` => `0`
         */
        NORMALIZED_ZERO_NULL,
        // XXX: NORMALIZED_ZERO_NULL obsoletes NORMALIZED_IMPLICIT_NULL
    };

    enum class HexState : int {
        // IMPASSABLE,         // obstacle/stack/gate(closed,attacker)
        PASSABLE,       // empty/mine/firewall/gate(open)/gate(closed,defender), ...
        STOPPING,       // moat/quicksand
        DAMAGING_L,     // moat/mine/firewall
        DAMAGING_R,     // moat/mine/firewall
        // GATE,               // XXX: redundant? Always set during siege (regardless of gate state)
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

    enum class GlobalAttribute : int {
        PERCENT_CUR_TO_START_TOTAL_VALUE,
    };

    enum class StackAttribute : int {
        ID,
        Y_COORD,
        X_COORD,
        SIDE,
        QUANTITY,
        ATTACK,
        DEFENSE,
        SHOTS,
        DMG_MIN,
        DMG_MAX,
        HP,
        HP_LEFT,
        SPEED,
        WAITED,
        QUEUE_POS,
        RETALIATIONS_LEFT,
        IS_WIDE,
        AI_VALUE,
        MORALE,
        LUCK,
        FLYING,
        BLIND_LIKE_ATTACK,          // % chance for unicorns, medusas, basilisks, scorpicores
        ADDITIONAL_ATTACK,          /*val: number of additional attacks to perform*/
        NO_MELEE_PENALTY,
        TWO_HEX_ATTACK_BREATH,      /*eg. dragons*/
        NON_LIVING,                 // indicates immunity to mind spells; UNDEAD included here
        BLOCKS_RETALIATION,         /*eg. naga*/

        _count
    };

    // For description on each attribute, see the comments for HEX_ENCODING
    enum class HexAttribute : int {
        Y_COORD,
        X_COORD,
        STATE_MASK,
        ACTION_MASK,
        STACK_ID,
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

    class IStats {
    public:
        virtual int getInitialArmyValueLeft() const = 0;
        virtual int getInitialArmyValueRight() const = 0;
        virtual int getCurrentArmyValueLeft() const = 0;
        virtual int getCurrentArmyValueRight() const = 0;
        virtual ~IStats() = default;
    };

    using StackAttrs = std::array<int, static_cast<int>(StackAttribute::_count)>;
    using HexAttrs = std::array<int, static_cast<int>(HexAttribute::_count)>;

    class IStack {
    public:
        virtual const StackAttrs& getAttrs() const = 0;
        virtual int getAttr(StackAttribute) const = 0;
        virtual char getAlias() const = 0;
        virtual ~IStack() = default;
    };


    class IHex {
    public:
        virtual const HexAttrs& getAttrs() const = 0;
        virtual int getAttr(HexAttribute) const = 0;
        virtual ~IHex() = default;
    };

    class IAttackLog {
    public:
        virtual IStack* getAttacker() const = 0;
        virtual IStack* getDefender() const = 0;
        virtual int getDamageDealt() const = 0;
        virtual int getUnitsKilled() const = 0;
        virtual int getValueKilled() const = 0;
        virtual ~IAttackLog() = default;
    };

    using AttackLogs = std::vector<IAttackLog*>;
    using Hexes = std::array<std::array<IHex*, 15>, 11>;
    using Stacks = std::array<std::array<IStack*, 10>, 2>;

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
        virtual bool getIsBattleEnded() const = 0;
        virtual bool getIsVictorious() const = 0;
        virtual const IStats* getStats() const = 0;
        virtual const Hexes getHexes() const = 0;
        virtual const Stacks getStacks() const = 0;
        virtual const AttackLogs getAttackLogs() const = 0;
        virtual const std::string getAnsiRender() const = 0;
        virtual ~ISupplementaryData() = default;
    };
}
