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
constexpr Action ACTION_RESET = -1;
constexpr Action ACTION_RENDER_ANSI = -2;

// Value used when masking NULL values during encoding
constexpr int NULL_VALUE_ENCODED = -1;
constexpr int NULL_VALUE_UNENCODED = -1;

// Convenience definitions which do not need to be exported
namespace X
{
	inline constexpr auto CAT = Encoding::CATEGORICAL;
	inline constexpr auto LIN = Encoding::LINNORM;
	inline constexpr auto RAW = Encoding::RAW;
}

/*
 * Compile-time constructor for E5H and E5S tuples
 * https://stackoverflow.com/a/23784921
 */
template<typename T>
constexpr std::tuple<T, Encoding, int, int, double> E5(T a, Encoding e, int vmax)
{
	switch(e)
	{
		// "0" is a category => vmax+1 categories
		case X::CAT:
			return {a, e, vmax + 1, vmax, -1};
		case X::LIN:
		case X::RAW:
			return {a, e, 1, vmax, -1};
		default:
			throw std::runtime_error("Unexpected encoding: " + std::to_string(EI(e)));
	}
}

// TODO: many of these constants may be redundant in v15

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
    static constexpr std::string_view name = "Global";
    static constexpr encoding_type encoding = {
		// LS is the correct encoding for BATTLE_ROUND, but since it replaces BATTLE_SIDE
		// which had n=2 => use LE to keep the dimensions unchanged.
		E5(A::BATTLE_WINNER, X::CAT, 2), // 0=attacker, 1=defender, 2=no winner (e.g. ongonig battle)
		E5(A::BATTLE_ROUND, X::LIN, MAX_ROUNDS + 1),
		E5(A::HAS_UPPER_TOWER, X::RAW, 1),
		E5(A::HAS_MIDDLE_TOWER, X::RAW, 1),
		E5(A::HAS_BOTTOM_TOWER, X::RAW, 1),
		E5(A::HAS_GATE_CORPSE, X::RAW, 1),
		E5(A::HAS_BRIDGE_CORPSE, X::RAW, 1),
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Player>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Player>
{
    static constexpr auto element_type = Graph::ElementType::NODE_PLAYER;
    static constexpr std::string_view name = "Player";
    static constexpr encoding_type encoding = {
		E5(A::BATTLE_SIDE, X::CAT, 1),
		E5(A::IS_ACTIVE, X::RAW, 1),
		E5(A::ARMY_VALUE_NOW_REL, X::LIN, 1000), //     (army_value_now / global_value_now)
		E5(A::ARMY_HP_NOW_REL, X::LIN, 1000), //        (army_hp_now / global_hp_now)
		E5(A::VALUE_KILLED_NOW_REL, X::LIN, 1000), //   (value_killed_this_turn / global_value_last_turn)
		E5(A::VALUE_LOST_NOW_REL, X::LIN, 1000), //     (value_lost_this_turn / global_value_last_turn)
		E5(A::DMG_DEALT_NOW_REL, X::LIN, 1000), //      (dmg_dealt_this_turn / global_hp_last_turn)
		E5(A::DMG_RECEIVED_NOW_REL, X::LIN, 1000), //   (dmg_received_this_turn / global_hp_last_turn)
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Unit>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Unit>
{
    static constexpr Graph::ElementType element_type = Graph::ElementType::NODE_UNIT;
    static constexpr std::string_view name = "Unit";
    static constexpr encoding_type encoding = {
        E5(A::VALUE_REL, X::LIN, 1000), // stack_value_now / global_value_now
        E5(A::SHOTS, X::LIN, 32), // sharpshooter is 32
        E5(A::DMG_UNCERTAINTY, X::LIN, 1),
        E5(A::IS_ACTIVE, X::RAW, 1),
        E5(A::IS_ENEMY, X::RAW, 1),
        E5(A::IS_SLEEPING, X::RAW, 1),
        E5(A::IS_WAR_MACHINE, X::RAW, 1),
        E5(A::HAS_ADDITIONAL_ATTACK, X::RAW, 1),
        E5(A::HAS_ALL_AROUND_ATTACK, X::RAW, 1),
        E5(A::HAS_BLOCKS_RETALIATION, X::RAW, 1),
        E5(A::HAS_DEATH_CLOUD, X::RAW, 1),
        E5(A::HAS_DOUBLE_DAMAGE_CHANCE, X::LIN, 1000), // v=chance
        E5(A::HAS_FIREBALL, X::RAW, 1),
        E5(A::HAS_FLYING, X::RAW, 1),
        E5(A::HAS_LIFE_DRAIN, X::RAW, 1),
        E5(A::HAS_NON_LIVING, X::RAW, 1),
        E5(A::HAS_NO_MELEE_PENALTY, X::RAW, 1),
        E5(A::HAS_RETURN_AFTER_STRIKE, X::RAW, 1),
        E5(A::HAS_THREE_HEADED_ATTACK, X::RAW, 1),
        E5(A::HAS_TWO_HEX_ATTACK_BREATH, X::RAW, 1),
        E5(A::HAS_AGE, X::RAW, 3), // 			 	v=rounds
        E5(A::HAS_AGE_ATTACK, X::LIN, 1000), //      v=chance
        E5(A::HAS_BIND, X::RAW, 3), //            	v=rounds
        E5(A::HAS_BIND_ATTACK, X::LIN, 1000), //     v=chance
        E5(A::HAS_BLIND, X::RAW, 3), //           	v=rounds
        E5(A::HAS_BLIND_ATTACK, X::LIN, 1000), //    v=chance
        E5(A::HAS_CURSE, X::RAW, 3), //           	v=rounds
        E5(A::HAS_CURSE_ATTACK, X::LIN, 1000), //    v=chance
        E5(A::HAS_DISPEL_ATTACK, X::LIN, 1000), //   v=chance
        E5(A::HAS_PETRIFY, X::RAW, 3), //         	v=rounds
        E5(A::HAS_PETRIFY_ATTACK, X::LIN, 1000), //  v=chance
        E5(A::HAS_POISON, X::RAW, 3), //          	v=rounds
        E5(A::HAS_POISON_ATTACK, X::LIN, 1000), //   v=chance
        E5(A::HAS_WEAKNESS, X::RAW, 3), //        	v=rounds
        E5(A::HAS_WEAKNESS_ATTACK, X::LIN, 1000), // v=chance
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Hex>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Hex>
{
    static constexpr auto element_type = Graph::ElementType::NODE_HEX;
    static constexpr std::string_view name = "Hex";
	static constexpr encoding_type encoding = {
		E5(A::Y_COORD, X::CAT, 10),
		E5(A::X_COORD, X::CAT, 14),
		E5(A::STATE_MASK, X::RAW, EI(HexState::_count)),
		E5(A::WALL_HEALTH, X::LIN, MAX_WALL_HEALTH),
	};
};

template <>
struct EncodingTraits<Graph::NodeAttributes::Action>
: detail::EncodingTraitsBase<Graph::NodeAttributes::Action>
{
    static constexpr auto element_type = Graph::ElementType::NODE_ACTION;
    static constexpr std::string_view name = "Action";
    static constexpr encoding_type encoding = {
		E5(A::ACTION_TYPE, X::CAT, EI(A::_count)),
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
    static constexpr std::string_view name = #attr_type; \
    static constexpr encoding_type encoding = {}; \
}

GENERIC_EDGE_ENCODING_TRAITS(Global_Has_Player, EDGE_GLOBAL_HAS_PLAYER);
GENERIC_EDGE_ENCODING_TRAITS(Global_Has_Unit, EDGE_GLOBAL_HAS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Global_Has_Hex, EDGE_GLOBAL_HAS_HEX);
GENERIC_EDGE_ENCODING_TRAITS(Player_Owns_Unit, EDGE_PLAYER_OWNS_UNIT);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Hex_Adjacent_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Hex_Adjacent_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_HEX_ADJACENT_HEX;
	static constexpr std::string_view name = "Hex_Adjacent_Hex";
	static constexpr encoding_type encoding = {
		E5(A::DIRECTION, X::CAT, 5),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Unit_Blocks_Unit, EDGE_UNIT_BLOCKS_UNIT);
GENERIC_EDGE_ENCODING_TRAITS(Unit_Occupies_Hex, EDGE_UNIT_OCCUPIES_HEX);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_ActsBefore_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_ActsBefore_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_ACTS_BEFORE_UNIT;
	static constexpr std::string_view name = "Unit_ActsBefore_Unit";
	static constexpr encoding_type encoding = {
		E5(A::TIMES, X::LIN, 2),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_MeleeDmg_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_MeleeDmg_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_MELEE_DMG_UNIT;
    static constexpr std::string_view name = "Unit_MeleeDmg_Unit";

	static constexpr encoding_type encoding = {
		E5(A::ESTIMATED_ATTACKER_HPDIFF_REL_SELF, X::LIN, 1000),
		E5(A::ESTIMATED_ATTACKER_HPDIFF_REL_BF, X::LIN, 1000),
		E5(A::ESTIMATED_DEFENDER_HPDIFF_REL_SELF, X::LIN, 1000),
		E5(A::ESTIMATED_DEFENDER_HPDIFF_REL_BF, X::LIN, 1000),
		E5(A::ESTIMATED_NET_VALUE_REL_BF, X::LIN, 1000),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Unit_ShootDmg_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Unit_ShootDmg_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_UNIT_SHOOT_DMG_UNIT;
	static constexpr std::string_view name = "Unit_ShootDmg_Unit";
	static constexpr encoding_type encoding = {
		E5(A::ESTIMATED_ATTACKER_HPDIFF_REL_SELF, X::LIN, 1000),
		E5(A::ESTIMATED_ATTACKER_HPDIFF_REL_BF, X::LIN, 1000),
		E5(A::ESTIMATED_DEFENDER_HPDIFF_REL_SELF, X::LIN, 1000),
		E5(A::ESTIMATED_DEFENDER_HPDIFF_REL_BF, X::LIN, 1000),
		E5(A::ESTIMATED_NET_VALUE_REL_BF, X::LIN, 1000),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_EndsAt_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_EndsAt_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_ENDS_AT_HEX;
	static constexpr std::string_view name = "Action_EndsAt_Hex";
	static constexpr encoding_type encoding = {
		E5(A::IS_REAR, X::RAW, 1),
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
	static constexpr std::string_view name = "Action_ExposesToShootFrom_Unit";
	static constexpr encoding_type encoding = {
		E5(A::DMG_MULT, X::LIN, 1000),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_Melees_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_Melees_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_MELEES_UNIT;
	static constexpr std::string_view name = "Action_Melees_Unit";
	static constexpr encoding_type encoding = {
		E5(A::IS_PRIMARY_TARGET, X::CAT, 1),
	};
};

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_Shoots_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_Shoots_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_SHOOTS_UNIT;
	static constexpr std::string_view name = "Action_Shoots_Unit";
	static constexpr encoding_type encoding = {
		E5(A::IS_PRIMARY_TARGET, X::CAT, 1),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesMeleeAt_Unit, EDGE_ACTION_ENABLES_MELEE_AT_UNIT);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_EnablesShootAt_Unit>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_EnablesShootAt_Unit>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT;
	static constexpr std::string_view name = "Action_EnablesShootAt_Unit";
	static constexpr encoding_type encoding = {
		E5(A::DMG_MULT, X::LIN, 1000),
	};
};

GENERIC_EDGE_ENCODING_TRAITS(Action_EnablesMeleeAt_Hex, EDGE_ACTION_ENABLES_MELEE_AT_HEX);

template <>
struct EncodingTraits<Graph::EdgeAttributes::Action_EnablesShootAt_Hex>
: detail::EncodingTraitsBase<Graph::EdgeAttributes::Action_EnablesShootAt_Hex>
{
	static constexpr auto element_type = Graph::ElementType::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT;
	static constexpr std::string_view name = "Action_EnablesShootAt_Hex";
	static constexpr encoding_type encoding = {
		E5(A::DMG_MULT, X::LIN, 1000),
	};
};

template <typename AttrType>
consteval bool EncodingIsValid()
{
    constexpr const auto& encoding = EncodingTraits<AttrType>::encoding;

    // The explicit asserts here are used for more informative errors
    // (a return value is still needed to flag the problematic attribute type)
	static_assert(UninitializedEncodingAttributes(encoding) == 0, "Found uninitialized elements");
	static_assert(DisarrayedEncodingAttributeIndex(encoding) == -1, "Found wrong element at this index");

    return UninitializedEncodingAttributes(encoding) == 0
        && DisarrayedEncodingAttributeIndex(encoding) == -1;
}


static_assert(EncodingIsValid<Graph::NodeAttributes::Hex>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Global>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Player>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Unit>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Hex>());
static_assert(EncodingIsValid<Graph::NodeAttributes::Action>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Global_Has_Player>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Global_Has_Unit>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Global_Has_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Player_Owns_Unit>());
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
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesMeleeAt_Hex>());
static_assert(EncodingIsValid<Graph::EdgeAttributes::Action_EnablesShootAt_Hex>());
static_assert(static_cast<int>(Graph::ElementType::_count) == 26);

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
