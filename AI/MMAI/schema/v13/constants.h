/*
 * constants.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <tuple>

#include "schema/base.h"
#include "schema/v13/types.h"
#include "schema/v13/util.h"

namespace MMAI::Schema::V13
{
constexpr int N_NONHEX_ACTIONS = 2;
constexpr Action ACTION_RETREAT = 0;
constexpr Action ACTION_WAIT = 1;
constexpr int N_HEX_ACTIONS = EI(HexAction::_count);
constexpr int N_ACTIONS = N_NONHEX_ACTIONS + 165 * N_HEX_ACTIONS;
constexpr int STACK_ATTR_OFFSET = EI(HexAttribute::_count) - EI(StackAttribute::_count);

// Control actions (not part of the regular action space)
constexpr Action ACTION_UNSET = -666;
constexpr Action ACTION_RESET = -1;
constexpr Action ACTION_RENDER_ANSI = -2;

// Value used when masking NULL values during encoding
constexpr int NULL_VALUE_ENCODED = -1;
constexpr int NULL_VALUE_UNENCODED = -1;

// Convenience definitions which do not need to be exported
namespace
{
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

	inline constexpr auto EE = Encoding::EXPNORM_EXPLICIT_NULL;
	inline constexpr auto EM = Encoding::EXPNORM_MASKING_NULL;
	inline constexpr auto ES = Encoding::EXPNORM_STRICT_NULL;
	inline constexpr auto EZ = Encoding::EXPNORM_ZERO_NULL;

	inline constexpr auto LE = Encoding::LINNORM_EXPLICIT_NULL;
	inline constexpr auto LM = Encoding::LINNORM_MASKING_NULL;
	inline constexpr auto LS = Encoding::LINNORM_STRICT_NULL;
	inline constexpr auto LZ = Encoding::LINNORM_ZERO_NULL;

	inline constexpr auto RAW = Encoding::RAW;

	using GA = GlobalAttribute;
	using PA = PlayerAttribute;
	using HA = HexAttribute;

	/*
         * The encoding schema `{a, e, n, vmax, p}`, where:
         * a=attribute
         * e=encoding
         * n=size
         * vmax=max_value
         * p=param (encoding-specific)
         */
	using E5G = std::tuple<GlobalAttribute, Encoding, int, int, double>;
	using E5P = std::tuple<PlayerAttribute, Encoding, int, int, double>;
	using E5H = std::tuple<HexAttribute, Encoding, int, int, double>;
}

using GlobalEncoding = std::array<E5G, EI(GlobalAttribute::_count)>;
using PlayerEncoding = std::array<E5P, EI(PlayerAttribute::_count)>;
using HexEncoding = std::array<E5H, EI(HexAttribute::_count)>;

/*
     * Compile-time constructor for E5H and E5S tuples
     * https://stackoverflow.com/a/23784921
     */
template<typename T>
constexpr std::tuple<T, Encoding, int, int, double> E5(T a, Encoding e, int vmax, double slope = -1, int bins = -1)
{
	switch(e)
	{
		// "0" is a value => vmax+1 values
		case AE:
			return {a, e, vmax + 2, vmax, -1};
		case AI:
			return {a, e, vmax + 1, vmax, -1};
		case AM:
			return {a, e, vmax + 1, vmax, -1};
		case AS:
			return {a, e, vmax + 1, vmax, -1};
		case AZ:
			return {a, e, vmax + 1, vmax, -1};

		// Log2(8)=3 (2^3), but if vmax=8 then 4 bits will be required
		// => Log2(9)=4
		case BE:
			return {a, e, static_cast<int>(Log2(vmax + 1)) + 1, vmax, -1};
		case BM:
			return {a, e, static_cast<int>(Log2(vmax + 1)), vmax, -1};
		case BS:
			return {a, e, static_cast<int>(Log2(vmax + 1)), vmax, -1};
		case BZ:
			return {a, e, static_cast<int>(Log2(vmax + 1)), vmax, -1};

		// "0" is a category => vmax+1 categories
		case CE:
			return {a, e, vmax + 2, vmax, -1};
		case CI:
			return {a, e, vmax + 1, vmax, -1};
		case CM:
			return {a, e, vmax + 1, vmax, -1};
		case CS:
			return {a, e, vmax + 1, vmax, -1};
		case CZ:
			return {a, e, vmax + 1, vmax, -1};

		case EE:
			return {a, e, 2, vmax, slope};
		case EM:
			return {a, e, 1, vmax, slope};
		case ES:
			return {a, e, 1, vmax, slope};
		case EZ:
			return {a, e, 1, vmax, slope};

		case LE:
			return {a, e, 2, vmax, -1};
		case LM:
			return {a, e, 1, vmax, -1};
		case LS:
			return {a, e, 1, vmax, -1};
		case LZ:
			return {a, e, 1, vmax, -1};

		case RAW:
			return {a, e, 1, vmax, -1};
		default:
			throw std::runtime_error("Unexpected encoding: " + std::to_string(EI(e)));
	}
}

