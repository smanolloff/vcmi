/*
 * types.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <vector>

#include "schema/base.h"
#include "./graph.h"
#include "encoding.h"

namespace MMAI::Schema::V15
{

enum class CombatResult : uint8_t
{
	LEFT_WINS,
	RIGHT_WINS,
	DRAW,
	NONE,

	_count
};

enum class StackActState : uint8_t
{
	READY, //   will act this turn, not waited
	WAITING, // will act this turn, already waited
	DONE, //    will not act this turn
	_count
};

enum class HexState : uint8_t
{
	PASSABLE, //      empty/mine/firewall/gate(open)/gate(closed,defender), ...
	STOPPING, //      moat/quicksand
	DAMAGING_L, //    moat/mine/firewall
	DAMAGING_R, //    moat/mine/firewall
	SIEGE_WALL,
	SIEGE_GATE,
	SIEGE_BRIDGE,
	OBSTACLE, //      permanent obstacles/indestructible walls/space between boats, ...
	_count
};

enum class HexAction : uint8_t
{
	AMOVE_TR, //  = Move to (*) + attack at hex 0..11:
	AMOVE_R, //    . . . . . . . . . 5 0 . . . .
	AMOVE_BR, //  . 1-hex:  . . . . 4 * 1 . . .
	AMOVE_BL, //   . . . . . . . . . 3 2 . . . .
	AMOVE_L, //   . . . . . . . . . . . . . . .
	AMOVE_TL, //   . . . . . . . . . 5 0 6 . . .
	AMOVE_2TR, // . 2-hex (R):  . . 4 * # 7 . .
	AMOVE_2R, //   . . . . . . . . . 3 2 8 . . .
	AMOVE_2BR, // . . . . . . . . . . . . . . .
	AMOVE_2BL, //  . . . . . . . .11 5 0 . . . .
	AMOVE_2L, //  . 2-hex (L):  .10 # * 1 . . .
	AMOVE_2TL, //  . . . . . . . . 9 3 2 . . . .
	MOVE, //      = Move to (defend if current hex)
	SHOOT, //     = shoot at
	_count
};

enum class GlobalAction : uint8_t
{
	RETREAT,
	WAIT,
	_count
};

enum class ActionType : uint8_t
{
	// RETREAT actions are not used by MMAI (they are only used through RESET)
	WAIT,
	DEFEND,
	MOVE,
	AMOVE,
	SHOOT,
	_count
};

// flags are split into two, as they can't fit in a single int after encoding
enum class StackFlag1 : uint8_t
{
	IS_ACTIVE,
	WILL_ACT,
	CAN_WAIT,
	CAN_RETALIATE,
	SLEEPING,
	// BLOCKED,
	// BLOCKING,
	IS_WIDE,
	FLYING,
	ADDITIONAL_ATTACK,
	NO_MELEE_PENALTY,
	TWO_HEX_ATTACK_BREATH,
	BLOCKS_RETALIATION,
	SHOOTER,
	NON_LIVING,
	WAR_MACHINE,
	FIREBALL,
	DEATH_CLOUD,
	THREE_HEADED_ATTACK,
	ALL_AROUND_ATTACK,
	RETURN_AFTER_STRIKE,
	ENEMY_DEFENCE_REDUCTION,
	LIFE_DRAIN,
	DOUBLE_DAMAGE_CHANCE,
	DEATH_STARE,

	_count
};

enum class StackFlag2 : uint8_t
{
	AGE,
	AGE_ATTACK,
	BIND,
	BIND_ATTACK,
	BLIND,
	BLIND_ATTACK,
	CURSE,
	CURSE_ATTACK,
	DISPEL_ATTACK,
	PETRIFY,
	PETRIFY_ATTACK,
	POISON,
	POISON_ATTACK,
	WEAKNESS,
	WEAKNESS_ATTACK,
	_count
};

using StackFlags1 = std::bitset<EI(StackFlag1::_count)>;
using StackFlags2 = std::bitset<EI(StackFlag2::_count)>;

enum class ErrorCode : uint8_t
{
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

enum class LinkType : uint8_t
{
	ADJACENT,
	REACH,
	RANGED_MOD,
	ACTS_BEFORE,
	MELEE_DMG_REL,
	RETAL_DMG_REL,
	RANGED_DMG_REL,
	_count
};

class IAttackLog
{
public:
	// NOTE: each of those can be "" if cstack was just resurrected/summoned
	virtual std::string getAttackerColor() const = 0;
	virtual std::string getAttackerAlias() const = 0;
	virtual std::string getDefenderColor() const = 0;
	virtual std::string getDefenderAlias() const = 0;

	virtual int getDamageDealt() const = 0;
	virtual int getDamageDealtPermille() const = 0;
	virtual int getUnitsKilled() const = 0;
	virtual int getValueKilled() const = 0;
	virtual int getValueKilledPermille() const = 0;
	virtual ~IAttackLog() = default;
};

using AttackLogs = std::vector<const IAttackLog *>;

class ILinks
{
public:
	virtual std::vector<int64_t> getSrcIndex() const = 0;
	virtual std::vector<int64_t> getDstIndex() const = 0;
	virtual std::vector<float> getAttributes() const = 0;
	virtual int getAttributeSize() const = 0;
	virtual ~ILinks() = default;
};

// This is returned as std::any by IState
// => MMAI_DLL_LINKAGE is needed to ensure std::any_cast sees the same symbol
class MMAI_DLL_LINKAGE ISupplementaryData
{
public:
	enum class Type : uint8_t
	{
		REGULAR,
		ANSI_RENDER
	};

	virtual Type getType() const = 0;
	virtual ErrorCode getErrorCode() const = 0;
	virtual const Graph::IGraph * getGraph() const = 0;
	virtual AttackLogs getAttackLogs() const = 0;
	virtual std::string getAnsiRender() const = 0;
	virtual ~ISupplementaryData() = default;
};
}
