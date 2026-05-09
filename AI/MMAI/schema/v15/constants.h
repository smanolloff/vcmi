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

#include <stdexcept>
#include <string>
#include <tuple>

#include "schema/base.h"
#include "schema/v15/graph.h"
#include "schema/v15/types.h"
#include "schema/v15/util.h"

namespace MMAI::Schema::V15
{
constexpr int N_NONHEX_ACTIONS = 2;
constexpr Action ACTION_RETREAT = 0;
constexpr Action ACTION_WAIT = 1;
constexpr int N_HEX_ACTIONS = EI(HexAction::_count);
constexpr int N_ACTIONS = N_NONHEX_ACTIONS + (165 * N_HEX_ACTIONS);

// Control actions (not part of the regular action space)
constexpr Action ACTION_UNSET = -666;
constexpr Action ACTION_RESET = -1;
constexpr Action ACTION_RENDER_ANSI = -2;

// Value used when masking NULL values during encoding
constexpr int NULL_VALUE_ENCODED = -1;
constexpr int NULL_VALUE_UNENCODED = -1;

// Convenience definitions which do not need to be exported
namespace X
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
}

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
		case X::AE:
			return {a, e, vmax + 2, vmax, -1};
		case X::AI:
		case X::AM:
		case X::AS:
		case X::AZ:
			return {a, e, vmax + 1, vmax, -1};

		// Log2(8)=3 (2^3), but if vmax=8 then 4 bits will be required
		// => Log2(9)=4
		case X::BE:
			return {a, e, static_cast<int>(Log2(vmax + 1)) + 1, vmax, -1};
		case X::BM:
		case X::BS:
		case X::BZ:
			return {a, e, static_cast<int>(Log2(vmax + 1)), vmax, -1};

		// "0" is a category => vmax+1 categories
		case X::CE:
			return {a, e, vmax + 2, vmax, -1};
		case X::CI:
		case X::CM:
		case X::CS:
		case X::CZ:
			return {a, e, vmax + 1, vmax, -1};

		case X::LE:
			return {a, e, 2, vmax, -1};
		case X::LM:
		case X::LS:
		case X::LZ:
			return {a, e, 1, vmax, -1};

		case X::EE:
			return {a, e, 2, vmax, slope};
		case X::EM:
		case X::ES:
		case X::EZ:
			return {a, e, 1, vmax, slope};

		case X::RAW:
			return {a, e, 1, vmax, -1};
		default:
			throw std::runtime_error("Unexpected encoding: " + std::to_string(EI(e)));
	}
}

// 0-6 regular; 7=war machines; 8=other (summoned, commander, etc.)
constexpr int STACK_SLOT_WARMACHINES = 7;
constexpr int STACK_SLOT_SPECIAL = 8;

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
constexpr auto BFIELD_VALUE_MAX = static_cast<int>(10e6); // 4.2M max for 4x1024.vmap
constexpr auto BFIELD_VALUE_SLOPE = 5;
constexpr auto BFIELD_HP_MAX = static_cast<int>(200e3); // 90k max for 4x1024.vmap
constexpr auto BFIELD_HP_SLOPE = 7.5;
constexpr auto MAX_ROUNDS = 30;

// 100 Ghost dragons => ~4K base dmg
// vs. Grand Elf = 8K dmg (+22 attack advantage)
// => 667 kills * 1.8k value = 1.2M value killed
constexpr auto VALUE_KILLED_NOW_MAX = static_cast<int>(2e6);
constexpr auto VALUE_KILLED_NOW_NBINS = 50;
constexpr auto VALUE_KILLED_NOW_SLOPE = 7.5; // granularity at low values OK (1 imp = 213)

constexpr auto DMG_DEALT_NOW_MAX = static_cast<int>(20e3);
constexpr auto DMG_DEALT_NOW_NBINS = 50;
constexpr auto DMG_DEALT_NOW_SLOPE = 6.5;

// Values above MAX are simply capped
constexpr int STACK_QUEUE_SIZE = 30;
constexpr int CREATURE_ID_MAX = 149; // H3 core has creature IDs 0..149
constexpr int STACK_SLOT_MAX = 8;


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

constexpr auto MAX_WALL_HEALTH = 3;  // can be increased via mod tho

