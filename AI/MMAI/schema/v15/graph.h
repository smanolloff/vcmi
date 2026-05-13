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

        EDGE_GLOBAL_YIELDS_PLAYER,
        EDGE_PLAYER_OWNS_UNIT,
        EDGE_HEX_ADJACENT_HEX,
        EDGE_UNIT_ACTS_BEFORE_UNIT,
        EDGE_UNIT_MELEE_DMG_UNIT,      // regardless if reachable
        EDGE_UNIT_SHOOT_DMG_UNIT,      // regardless if blocked
        EDGE_UNIT_BLOCKS_UNIT,
        EDGE_UNIT_OCCUPIES_HEX,

        EDGE_ACTION_BY_UNIT,
        EDGE_ACTION_BLOCKS_UNIT,
        EDGE_ACTION_ENDS_AT_HEX,
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
            BATTLE_ROUND,
            BATTLE_SIDE_ACTIVE_PLAYER, // 0=left, 1=right (NA = battle finished)
            BATTLE_WINNER, //             0=left, 1=right (NA = battle not finished)
            BFIELD_VALUE_START_ABS, //    global_value_at_start
            BFIELD_VALUE_NOW_ABS, //      global_value_now
            BFIELD_VALUE_NOW_REL0, //     global_value_now / global_value_at_start
            BFIELD_HP_START_ABS, //       global_hp_at_start
            BFIELD_HP_NOW_ABS, //         global_hp_now
            BFIELD_HP_NOW_REL0, //        global_hp_now / global_hp_at_start
            SIEGE_TOWERS, //              {upper, keep, lower}
            SIEGE_CORPSES, //             {gate, bridge}

            _count
        };

        enum class Player : uint8_t
        {
            BATTLE_SIDE, //           0=left, 1=right
            ARMY_VALUE_NOW_ABS,
            ARMY_VALUE_NOW_REL, //    side_army_value_now / global_value_now
            ARMY_VALUE_NOW_REL0, //   side_army_value_now / global_value_at_start
            ARMY_HP_NOW_ABS,
            ARMY_HP_NOW_REL, //       side_army_hp_now / global_hp_now
            ARMY_HP_NOW_REL0, //      side_army_hp_now / global_hp_at_start
            VALUE_KILLED_NOW_ABS,
            VALUE_KILLED_NOW_REL, //  left_value_killed_this_turn / global_value_last_turn
            VALUE_KILLED_ACC_ABS,
            VALUE_KILLED_ACC_REL0, // left_value_killed_lifetime / global_value_at_start
            VALUE_LOST_NOW_ABS,
            VALUE_LOST_NOW_REL, //    left_value_lost_this_turn / global_value_last_turn
            VALUE_LOST_ACC_ABS,
            VALUE_LOST_ACC_REL0, //   left_value_lost_lifetime / global_value_at_start
            DMG_DEALT_NOW_ABS,
            DMG_DEALT_NOW_REL, //     left_dmg_dealt_this_turn / global_hp_last_turn
            DMG_DEALT_ACC_ABS,
            DMG_DEALT_ACC_REL0, //    left_dmg_dealt_lifetime / global_hp_at_start
            DMG_RECEIVED_NOW_ABS,
            DMG_RECEIVED_NOW_REL, //  left_dmg_taken_this_turn / global_hp_last_turn
            DMG_RECEIVED_ACC_ABS,
            DMG_RECEIVED_ACC_REL0, // left_dmg_taken_lifetime / global_hp_at_start

            _count
        };

        enum class Unit : uint8_t
        {
            SIDE,
            SLOT,
            QUANTITY,
            ATTACK,
            DEFENSE,
            SHOTS,
            DMG_MIN,
            DMG_MAX,
            HP,
            HP_LEFT,
            SPEED,
            VALUE_ONE,
            FLAGS1,
            FLAGS2,

            // RELATIVE values
            VALUE_REL, //             stack_value_now / global_value_now
            VALUE_REL0, //            stack_value_now / global_value_at_start
            VALUE_KILLED_REL, //      value_killed_this_turn / global_value_last_turn
            VALUE_KILLED_ACC_REL0, // value_killed_lifetime / global_value_at_start
            VALUE_LOST_REL, //        value_lost_this_turn / global_value_last_turn
            VALUE_LOST_ACC_REL0, //   value_lost_lifetime / global_value_at_start
            DMG_DEALT_REL, //         dmg_dealt_this_turn / global_hp_last_turn
            DMG_DEALT_ACC_REL0, //    dmg_dealt_lifetime / global_hp_at_start
            DMG_RECEIVED_REL, //      dmg_received_this_turn / global_hp_last_turn
            DMG_RECEIVED_ACC_REL0, // dmg_received_lifetime / global_hp_at_start

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
        BLANK_ENUM_DEF(Global_Yields_Player);
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
            ATTACK_ALLKILL_CHANCE,
            _count
        };

        enum class Unit_ShootDmg_Unit : uint8_t
        {
            ATTACK_DMG_MEAN_REL_OTHER,
            ATTACK_DMG_MEAN_REL_BF,
            ATTACK_DMG_STD_REL_OTHER,
            ATTACK_DMG_STD_REL_BF,
            ATTACK_VALUE_REL_BF,
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
        static_assert(static_cast<int>(ElementType::_count) == 5 + 19);
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
        ElementType::EDGE_GLOBAL_YIELDS_PLAYER,
        ElementType::EDGE_PLAYER_OWNS_UNIT,
        ElementType::EDGE_HEX_ADJACENT_HEX,
        ElementType::EDGE_UNIT_ACTS_BEFORE_UNIT,
        ElementType::EDGE_UNIT_MELEE_DMG_UNIT,
        ElementType::EDGE_UNIT_SHOOT_DMG_UNIT,
        ElementType::EDGE_UNIT_BLOCKS_UNIT,
        ElementType::EDGE_UNIT_OCCUPIES_HEX,
        ElementType::EDGE_ACTION_BY_UNIT,
        ElementType::EDGE_ACTION_BLOCKS_UNIT,
        ElementType::EDGE_ACTION_ENDS_AT_HEX,
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