// 0-6 regular; 7=war machines; 8=other (summoned, commander, etc.)
constexpr int STACK_SLOT_WARMACHINES = 7;
constexpr int STACK_SLOT_SPECIAL = 8;

// Values above MAX are simply capped
constexpr int STACK_QUEUE_SIZE = 30;
constexpr int CREATURE_ID_MAX = 149; // H3 core has creature IDs 0..149
constexpr int STACK_SLOT_MAX = 8;

// NOTE: the generated maps use old AIValue() which is 4-6x LOWER
//       than the one calculated by MMAI (in Stack::CalcValue())
//       => a map with 100K pools corresponds to 400K..600K pools now
// The biggest pools are:
//   (1) 4x1096  => 500K pools (old)  => 3000K (new) => total = 6000K  (new)
//   (2) 8x64    => 800K pools (old)  => 4800K (new) => total = 9600K  (new)
//   (3) 8x64    => 1600K pools (old) => 9600K (new) => total = 19200K (new)
// Since (1) is used for training while (2) and (3) are for evaluation, we
// set max=10000K=10M (new) in order to:
// - test higher-than-trained values via (2), but within limits,
// - test higher-than-trained values via (3), but outside limits
//
// XXX: THIS IS NOW LEFT UNUSED (switched to relative values instead)
// constexpr int ARMY_VALUE_MAX = 10 * 1000 * 1000; // 10M

constexpr auto BFIELD_VALUE_MAX = int(10e6); // 4.2M max for 4x1024.vmap
constexpr auto BFIELD_VALUE_SLOPE = 5;

constexpr auto BFIELD_HP_MAX = int(200e3); // 90k max for 4x1024.vmap
constexpr auto BFIELD_HP_SLOPE = 7.5;