namespace detail
{
	template <typename AttrType>
	struct EncodingTraitsBase
	{
		/*
		 * attr_enc_schema_type is the `{a, e, n, vmax, p}` tuple, where:
		 *   a=attribute
		 *   e=encoding
		 *   n=size
		 *   vmax=max_value
		 *   p=param (encoding-specific)
		 */
		using A = AttrType;
		using attr_enc_schema_type = std::tuple<AttrType, Encoding, int, int, double>;
		using encoding_type = std::array<attr_enc_schema_type, EI(AttrType::_count)>;
	    static constexpr std::size_t attr_count = EI(AttrType::_count);
	};
}

template <typename AttrType>
struct EncodingTraits;

template <>
struct EncodingTraits<Graph::NodeAttributes::Global>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Global>
{
    static constexpr auto element_type = Graph::ElementType::NODE_GLOBAL;
    static constexpr std::string_view name = "GLOBAL_ENCODING";
    static constexpr encoding_type encoding = {
		// LS is the correct encoding for BATTLE_ROUND, but since it replaces BATTLE_SIDE
		// which had n=2 => use LE to keep the dimensions unchanged.
		E5(A::BATTLE_ROUND, X::LE, MAX_ROUNDS + 1),
		E5(A::BATTLE_SIDE_ACTIVE_PLAYER, X::CE, 1), // NULL means no battle
		E5(A::BATTLE_WINNER, X::CE, 1), // NULL means ongoing battle
		E5(A::BFIELD_VALUE_START_ABS, X::ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
		E5(A::BFIELD_VALUE_NOW_ABS, X::ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
		E5(A::BFIELD_VALUE_NOW_REL0, X::LS, 1000), // bfield_value_now / bfield_value_at_start
		E5(A::BFIELD_HP_START_ABS, X::ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
		E5(A::BFIELD_HP_NOW_ABS, X::ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
		E5(A::BFIELD_HP_NOW_REL0, X::LS, 1000), // bfield_hp_now / bfield_hp_at_start
		E5(A::SIEGE_TOWERS, X::BS, (1 << 3) - 1),
		E5(A::SIEGE_CORPSES, X::BS, (1 << 2) - 1),
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Player>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Player>
{
    static constexpr auto element_type = Graph::ElementType::NODE_PLAYER;
    static constexpr std::string_view name = "PLAYER_ENCODING";
    static constexpr encoding_type encoding = {
		E5(A::BATTLE_SIDE, X::CS, 1),
		E5(A::ARMY_VALUE_NOW_ABS, X::ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
		E5(A::ARMY_VALUE_NOW_REL, X::LS, 1000), //     (army_value_now / global_value_now)
		E5(A::ARMY_VALUE_NOW_REL0, X::LS, 1000), //    (army_value_now / global_value_at_start)
		E5(A::ARMY_HP_NOW_ABS, X::ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
		E5(A::ARMY_HP_NOW_REL, X::LS, 1000), //        (army_hp_now / global_hp_now)
		E5(A::ARMY_HP_NOW_REL0, X::LS, 1000), //       (army_hp_now / global_hp_at_start)
		E5(A::VALUE_KILLED_NOW_ABS, X::ES, VALUE_KILLED_NOW_MAX, VALUE_KILLED_NOW_SLOPE),
		E5(A::VALUE_KILLED_NOW_REL, X::LS, 1000), //   (value_killed_this_turn / global_value_last_turn)
		E5(A::VALUE_KILLED_ACC_ABS, X::ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
		E5(A::VALUE_KILLED_ACC_REL0, X::LS, 1000), //  (value_killed_lifetime / global_value_at_start)
		E5(A::VALUE_LOST_NOW_ABS, X::ES, VALUE_KILLED_NOW_MAX, VALUE_KILLED_NOW_SLOPE),
		E5(A::VALUE_LOST_NOW_REL, X::LS, 1000), //     (value_lost_this_turn / global_value_last_turn)
		E5(A::VALUE_LOST_ACC_ABS, X::ES, BFIELD_VALUE_MAX, BFIELD_VALUE_SLOPE),
		E5(A::VALUE_LOST_ACC_REL0, X::LS, 1000), //    (value_lost_lifetime / global_value_at_start)
		E5(A::DMG_DEALT_NOW_ABS, X::ES, DMG_DEALT_NOW_MAX, DMG_DEALT_NOW_SLOPE),
		E5(A::DMG_DEALT_NOW_REL, X::LS, 1000), //      (dmg_dealt_this_turn / global_hp_last_turn)
		E5(A::DMG_DEALT_ACC_ABS, X::ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
		E5(A::DMG_DEALT_ACC_REL0, X::LS, 1000), //     (dmg_dealt_lifetime / global_hp_at_start)
		E5(A::DMG_RECEIVED_NOW_ABS, X::ES, DMG_DEALT_NOW_MAX, DMG_DEALT_NOW_SLOPE),
		E5(A::DMG_RECEIVED_NOW_REL, X::LS, 1000), //   (dmg_received_this_turn / global_hp_last_turn)
		E5(A::DMG_RECEIVED_ACC_ABS, X::ES, BFIELD_HP_MAX, BFIELD_HP_SLOPE),
		E5(A::DMG_RECEIVED_ACC_REL0, X::LS, 1000), //  (dmg_received_lifetime / global_hp_at_start)
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Unit>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Unit>
{
    static constexpr Graph::ElementType element_type = Graph::ElementType::NODE_UNIT;
    static constexpr std::string_view name = "UNIT_ENCODING";
    static constexpr encoding_type encoding = {
		E5(A::SIDE, X::CE, 1), // 0=attacker, 1=defender
		E5(A::SLOT, X::CE, STACK_SLOT_MAX),
		E5(A::QUANTITY, X::EZ, STACK_QTY_MAX, STACK_QTY_SLOPE),
		E5(A::ATTACK, X::LZ, 80),
		E5(A::DEFENSE, X::LZ, 80), // azure dragon is 60 when defending
		E5(A::SHOTS, X::LZ, 32), // sharpshooter is 32
		E5(A::DMG_MIN, X::LZ, 100),
		E5(A::DMG_MAX, X::LZ, 100),
		E5(A::HP, X::EZ, STACK_HP_MAX, STACK_HP_SLOPE),
		E5(A::HP_LEFT, X::EZ, STACK_HP_MAX, STACK_HP_SLOPE),
		E5(A::SPEED, X::CE, 20),
		E5(A::VALUE_ONE, X::EZ, STACK_VALUE_MAX, STACK_VALUE_SLOPE),
		E5(A::FLAGS1, X::BZ, (1 << EI(StackFlag1::_count)) - 1),
		E5(A::FLAGS2, X::BZ, (1 << EI(StackFlag2::_count)) - 1),

		E5(A::VALUE_REL, X::LZ, 1000),
		E5(A::VALUE_REL0, X::LZ, 1000),
		E5(A::VALUE_KILLED_REL, X::LZ, 1000),
		E5(A::VALUE_KILLED_ACC_REL0, X::LZ, 1000),
		E5(A::VALUE_LOST_REL, X::LZ, 1000),
		E5(A::VALUE_LOST_ACC_REL0, X::LZ, 1000),
		E5(A::DMG_DEALT_REL, X::LZ, 1000),
		E5(A::DMG_DEALT_ACC_REL0, X::LZ, 1000),
		E5(A::DMG_RECEIVED_REL, X::LZ, 1000),
		E5(A::DMG_RECEIVED_ACC_REL0, X::LZ, 1000),
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Hex>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Hex>
{
    static constexpr auto element_type = Graph::ElementType::NODE_HEX;
    static constexpr std::string_view name = "HEX_ENCODING";
	static constexpr encoding_type encoding = {
		E5(A::Y_COORD, X::CS, 10),
		E5(A::X_COORD, X::CS, 14),
		E5(A::STATE_MASK, X::BS, (1 << EI(HexState::_count)) - 1),
		E5(A::ACTION_MASK, X::BZ, (1 << EI(HexAction::_count)) - 1),
		E5(A::IS_REAR, X::CZ, 1), // 1=this is the rear hex of a stack
		E5(A::IS_RUFR, X::CS, 1), // 1=this is the rear part of a RUFR pair
		E5(A::WALL_HEALTH, X::LE, MAX_WALL_HEALTH),
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Action>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Action>
{
    static constexpr auto element_type = Graph::ElementType::NODE_ACTION;
    static constexpr std::string_view name = "ACTION_ENCODING";
    static constexpr encoding_type encoding = {
    	E5(A::TYPE, X::RAW, EI(ActionType::_count))
    };
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Actaction>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Actaction>
{
    static constexpr auto element_type = Graph::ElementType::NODE_ACTACTION;
    static constexpr std::string_view name = "ACTACTION_ENCODING";
    static constexpr encoding_type encoding = {
    	E5(A::TYPE, X::RAW, EI(ActionType::_count)),
		E5(A::ID, X::RAW, N_ACTIONS)
	};
};


/*
 * The macro is useful for generic edges which have no attributes.
 *
 * GENERIC_EDGE_ENCODING_TRAITS(Foo, BAR) expands to:
 *
 *     using E5E_Foo = std::tuple<Graph::EdgeAttributes::Foo, Encoding, int, int, double>;
 *     namespace X {
 *         using E5E_Foo = std::tuple<Graph::EdgeAttributes::Foo, Encoding, int, int, double>;
 *     }
 *     using EdgeEncoding_Foo = std::array<X::E5E_Foo, EI(Graph::EdgeAttributes::Foo::_count)>;
 *     template <>
 *     struct EncodingTraits<EdgeEncoding_Foo>
 *     {
 *         using attr_type = Graph::EdgeAttributes::Foo;
 *         static constexpr auto element_type = Graph::ElementType::BAR;
 *         static constexpr std::string_view name = "BAR";
 *         static constexpr std::size_t attr_count = 0;
 *         static constexpr EdgeEncoding_##edge_type encoding = {};
 *         static constexpr std::size_t encoded_size = 0;
 *     };
 */

#define GENERIC_EDGE_ENCODING_TRAITS(attr_type, elem_type) \
template <> \
struct EncodingTraits<Graph::EdgeAttributes::attr_type> \
: detail::EncodingTraitsBase<Graph::EdgeAttributes::attr_type> \
{ \
    static constexpr auto element_type = Graph::ElementType::elem_type; \
    static constexpr std::string_view name = #elem_type; \
    static constexpr encoding_type encoding = {}; \
}

template <>
struct EncodingTraits<Graph::EdgeAttributes::Hex_Adjacent_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Hex_Adjacent_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_HEX_ADJACENT_HEX;
	static constexpr std::string_view name = "EDGE_ENCODING_HEX_ADJACENT_HEX";
	static constexpr encoding_type encoding = {
		E5(A::DIRECTION, X::CS, 5),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Unit_Blocks_Unit, EDGE_UNIT_BLOCKS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Unit_Occupies_Hex, EDGE_UNIT_OCCUPIES_HEX);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_ActsBefore_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_ActsBefore_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_ACTS_BEFORE_UNIT;
	static constexpr std::string_view name = "EDGE_ENCODING_UNIT_ACTS_BEFORE_UNIT";
	static constexpr encoding_type encoding = {
		E5(A::TIMES, X::LZ, 2),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_MeleeDmg_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_MeleeDmg_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_MELEE_DMG_UNIT;
    static constexpr std::string_view name = "EDGE_ENCODING_UNIT_MELEE_DMG_UNIT";

	static constexpr encoding_type encoding = {
		E5(A::ATTACK_DMG_MEAN_REL_OTHER, X::LS, 1000),
		E5(A::ATTACK_DMG_MEAN_REL_BF, X::LS, 1000),
		E5(A::ATTACK_DMG_STD_REL_OTHER, X::LS, 1000),
		E5(A::ATTACK_DMG_STD_REL_BF, X::LS, 1000),
		E5(A::ATTACK_VALUE_REL_BF, X::LS, 1000),
		E5(A::RETAL_DMG_MEAN_REL_OTHER, X::LS, 1000),
		E5(A::RETAL_DMG_MEAN_REL_BF, X::LS, 1000),
		E5(A::RETAL_DMG_STD_REL_OTHER, X::LS, 1000),
		E5(A::RETAL_DMG_STD_REL_BF, X::LS, 1000),
		E5(A::RETAL_VALUE_REL_BF, X::LS, 1000),
		E5(A::ATTACK_ALLKILL_CHANCE, X::LS, 1000),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_ShootDmg_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_ShootDmg_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_SHOOT_DMG_UNIT;
	static constexpr std::string_view name = "EDGE_ENCODING_UNIT_SHOOT_DMG_UNIT";
	static constexpr encoding_type encoding = {
		E5(A::ATTACK_DMG_MEAN_REL_OTHER, X::LS, 1000),
		E5(A::ATTACK_DMG_MEAN_REL_BF, X::LS, 1000),
		E5(A::ATTACK_DMG_STD_REL_OTHER, X::LS, 1000),
		E5(A::ATTACK_DMG_STD_REL_BF, X::LS, 1000),
		E5(A::ATTACK_VALUE_REL_BF, X::LS, 1000),
		E5(A::ATTACK_ALLKILL_CHANCE, X::LS, 1000),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_EndsAt_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_EndsAt_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_ENDS_AT_HEX;
	static constexpr std::string_view name = "EDGE_ENCODING_EDGE_ACTION_ENDS_AT_HEX";
	static constexpr encoding_type encoding = {
		E5(A::IS_REAR, X::BS, 1),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Actaction_EndsAt_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Actaction_EndsAt_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTACTION_ENDS_AT_HEX;
	static constexpr std::string_view name = "EDGE_ENCODING_EDGE_ACTACTION_ENDS_AT_HEX";
	static constexpr encoding_type encoding = {
		E5(A::IS_REAR, X::BS, 1),
	};
};


GENERIC_EDGE_ENCODING_TRAITS(Action_By_Unit, EDGE_ACTION_BY_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Action_Blocks_Unit, EDGE_ACTION_BLOCKS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Action_ExposesToMeleeFrom_Unit, EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_ExposesToShootFrom_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_ExposesToShootFrom_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT;
	static constexpr std::string_view name = "EDGE_ENCODING_EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT";
	static constexpr encoding_type encoding = {
		E5(A::DMG_MULT, X::LS, 1000),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Action_Melees_Unit, EDGE_ACTION_MELEES_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Action_Shoots_Unit, EDGE_ACTION_SHOOTS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesMeleeAt_Unit, EDGE_ACTION_ENABLES_MELEE_AT_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesShootAt_Unit, EDGE_ACTION_ENABLES_SHOOT_AT_UNIT);
#ifdef MMAI_ENABLE_EDGE_ACTION_ENABLES
GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesMeleeAt_Hex, EDGE_ACTION_ENABLES_MELEE_AT_HEX);
GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesShootAt_Hex, EDGE_ACTION_ENABLES_SHOOT_AT_HEX);
#endif

GENERIC_EDGE_ENCODING_TRAITS(Actaction_By_Unit, EDGE_ACTACTION_BY_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_Blocks_Unit, EDGE_ACTACTION_BLOCKS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_ExposesToMeleeFrom_Unit, EDGE_ACTACTION_EXPOSES_TO_MELEE_FROM_UNIT);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Actaction_ExposesToShootFrom_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Actaction_ExposesToShootFrom_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTACTION_EXPOSES_TO_SHOOT_FROM_UNIT;
	static constexpr std::string_view name = "EDGE_ENCODING_EDGE_ACTACTION_EXPOSES_TO_SHOOT_FROM_UNIT";
	static constexpr encoding_type encoding = {
		E5(A::DMG_MULT, X::LS, 1000),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Actaction_Melees_Unit, EDGE_ACTACTION_MELEES_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_Shoots_Unit, EDGE_ACTACTION_SHOOTS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_EnablesMeleeAt_Unit, EDGE_ACTACTION_ENABLES_MELEE_AT_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_EnablesShootAt_Unit, EDGE_ACTACTION_ENABLES_SHOOT_AT_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_EnablesMeleeAt_Hex, EDGE_ACTACTION_ENABLES_MELEE_AT_HEX);
GENERIC_EDGE_ENCODING_TRAITS(Actaction_EnablesShootAt_Hex, EDGE_ACTACTION_ENABLES_SHOOT_AT_HEX);

template <typename AttrType>
consteval bool EncodingIsValid()
{
    constexpr const auto& encoding = EncodingTraits<AttrType>::encoding;

    // The explicit asserts here are used for more informative errors
    // (a return value is still needed to flag the problematic attribute type)
	static_assert(UninitializedEncodingAttributes(encoding) == 0);
	static_assert(DisarrayedEncodingAttributeIndex(encoding) == -1);
	static_assert(MisconfiguredExpnormSlopeIndex(encoding) == -1);

    return UninitializedEncodingAttributes(encoding) == 0
        && DisarrayedEncodingAttributeIndex(encoding) == -1
        && MisconfiguredExpnormSlopeIndex(encoding) == -1;}


static_assert(EncodingIsValid<Graph::NodeAttributes::Hex>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Hex>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Global>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Player>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Unit>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Hex>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Action>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Actaction>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Hex_Adjacent_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Unit_ActsBefore_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Unit_MeleeDmg_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Unit_ShootDmg_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Unit_Blocks_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Unit_Occupies_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_By_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EndsAt_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_ExposesToMeleeFrom_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_ExposesToShootFrom_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_Melees_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_Shoots_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesMeleeAt_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesShootAt_Unit>());
#ifdef MMAI_ENABLE_EDGE_ACTION_ENABLES_AT_HEX
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesMeleeAt_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesShootAt_Hex>());
#endif
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_By_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_EndsAt_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_ExposesToMeleeFrom_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_ExposesToShootFrom_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_Melees_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_Shoots_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_EnablesMeleeAt_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_EnablesShootAt_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_EnablesMeleeAt_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Actaction_EnablesShootAt_Hex>());

/*
 * These below are not really used
 * They are just for informational purposes
 */

/*
constexpr int MAX_NUM_NODES_GLOBAL = 1;
constexpr int MAX_NUM_NODES_PLAYER = 2;
constexpr int MAX_NUM_NODES_UNIT = 30;
constexpr int MAX_NUM_NODES_HEX = 165;

constexpr int MAX_NUM_EDGES_ACTION_EXPOSES_TO_UNIT = MAX_NUM_NODES_UNIT * N_ACTIONS;
constexpr int MAX_NUM_EDGES_ACTION_THREATENS_UNIT = MAX_NUM_NODES_UNIT * N_ACTIONS;
constexpr int MAX_NUM_EDGES_ACTION_DAMAGES_UNIT = 2 * N_ACTIONS;
constexpr int MAX_NUM_EDGES_ACTION_ENDS_AT_HEX = 165 * EI(HexAction::_count);
constexpr int MAX_NUM_EDGES_ACTION_BY_UNIT = N_ACTIONS;  // actions are only by active unit
constexpr int MAX_NUM_EDGES_UNIT_BLOCKS_UNIT = 100;  // blind guess
constexpr int MAX_NUM_EDGES_UNIT_MELEE_DMG_UNIT = 420;  // 15 vs 15 units
constexpr int MAX_NUM_EDGES_UNIT_RANGED_DMG_UNIT = 420;  // 15 vs 15 shooters
constexpr int MAX_NUM_EDGES_UNIT_THREATENS_UNIT = 420;
constexpr int MAX_NUM_EDGES_UNIT_ACTS_BEFORE_UNIT = 210;
constexpr int MAX_NUM_EDGES_UNIT_THREATENS_HEX = 165 * MAX_NUM_NODES_UNIT;
constexpr int MAX_NUM_EDGES_UNIT_OCCUPIES_HEX = 2 * MAX_NUM_NODES_UNIT;
constexpr int MAX_NUM_EDGES_HEX_ADJACENT_HEX = 165 * 6;

constexpr int ENCODED_NODE_SIZE_GLOBAL = EncodedSize(EncodingTraits<NodeEncoding_Global>::encoding);
constexpr int ENCODED_NODE_SIZE_PLAYER = EncodedSize(EncodingTraits<NodeEncoding_Player>::encoding);
constexpr int ENCODED_NODE_SIZE_UNIT = EncodedSize(EncodingTraits<NodeEncoding_Unit>::encoding);
constexpr int ENCODED_NODE_SIZE_HEX = EncodedSize(EncodingTraits<NodeEncoding_Hex>::encoding);

constexpr int BATTLEFIELD_STATE_SIZE =
	ENCODED_NODE_SIZE_GLOBAL
	+ (2 * ENCODED_NODE_SIZE_PLAYER)
	+ (165 * ENCODED_NODE_SIZE_HEX);

constexpr int ENCODED_EDGE_SIZE_ACTION_EXPOSES_TO_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_ACTION_THREATENS_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_ACTION_DAMAGES_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_ACTION_ENDS_AT_HEX = 0;
constexpr int ENCODED_EDGE_SIZE_ACTION_BY_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_UNIT_BLOCKS_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_UNIT_MELEE_DMG_UNIT = EncodedSize(EncodingTraits<EdgeEncoding_Unit_MeleeDmg_Unit>::encoding);
constexpr int ENCODED_EDGE_SIZE_UNIT_RANGED_DMG_UNIT = EncodedSize(EncodingTraits<EdgeEncoding_Unit_ShootDmg_Unit>::encoding);
constexpr int ENCODED_EDGE_SIZE_UNIT_THREATENS_UNIT = 0;
constexpr int ENCODED_EDGE_SIZE_UNIT_ACTS_BEFORE_UNIT = EncodedSize(EncodingTraits<EdgeEncoding_Unit_ActsBefore_Unit>::encoding);
constexpr int ENCODED_EDGE_SIZE_UNIT_THREATENS_HEX = 0;
constexpr int ENCODED_EDGE_SIZE_UNIT_OCCUPIES_HEX = 0;
constexpr int ENCODED_EDGE_SIZE_HEX_ADJACENT_HEX = EncodedSize(EncodingTraits<EdgeEncoding_Hex_Adjacent_Hex>::encoding);

// Not used anywhere, but gives good idea of the theoreticla maximum state size
constexpr int BATTLEFIELD_STATE_SIZE_MAX =
	(MAX_NUM_NODES_GLOBAL * ENCODED_NODE_SIZE_GLOBAL)
	+ (MAX_NUM_NODES_PLAYER * ENCODED_NODE_SIZE_PLAYER)
	+ (MAX_NUM_NODES_UNIT * ENCODED_NODE_SIZE_UNIT)
	+ (MAX_NUM_NODES_HEX * ENCODED_NODE_SIZE_HEX)
	+ (MAX_NUM_EDGES_ACTION_EXPOSES_TO_UNIT * ENCODED_EDGE_SIZE_ACTION_EXPOSES_TO_UNIT)
	+ (MAX_NUM_EDGES_ACTION_THREATENS_UNIT * ENCODED_EDGE_SIZE_ACTION_THREATENS_UNIT)
	+ (MAX_NUM_EDGES_ACTION_DAMAGES_UNIT * ENCODED_EDGE_SIZE_ACTION_DAMAGES_UNIT)
	+ (MAX_NUM_EDGES_ACTION_ENDS_AT_HEX * ENCODED_EDGE_SIZE_ACTION_ENDS_AT_HEX)
	+ (MAX_NUM_EDGES_ACTION_BY_UNIT * ENCODED_EDGE_SIZE_ACTION_BY_UNIT)
	+ (MAX_NUM_EDGES_UNIT_BLOCKS_UNIT * ENCODED_EDGE_SIZE_UNIT_BLOCKS_UNIT)
	+ (MAX_NUM_EDGES_UNIT_MELEE_DMG_UNIT * ENCODED_EDGE_SIZE_UNIT_MELEE_DMG_UNIT)
	+ (MAX_NUM_EDGES_UNIT_RANGED_DMG_UNIT * ENCODED_EDGE_SIZE_UNIT_RANGED_DMG_UNIT)
	+ (MAX_NUM_EDGES_UNIT_THREATENS_UNIT * ENCODED_EDGE_SIZE_UNIT_THREATENS_UNIT)
	+ (MAX_NUM_EDGES_UNIT_ACTS_BEFORE_UNIT * ENCODED_EDGE_SIZE_UNIT_ACTS_BEFORE_UNIT)
	+ (MAX_NUM_EDGES_UNIT_THREATENS_HEX * ENCODED_EDGE_SIZE_UNIT_THREATENS_HEX)
	+ (MAX_NUM_EDGES_UNIT_OCCUPIES_HEX * ENCODED_EDGE_SIZE_UNIT_OCCUPIES_HEX)
	+ (MAX_NUM_EDGES_HEX_ADJACENT_HEX * ENCODED_EDGE_SIZE_HEX_ADJACENT_HEX)
	// edge index
	+ (2 * MAX_NUM_EDGES_ACTION_EXPOSES_TO_UNIT)
	+ (2 * MAX_NUM_EDGES_ACTION_THREATENS_UNIT)
	+ (2 * MAX_NUM_EDGES_ACTION_DAMAGES_UNIT)
	+ (2 * MAX_NUM_EDGES_ACTION_ENDS_AT_HEX)
	+ (2 * MAX_NUM_EDGES_ACTION_BY_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_BLOCKS_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_MELEE_DMG_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_RANGED_DMG_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_THREATENS_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_ACTS_BEFORE_UNIT)
	+ (2 * MAX_NUM_EDGES_UNIT_THREATENS_HEX)
	+ (2 * MAX_NUM_EDGES_UNIT_OCCUPIES_HEX)
	+ (2 * MAX_NUM_EDGES_HEX_ADJACENT_HEX);
*/

}
