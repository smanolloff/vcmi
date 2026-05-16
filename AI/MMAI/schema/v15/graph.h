/*
 * graph.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <cstdint>
#include <vector>

namespace MMAI::Schema::V15::Graph
{
    enum class ElementType : uint8_t
    {
        NODE_GLOBAL,
        NODE_PLAYER,
        NODE_UNIT,
        NODE_HEX,
        NODE_ACTION,

        EDGE_GLOBAL_HAS_PLAYER,
        EDGE_GLOBAL_HAS_UNIT,
        EDGE_GLOBAL_HAS_HEX,

        EDGE_PLAYER_OWNS_UNIT,
        EDGE_HEX_ADJACENT_HEX,
        EDGE_UNIT_ACTS_BEFORE_UNIT,
        EDGE_UNIT_MELEE_DMG_UNIT,      // regardless if reachable
        EDGE_UNIT_SHOOT_DMG_UNIT,      // regardless if blocked
        EDGE_UNIT_BLOCKS_UNIT,
        EDGE_UNIT_OCCUPIES_HEX,

        EDGE_ACTION_BY_UNIT,
        EDGE_ACTION_ENDS_AT_HEX,
        EDGE_ACTION_BLOCKS_UNIT,
        EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT,
        EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT,    // v=ranged penalty
        EDGE_ACTION_MELEES_UNIT, // v=primary target (e.g. for dragon breath, 3-headed attack, etc.)
        EDGE_ACTION_SHOOTS_UNIT, // v=primary target (e.g. for fireball)
        EDGE_ACTION_ENABLES_MELEE_AT_UNIT,
        EDGE_ACTION_ENABLES_SHOOT_AT_UNIT,

        // can explode to 350K for 14 archangels
        // => present only for active action nodes
        EDGE_ACTION_ENABLES_MELEE_AT_HEX,
        EDGE_ACTION_ENABLES_SHOOT_AT_HEX,
        _count
    };

    #define BLANK_ENUM_DEF(name)    \
    enum class name : uint8_t {     \
        _count                      \
    }

    namespace NodeAttributes
    {
        enum class Global : uint8_t
        {
            BATTLE_WINNER, //             0=left, 1=right (NA = battle not finished)
            BATTLE_ROUND,
            HAS_UPPER_TOWER,
            HAS_MIDDLE_TOWER,
            HAS_BOTTOM_TOWER,
            HAS_GATE_CORPSE,
            HAS_BRIDGE_CORPSE,

            _count
        };

        enum class Player : uint8_t
        {
            BATTLE_SIDE, //           0=left, 1=right
            IS_ACTIVE,
            ARMY_VALUE_NOW_REL, //    side_army_value_now / global_value_now
            ARMY_HP_NOW_REL, //       side_army_hp_now / global_hp_now
            VALUE_KILLED_NOW_REL, //  left_value_killed_this_turn / global_value_last_turn
            VALUE_LOST_NOW_REL, //    left_value_lost_this_turn / global_value_last_turn
            DMG_DEALT_NOW_REL, //     left_dmg_dealt_this_turn / global_hp_last_turn
            DMG_RECEIVED_NOW_REL, //  left_dmg_taken_this_turn / global_hp_last_turn

            _count
        };

        enum class Unit : uint8_t
        {
            VALUE_REL, // stack_value_now / global_value_now
            SHOTS,
            IS_ACTIVE,
            IS_ENEMY,
            IS_SLEEPING,
            IS_WAR_MACHINE,
            // IS_CLONED,   // these are useful, but not in the current
            // IS_SUMMONED, // training setup where they are never true
            HAS_ADDITIONAL_ATTACK,
            HAS_ALL_AROUND_ATTACK,
            HAS_BLOCKS_RETALIATION,
            HAS_DEATH_CLOUD,
            HAS_DOUBLE_DAMAGE_CHANCE,  // v=chance
            HAS_FIREBALL,
            HAS_FLYING,
            HAS_LIFE_DRAIN,
            HAS_NON_LIVING,
            HAS_NO_MELEE_PENALTY,
            HAS_RETURN_AFTER_STRIKE,
            HAS_THREE_HEADED_ATTACK,
            HAS_TWO_HEX_ATTACK_BREATH,
            HAS_AGE,
            HAS_AGE_ATTACK, //      v=chance
            HAS_BIND, //            v=rounds
            HAS_BIND_ATTACK, //     v=chance
            HAS_BLIND, //           v=rounds
            HAS_BLIND_ATTACK, //    v=chance
            HAS_CURSE, //           v=rounds
            HAS_CURSE_ATTACK, //    v=chance
            HAS_DISPEL_ATTACK, //   v=chance
            HAS_PETRIFY, //         v=rounds
            HAS_PETRIFY_ATTACK, //  v=chance
            HAS_POISON, //          v=rounds
            HAS_POISON_ATTACK, //   v=chance
            HAS_WEAKNESS, //        v=rounds
            HAS_WEAKNESS_ATTACK, // v=chance

            _count
        };

        enum class Hex : uint8_t
        {
            Y_COORD,
            X_COORD,
            STATE_MASK,
            WALL_HEALTH,

            _count
        };

        BLANK_ENUM_DEF(Action);
    }

namespace EdgeAttributes
    {
        BLANK_ENUM_DEF(Global_Has_Player);
        BLANK_ENUM_DEF(Global_Has_Unit);
        BLANK_ENUM_DEF(Global_Has_Hex);

        BLANK_ENUM_DEF(Player_Owns_Unit);

        enum class Hex_Adjacent_Hex : uint8_t
        {
            DIRECTION,
            _count
        };

        BLANK_ENUM_DEF(Unit_Blocks_Unit);
        BLANK_ENUM_DEF(Unit_Occupies_Hex);

        enum class Unit_ActsBefore_Unit : uint8_t
        {
            TIMES,
            _count
        };

        enum class Unit_MeleeDmg_Unit : uint8_t
        {
            ATTACK_DMG_MEAN_REL_OTHER,
            ATTACK_DMG_MEAN_REL_BF,
            ATTACK_DMG_STD_REL_OTHER,
            ATTACK_DMG_STD_REL_BF,
            ATTACK_VALUE_REL_BF,
            RETAL_DMG_MEAN_REL_OTHER,
            RETAL_DMG_MEAN_REL_BF,
            RETAL_DMG_STD_REL_OTHER,
            RETAL_DMG_STD_REL_BF,
            RETAL_VALUE_REL_BF,
            ATTACK_ONEKILL_CHANCE,
            ATTACK_ALLKILL_CHANCE,
            // RETAL_ONEKILL_CHANCE,  // too hard to calculate
            // RETAL_ALLKILL_CHANCE,  // too hard to calculate
            _count
        };

        enum class Unit_ShootDmg_Unit : uint8_t
        {
            ATTACK_DMG_MEAN_REL_OTHER,
            ATTACK_DMG_MEAN_REL_BF,
            ATTACK_DMG_STD_REL_OTHER,
            ATTACK_DMG_STD_REL_BF,
            ATTACK_VALUE_REL_BF,
            ATTACK_ONEKILL_CHANCE,
            ATTACK_ALLKILL_CHANCE,
            _count
        };

        BLANK_ENUM_DEF(Action_By_Unit);
        BLANK_ENUM_DEF(Action_Blocks_Unit);

        enum class Action_EndsAt_Hex : uint8_t
        {
            IS_REAR,
            _count
        };

        BLANK_ENUM_DEF(Action_ExposesToMeleeFrom_Unit);

        enum class Action_ExposesToShootFrom_Unit : uint8_t
        {
            DMG_MULT,  // 1=full dmg
            _count
        };

        enum class Action_Melees_Unit : uint8_t
        {
            IS_PRIMARY_TARGET,  // e.g. for dragons
            _count
        };

        enum class Action_Shoots_Unit : uint8_t
        {
            IS_PRIMARY_TARGET,  // e.g. for magogs
            _count
        };

        BLANK_ENUM_DEF(Action_EnablesMeleeAt_Unit);
        BLANK_ENUM_DEF(Action_EnablesShootAt_Unit);
        BLANK_ENUM_DEF(Action_EnablesMeleeAt_Hex);
        BLANK_ENUM_DEF(Action_EnablesShootAt_Hex);

        // 6 nodes, 26 edges
        static_assert(static_cast<int>(ElementType::_count) == 5 + 21);
    };

    class INode
    {
    public:
        virtual ElementType elementType() const = 0;
        virtual std::vector<int> rawAttributes() const = 0;
        virtual std::vector<float> encodedAttributes() const = 0;
        virtual std::string name() const = 0;
        virtual ~INode() = default;
    };

    using Endpoints = std::pair<const INode*, const INode*>;

    class IEdge
    {
    public:
        virtual ElementType elementType() const = 0;
        virtual std::vector<int> rawAttributes() const = 0;
        virtual std::vector<float> encodedAttributes() const = 0;
        virtual std::string name() const = 0;
        virtual Endpoints endpoints() const = 0;
        virtual ~IEdge() = default;
    };

    class IGraph
    {
    public:
        virtual std::vector<const INode*> getNodes(ElementType t) const = 0;
        virtual std::vector<const IEdge*> getEdges(ElementType t) const = 0;

        // Tuples of {Node ID, Action ID}
        // Node IDs are indexes in the result of getNodes(NodeType::ACTION)
        // Action IDs are the legacy numeric action (0..2312) for those nodes
        virtual std::vector<std::tuple<int, int>> getActiveNodeToActionIds() const = 0;

        virtual ~IGraph() = default;
    };

    inline constexpr std::array NODE_TYPES{
        ElementType::NODE_GLOBAL,
        ElementType::NODE_PLAYER,
        ElementType::NODE_UNIT,
        ElementType::NODE_HEX,
        ElementType::NODE_ACTION,
    };

    inline constexpr std::array EDGE_TYPES{
        ElementType::EDGE_GLOBAL_HAS_PLAYER,
        ElementType::EDGE_GLOBAL_HAS_UNIT,
        ElementType::EDGE_GLOBAL_HAS_HEX,
        ElementType::EDGE_PLAYER_OWNS_UNIT,
        ElementType::EDGE_HEX_ADJACENT_HEX,
        ElementType::EDGE_UNIT_ACTS_BEFORE_UNIT,
        ElementType::EDGE_UNIT_MELEE_DMG_UNIT,
        ElementType::EDGE_UNIT_SHOOT_DMG_UNIT,
        ElementType::EDGE_UNIT_BLOCKS_UNIT,
        ElementType::EDGE_UNIT_OCCUPIES_HEX,
        ElementType::EDGE_ACTION_BY_UNIT,
        ElementType::EDGE_ACTION_ENDS_AT_HEX,
        ElementType::EDGE_ACTION_BLOCKS_UNIT,
        ElementType::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT,
        ElementType::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT,
        ElementType::EDGE_ACTION_MELEES_UNIT,
        ElementType::EDGE_ACTION_SHOOTS_UNIT,
        ElementType::EDGE_ACTION_ENABLES_MELEE_AT_UNIT,
        ElementType::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT,
    };

    inline constexpr std::array ACTIVE_ACTION_EXCLUSIVE_EDGE_TYPES{
        ElementType::EDGE_ACTION_ENABLES_MELEE_AT_HEX,
        ElementType::EDGE_ACTION_ENABLES_SHOOT_AT_HEX,
    };

    static_assert(
        NODE_TYPES.size() +
        EDGE_TYPES.size() +
        ACTIVE_ACTION_EXCLUSIVE_EDGE_TYPES.size() == static_cast<int>(ElementType::_count));

} // namespace