constexpr GlobalEncoding GLOBAL_ENCODING{
	E5(GA::BATTLE_SIDE, CS, 1),
	E5(GA::BATTLE_SIDE_ACTIVE_PLAYER, CE, 1), // NULL means no battle
	E5(GA::BATTLE_WINNER, CE, 1), // NULL means ongoing battle
	E5(GA::BFIELD_VALUE_START_ABS, ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
	E5(GA::BFIELD_VALUE_NOW_ABS, ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
	E5(GA::BFIELD_VALUE_NOW_REL0, LS, 1000), // bfield_value_now          / bfield_value_at_start
	E5(GA::BFIELD_HP_START_ABS, ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
	E5(GA::BFIELD_HP_NOW_ABS, ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
	E5(GA::BFIELD_HP_NOW_REL0, LS, 1000), // bfield_hp_now             / bfield_hp_at_start
	E5(GA::ACTION_MASK, BS, (1 << EI(GlobalAction::_count)) - 1)
};

constexpr auto VALUE_KILLED_NOW_MAX = int(2e6); // 4x1096 has 98 Ghost dragons => ~4K dmg => 8K (+22 attack)
constexpr auto VALUE_KILLED_NOW_NBINS = 50; // ^ vs. grand elf(15 HP) = 667 kills * 1.8k value = 1.2M
constexpr auto VALUE_KILLED_NOW_SLOPE = 7.5; // granularity at low values OK (1 imp = 213)

constexpr auto DMG_DEALT_NOW_MAX = int(20e3);
constexpr auto DMG_DEALT_NOW_NBINS = 50;
constexpr auto DMG_DEALT_NOW_SLOPE = 6.5;

constexpr PlayerEncoding PLAYER_ENCODING{
	E5(PA::BATTLE_SIDE, CS, 1),
	E5(PA::ARMY_VALUE_NOW_ABS, ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
	E5(PA::ARMY_VALUE_NOW_REL, LS, 1000), // army_value_now          / global_value_now
	E5(PA::ARMY_VALUE_NOW_REL0, LS, 1000), // army_value_now          / global_value_at_start
	E5(PA::ARMY_HP_NOW_ABS, ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
	E5(PA::ARMY_HP_NOW_REL, LS, 1000), // army_hp_now             / global_hp_now
	E5(PA::ARMY_HP_NOW_REL0, LS, 1000), // army_hp_now             / global_hp_at_start
	E5(PA::VALUE_KILLED_NOW_ABS, ES, VALUE_KILLED_NOW_MAX, VALUE_KILLED_NOW_SLOPE),
	E5(PA::VALUE_KILLED_NOW_REL, LS, 1000), // value_killed_this_turn  / global_value_last_turn
	E5(PA::VALUE_KILLED_ACC_ABS, ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
	E5(PA::VALUE_KILLED_ACC_REL0, LS, 1000), // value_killed_lifetime   / global_value_at_start
	E5(PA::VALUE_LOST_NOW_ABS, ES, VALUE_KILLED_NOW_MAX, VALUE_KILLED_NOW_SLOPE),
	E5(PA::VALUE_LOST_NOW_REL, LS, 1000), // value_lost_this_turn    / global_value_last_turn
	E5(PA::VALUE_LOST_ACC_ABS, ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
	E5(PA::VALUE_LOST_ACC_REL0, LS, 1000), // value_lost_lifetime     / global_value_at_start
	E5(PA::DMG_DEALT_NOW_ABS, ES, DMG_DEALT_NOW_MAX, DMG_DEALT_NOW_SLOPE),
	E5(PA::DMG_DEALT_NOW_REL, LS, 1000), // dmg_dealt_this_turn     / global_hp_last_turn
	E5(PA::DMG_DEALT_ACC_ABS, ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
	E5(PA::DMG_DEALT_ACC_REL0, LS, 1000), // dmg_dealt_lifetime      / global_hp_at_start
	E5(PA::DMG_RECEIVED_NOW_ABS, ES, DMG_DEALT_NOW_MAX, DMG_DEALT_NOW_SLOPE),
	E5(PA::DMG_RECEIVED_NOW_REL, LS, 1000), // dmg_received_this_turn  / global_hp_last_turn
	E5(PA::DMG_RECEIVED_ACC_ABS, ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
	E5(PA::DMG_RECEIVED_ACC_REL0, LS, 1000), // dmg_received_lifetime   / global_hp_at_start
};

// Visualise on https://www.desmos.com/calculator:
// ln(1 + (x/M) * (exp(S)-1))/S
// Add slider "S" (slope) and "M" (vmax).
// Play with the sliders to see the nonlinearity (use M=1 for best view)
// XXX: slope cannot be 0

constexpr auto STACK_QTY_MAX = 1500;
constexpr auto STACK_QTY_SLOPE = 5;

constexpr auto STACK_HP_MAX = 1000;
constexpr auto STACK_HP_SLOPE = 6;

constexpr auto STACK_VALUE_MAX = 200e3; // titan 55k, crystal dr. 113k, azure 180k...
constexpr auto STACK_VALUE_NBINS = 20;
constexpr auto STACK_VALUE_SLOPE = 6.5;

constexpr HexEncoding HEX_ENCODING{
	E5(HA::Y_COORD, CS, 10),
	E5(HA::X_COORD, CS, 14),
	E5(HA::STATE_MASK, BS, (1 << EI(HexState::_count)) - 1),
	E5(HA::ACTION_MASK, BZ, (1 << EI(HexAction::_count)) - 1),
	E5(HA::IS_REAR, CZ, 1), // 1=this is the rear hex of a stack
	E5(HA::STACK_SIDE, CE, 1), // 0=attacker, 1=defender
	E5(HA::STACK_SLOT, CE, STACK_SLOT_MAX),
	E5(HA::STACK_QUANTITY, EZ, STACK_QTY_MAX, STACK_QTY_SLOPE),
	E5(HA::STACK_ATTACK, LZ, 80),
	E5(HA::STACK_DEFENSE, LZ, 80), // azure dragon is 60 when defending
	E5(HA::STACK_SHOTS, LZ, 32), // sharpshooter is 32
	E5(HA::STACK_DMG_MIN, LZ, 100),
	E5(HA::STACK_DMG_MAX, LZ, 100),
	E5(HA::STACK_HP, EZ, STACK_HP_MAX, STACK_HP_SLOPE),
	E5(HA::STACK_HP_LEFT, EZ, STACK_HP_MAX, STACK_HP_SLOPE),
	E5(HA::STACK_SPEED, CE, 20),
	E5(HA::STACK_QUEUE, BZ, (1 << STACK_QUEUE_SIZE) - 1), // 0..14, 0=active stack
	// H4(SSTACK_A::ESTIMATED_DMG,  NE, 1000),      // est. dmg by the active stack as a permillage of this stack's total HP
	E5(HA::STACK_VALUE_ONE, EZ, STACK_VALUE_MAX, STACK_VALUE_SLOPE),
	E5(HA::STACK_FLAGS1, BZ, (1 << EI(StackFlag1::_count)) - 1),
	E5(HA::STACK_FLAGS2, BZ, (1 << EI(StackFlag2::_count)) - 1),

	E5(HA::STACK_VALUE_REL, LZ, 1000),
	E5(HA::STACK_VALUE_REL0, LZ, 1000),
	E5(HA::STACK_VALUE_KILLED_REL, LZ, 1000),
	E5(HA::STACK_VALUE_KILLED_ACC_REL0, LZ, 1000),
	E5(HA::STACK_VALUE_LOST_REL, LZ, 1000),
	E5(HA::STACK_VALUE_LOST_ACC_REL0, LZ, 1000),
	E5(HA::STACK_DMG_DEALT_REL, LZ, 1000),
	E5(HA::STACK_DMG_DEALT_ACC_REL0, LZ, 1000),
	E5(HA::STACK_DMG_RECEIVED_REL, LZ, 1000),
	E5(HA::STACK_DMG_RECEIVED_ACC_REL0, LZ, 1000),
};

// Dedining encodings for each attribute by hand is error-prone
// The below compile-time asserts are essential.
static_assert(UninitializedEncodingAttributes(GLOBAL_ENCODING) == 0, "Found uninitialized elements");
static_assert(UninitializedEncodingAttributes(PLAYER_ENCODING) == 0, "Found uninitialized elements");
static_assert(UninitializedEncodingAttributes(HEX_ENCODING) == 0, "Found uninitialized elements");
static_assert(DisarrayedEncodingAttributeIndex(GLOBAL_ENCODING) == -1, "Found wrong element at this index");
static_assert(DisarrayedEncodingAttributeIndex(PLAYER_ENCODING) == -1, "Found wrong element at this index");
static_assert(DisarrayedEncodingAttributeIndex(HEX_ENCODING) == -1, "Found wrong element at this index");
static_assert(MisconfiguredExpnormSlopeIndex(GLOBAL_ENCODING) == -1, "Found miscalculated binary vmax element at this index");
static_assert(MisconfiguredExpnormSlopeIndex(PLAYER_ENCODING) == -1, "Found miscalculated binary vmax element at this index");
static_assert(MisconfiguredExpnormSlopeIndex(HEX_ENCODING) == -1, "Found miscalculated binary vmax element at this index");

constexpr int BATTLEFIELD_STATE_SIZE_GLOBAL = EncodedSize(GLOBAL_ENCODING);
constexpr int BATTLEFIELD_STATE_SIZE_ONE_PLAYER = EncodedSize(PLAYER_ENCODING);
constexpr int BATTLEFIELD_STATE_SIZE_ONE_HEX = EncodedSize(HEX_ENCODING);
constexpr int BATTLEFIELD_STATE_SIZE_ALL_HEXES = 165 * BATTLEFIELD_STATE_SIZE_ONE_HEX;
constexpr int BATTLEFIELD_STATE_SIZE =
	BATTLEFIELD_STATE_SIZE_GLOBAL + BATTLEFIELD_STATE_SIZE_ONE_PLAYER + BATTLEFIELD_STATE_SIZE_ONE_PLAYER + BATTLEFIELD_STATE_SIZE_ALL_HEXES;
}
