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

namespace MMAI::Schema::V15::Graph
{
    namespace Nodes
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
            ACTION_MASK, //               mask for global actions (retreat, wait)

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

        enum class Action : uint8_t
        {
            ID // 0..2311
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
            QUEUE,
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
            ACTION_MASK,
            IS_REAR, // is this hex the rear hex of a stack
            IS_RUFR,
            WALL_HEALTH,

            _count
        };
    }

    enum class Edges : uint8_t
    {
        Action_EXPOSES_TO_Unit,
        Action_THREATENS_Unit,
        Action_DAMAGES_Unit,
        Action_ENDS_AT_Hex,
        Action_BY_Unit,
        Unit_BLOCKS_Unit,
        Unit_CAN_MELEE_Unit,
        Unit_CAN_SHOOT_Unit,
        Unit_ACTS_BEFORE_Unit,
        Unit_THREATENS_Unit,
        Unit_THREATENS_Hex, //    unit could attack occupants of hex
        Unit_OCCUPIES_Hex,
        Hex_ADJACENT_Hex, //      direction: 0..5

        _count
    };
} // namespace
