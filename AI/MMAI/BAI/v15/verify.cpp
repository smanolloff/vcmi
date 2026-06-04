#include "BAI/v15/graph/edges/action_ends_at_hex.h"
#include "BAI/v15/graph/edges/generic.h"
#include "BAI/v15/graph/edges/hex_adjacent_hex.h"
#include "BAI/v15/graph/edges/unit_acts_before_unit.h"
#include "BAI/v15/graph/nodes/action.h"
#include "CStack.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleAttackInfo.h"
#include "battle/BattleHex.h"
#include "battle/BattleSide.h"
#include "battle/CObstacleInstance.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/IBattleInfoCallback.h"
#include "battle/ReachabilityInfo.h"
#include "battle/Unit.h"
#include "constants/Enumerations.h"
#include "entities/building/TownFortifications.h"
#include "schema/base.h"
#include "vcmi/spells/Caster.h"

#include "verify.h"
#include "common.h"
#include "schema/v15/types.h"
#include <algorithm>
#include <vector>


namespace MMAI::BAI::V15
{
namespace S = Schema;
namespace S15 = S::V15;

namespace N = Graph::Nodes;
namespace E = Graph::Edges;

// using UnitPtr = std::shared_ptr<const N::Unit>;
// using HexPtr = std::shared_ptr<const N::Hex>;
// using ActionPtr = std::shared_ptr<const N::Action>;
// using TowerFlags = N::Global::TowerFlags;
// using CorpseFlags = N::Global::CorpseFlags;
// using ET = S15::Graph::ElementType;


namespace
{
	struct Context
	{
		const Graph::Graph & G;
		const CPlayerBattleCallback & battle;
		const CStack * astack;
		const std::unordered_set<const CStack *> allstacks;
		const std::unordered_map<int, const CStack *> bhexstacks;
		const std::bitset<GameConstants::BFIELD_SIZE> bhexcorpses;
		const std::map<const CStack *, ReachabilityInfo::TDistances> distances;
		const std::vector<const battle::Unit *> queue;
		const bool ended;
	};

	Context BuildContext(const State * state)
	{
		const auto & battle = state->battle;
		const auto & G = state->G;
		const CStack * astack = nullptr;
		const auto ended = state->supdata->ended;

		auto allstacks = std::unordered_set<const CStack *>{};
		auto bhexstacks = std::unordered_map<int, const CStack *>{};
		auto bhexcorpses = std::bitset<GameConstants::BFIELD_SIZE>{};
		auto distances = std::map<const CStack *, ReachabilityInfo::TDistances>{};

		for (const auto & cstack : battle.battleGetStacks(CBattleInfoEssentials::EStackOwnership::MINE_AND_ENEMY, false))
		{
			if (cstack->creatureId() == CreatureID::ARROW_TOWERS || cstack->creatureId() == CreatureID::CATAPULT)
				continue;

			if (!cstack->alive())
			{
				for(const auto & bh : cstack->getHexes())
					bhexcorpses.set(bh.toInt());
				continue;
			}

			allstacks.emplace(cstack);
			distances.emplace(cstack, battle.getReachability(cstack).distances);

			for(const auto & bh : cstack->getHexes())
				bhexstacks.emplace(bh.toInt(), cstack);

			if(cstack->unitId() == battle.battleActiveUnit()->unitId())
				astack = cstack;
		}

		// XXX: good morale is NOT handled here for simplicity
		//      See comments in Battlefield::GetQueue how to handle it.
		auto tmp = std::vector<battle::Units>{};
		battle.battleGetTurnOrder(tmp, S15::STACK_QUEUE_SIZE, 0);
		auto queue = std::vector<const battle::Unit *>{};
		for(const auto & units : tmp)
		{
			for(const auto & unit : units)
			{
				if(queue.size() < S15::STACK_QUEUE_SIZE)
					queue.push_back(unit);
				else
					break;
			}
		}

		return Context{
			.G=*G,
			.battle=battle,
			.astack=astack,
			.allstacks=allstacks,
			.bhexstacks=bhexstacks,
			.bhexcorpses=bhexcorpses,
			.distances=distances,
			.queue=queue,
			.ended=ended,
		};
	}

	// Plain message overload: expect(cond, "expectation failed");
	inline void expect(bool exp, std::string_view message)
	{
		if(exp)
			return;

		throw std::runtime_error(std::string(message));
	}

	template<typename... Args>
	inline void expect(bool exp, std::string_view format, const Args &... args)
	requires(sizeof...(Args) > 0)
	{
		if(exp)
			return;

		boost::format f{std::string(format)};

		// Fold expression: expands to (f % arg1, f % arg2, ...)
		((f % args), ...);

		throw std::runtime_error(f.str());
	}

	void vassert(int have, int want, const std::string_view attrname, const std::string & desc = "")
	{
		desc.empty() ? expect(have == want, "%s: have: %d, want: %d", attrname, have, want)
					 : expect(have == want, "%s: have: %d, want: %d (%s)", attrname, have, want, desc.c_str());
	};

	void Verify_NODE_GLOBAL(const Context & ctx)
	{
		const auto & nodes = ctx.G.getAll<N::Global>();

		expect(nodes.size() == 1, "GLOBAL: nodes.size() != 1");
		const auto & global = nodes.front();

		auto haswall = [&ctx](EWallPart wp)
		{
			auto wstate = ctx.battle.battleGetWallState(wp);
			return wstate == EWallState::INTACT || wstate == EWallState::REINFORCED;
		};

		using A = N::Global::A;
		for (int i = 0; i < EU(A::_count); ++i)
		{
			auto a = A(i);
			auto v = global->attr(a);

			switch (a)
			{
			case A::BATTLE_WINNER:
				switch (S15::CombatResult(v))
				{

				case S15::CombatResult::LEFT_WINS:
				case S15::CombatResult::RIGHT_WINS:
				case S15::CombatResult::DRAW:
					// XXX: The logic in battleIsFinished is flawed and returns no value
					//      (i.e. "not finished") when both sides have alive units.
					//      This is incorrect in case of a retreat => don't use it
					// vassert(v, battle.battleIsFinished(), "A::BATTLE_WINNER");
					expect(ctx.ended, "A::BATTLE_WINNER is " + std::to_string(v) + ", but ended is false");
					break;
				case S15::CombatResult::NONE:
				default:
					throw std::runtime_error("Unexpected CombatResult: " + std::to_string(v));
					break;
				}
				break;
			case A::BATTLE_ROUND:
				// XXX: technically, the first round for MMAI may not be the 1st VCMI round
				// It's highly unlikely though: all MMAI units must be blinded for the 1st round
				vassert(v, ctx.battle.battleGetRound(), "GLOBAL.BATTLE_ROUND");
				break;
			case A::HAS_UPPER_TOWER:
				vassert(v, haswall(EWallPart::UPPER_TOWER), "GLOBAL.HAS_UPPER_TOWER");
				break;
			case A::HAS_MIDDLE_TOWER:
				vassert(v, haswall(EWallPart::KEEP), "GLOBAL.HAS_MIDDLE_TOWER");
				break;
			case A::HAS_BOTTOM_TOWER:
				vassert(v, haswall(EWallPart::BOTTOM_TOWER), "GLOBAL.HAS_BOTTOM_TOWER");
				break;
			case A::HAS_GATE_CORPSE:
				vassert(v, ctx.bhexcorpses.test(BattleHex::GATE_INNER) || ctx.bhexcorpses.test(BattleHex::GATE_OUTER), "GLOBAL.HAS_GATE_CORPSE");
				break;
			case A::HAS_BRIDGE_CORPSE:
				vassert(v, ctx.bhexcorpses.test(BattleHex::GATE_BRIDGE), "GLOBAL.HAS_BRIDGE_CORPSE");
				break;
			default:
				throw std::runtime_error("Unexpected GLOBAL attr: " + std::to_string(EU(a)));
			}
		}
	}

	void Verify_NODE_PLAYER(const Context & ctx)
	{
		const auto & nodes = ctx.G.getAll<N::Player>();

		expect(nodes.size() == 2, "PLAYER: nodes.size() != 2");

		using A = N::Player::A;
		for (const auto & player : nodes)
		{
			for (int i = 0; i < EU(A::_count); ++i)
			{
				auto a = A(i);
				auto v = player->attr(a);

				static_assert(EU(S::Side::LEFT) == EU(BattleSide::LEFT_SIDE));
				static_assert(EU(S::Side::RIGHT) == EU(BattleSide::RIGHT_SIDE));
				const auto cplayer = ctx.battle.sideToPlayer(BattleSide(player->attr(A::BATTLE_SIDE)));

				switch (a)
				{
				case A::BATTLE_SIDE:
					break;
				case A::IS_ACTIVE:
					vassert(v, cplayer == ctx.astack->getOwner(), "PLAYER.IS_ACTIVE");
					break;
				case A::ARMY_VALUE_NOW_REL0:
				case A::ARMY_VALUE_NOW_REL:
				case A::ARMY_HP_NOW_REL:
				case A::VALUE_KILLED_NOW_REL:
				case A::VALUE_LOST_NOW_REL:
				case A::DMG_DEALT_NOW_REL:
				case A::DMG_RECEIVED_NOW_REL:
					// Not verifying those.
					break;
				default:
					throw std::runtime_error("Unexpected PLAYER attr: " + std::to_string(EU(a)));
				}
			}
		}
	}

	void Verify_NODE_UNIT(const Context & ctx)
	{
		const auto & nodes = ctx.G.getAll<N::Unit>();

		expect(nodes.size() == ctx.allstacks.size(), "UNIT: nodes.size() != " + std::to_string(ctx.allstacks.size()));

		auto duration = [](const CStack & cstack, BonusSource source, BonusSourceID sourceID)
		{
			auto cachingStr = "source_" + std::to_string(static_cast<int>(source));
			auto bonuses = cstack.getBonuses(Selector::source(source, sourceID), cachingStr);
			return bonuses->empty() ? 0 : bonuses->front()->turnsRemain;
		};

		using A = N::Unit::A;
		for (const auto & unit : nodes)
		{
			for (int i = 0; i < EU(A::_count); ++i)
			{
				auto a = A(i);
				auto v = unit->attr(a);

				const auto & cstack = unit->cstack;

				switch (a)
				{
				case A::VALUE_REL:
					// not verifying
					break;
				case A::SHOTS:
					vassert(v, cstack.shots.available(), "UNIT.SHOTS", cstack.getDescription());
					break;
				case A::DMG_UNCERTAINTY:
					// not verifying
					break;
				case A::IS_ACTIVE:
					vassert(v, cstack.unitId() == ctx.battle.battleActiveUnit()->unitId(), "UNIT.IS_ACTIVE", cstack.getDescription());
					break;
				case A::IS_ENEMY:
					vassert(v, cstack.unitSide() != ctx.battle.battleGetMySide(), "UNIT.IS_ENEMY", cstack.getDescription());
					break;
				case A::IS_SLEEPING:
					cstack.creatureId() == CreatureID::AMMO_CART
						? vassert(v, false, "UNIT.IS_SLEEPING")
						: vassert(v, cstack.hasBonusOfType(BonusType::NOT_ACTIVE), "UNIT.IS_SLEEPING");
					break;
				case A::IS_WAR_MACHINE:
					vassert(v, cstack.hasBonusOfType(BonusType::SIEGE_WEAPON), "UNIT.IS_WAR_MACHINE", cstack.getDescription());
					break;
				case A::HAS_ADDITIONAL_ATTACK:
					vassert(v, cstack.hasBonusOfType(BonusType::ADDITIONAL_ATTACK), "UNIT.HAS_ADDITIONAL_ATTACK", cstack.getDescription());
					break;
				case A::HAS_ALL_AROUND_ATTACK:
					vassert(v, cstack.hasBonusOfType(BonusType::ATTACKS_ALL_ADJACENT), "UNIT.HAS_ALL_AROUND_ATTACK", cstack.getDescription());
					break;
				case A::HAS_BLOCKS_RETALIATION:
					vassert(v, cstack.hasBonusOfType(BonusType::BLOCKS_RETALIATION), "UNIT.HAS_BLOCKS_RETALIATION", cstack.getDescription());
					break;
				case A::HAS_DEATH_CLOUD:
					vassert(v, cstack.hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::DEATH_CLOUD)), "UNIT.HAS_DEATH_CLOUD", cstack.getDescription());
					break;
				case A::HAS_DOUBLE_DAMAGE_CHANCE:
					vassert(v, 10 * cstack.valOfBonuses(BonusType::DOUBLE_DAMAGE_CHANCE), "UNIT.HAS_DOUBLE_DAMAGE_CHANCE", cstack.getDescription());
					break;
				case A::HAS_FIREBALL:
					vassert(v, cstack.hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::FIREBALL)), "UNIT.HAS_FIREBALL", cstack.getDescription());
					break;
				case A::HAS_FLYING:
					vassert(v, cstack.hasBonusOfType(BonusType::FLYING), "UNIT.HAS_FLYING", cstack.getDescription());
					break;
				case A::HAS_LIFE_DRAIN:
					vassert(v, 10 * cstack.hasBonusOfType(BonusType::LIFE_DRAIN), "UNIT.HAS_FLYING", cstack.getDescription());
					break;
				case A::HAS_NON_LIVING:
				{
					auto undead = cstack.hasBonusOfType(BonusType::UNDEAD);
					auto nonliving = cstack.hasBonusOfType(BonusType::NON_LIVING);
					vassert(v, undead || nonliving, "UNIT.HAS_NON_LIVING", cstack.getDescription());
					break;
				}
				case A::HAS_NO_MELEE_PENALTY:
					vassert(v, cstack.hasBonusOfType(BonusType::NO_MELEE_PENALTY), "UNIT.HAS_NO_MELEE_PENALTY", cstack.getDescription());
					break;
				case A::HAS_RETURN_AFTER_STRIKE:
					vassert(v, cstack.hasBonusOfType(BonusType::RETURN_AFTER_STRIKE), "UNIT.HAS_RETURN_AFTER_STRIKE", cstack.getDescription());
					break;
				case A::HAS_THREE_HEADED_ATTACK:
					vassert(v, cstack.hasBonusOfType(BonusType::THREE_HEADED_ATTACK), "UNIT.HAS_THREE_HEADED_ATTACK", cstack.getDescription());
					break;
				case A::HAS_TWO_HEX_ATTACK_BREATH:
					vassert(v, cstack.hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "UNIT.HAS_TWO_HEX_ATTACK_BREATH", cstack.getDescription());
					break;
				case A::HAS_AGE:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::AGE)), "UNIT.HAS_AGE", cstack.getDescription());
					break;
				case A::HAS_AGE_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::AGE)), "UNIT.HAS_AGE_ATTACK", cstack.getDescription());
					break;
				case A::HAS_BIND:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::BIND)), "UNIT.HAS_BIND", cstack.getDescription());
					break;
				case A::HAS_BIND_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BIND)), "UNIT.HAS_BIND_ATTACK", cstack.getDescription());
					break;
				case A::HAS_BLIND:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::BLIND)), "UNIT.HAS_BIND", cstack.getDescription());
					break;
				case A::HAS_BLIND_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BLIND)), "UNIT.HAS_BLIND_ATTACK", cstack.getDescription());
					break;
				case A::HAS_CURSE:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::CURSE)), "UNIT.HAS_CURSE", cstack.getDescription());
					break;
				case A::HAS_CURSE_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::CURSE)), "UNIT.HAS_CURSE_ATTACK", cstack.getDescription());
					break;
				case A::HAS_DISPEL_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::DISPEL_HELPFUL_SPELLS)), "UNIT.HAS_DISPEL_ATTACK", cstack.getDescription());
					break;
				case A::HAS_PETRIFY:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::STONE_GAZE)), "UNIT.HAS_PETRIFY", cstack.getDescription());
					break;
				case A::HAS_PETRIFY_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::STONE_GAZE)), "UNIT.HAS_PETRIFY_ATTACK", cstack.getDescription());
					break;
				case A::HAS_POISON:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::POISON)), "UNIT.HAS_POISON", cstack.getDescription());
					break;
				case A::HAS_POISON_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::POISON)), "UNIT.HAS_POISON_ATTACK", cstack.getDescription());
					break;
				case A::HAS_WEAKNESS:
					vassert(v, duration(cstack, BonusSource::SPELL_EFFECT, SpellID(SpellID::WEAKNESS)), "UNIT.HAS_WEAKNESS", cstack.getDescription());
					break;
				case A::HAS_WEAKNESS_ATTACK:
					vassert(v, cstack.valOfBonuses(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::WEAKNESS)), "UNIT.HAS_WEAKNESS_ATTACK", cstack.getDescription());
					break;
				default:
					throw std::runtime_error("Unexpected UNIT attr: " + std::to_string(EU(a)));
				}
			}
		}
	}

	void Verify_NODE_HEX(const Context & ctx)
	{
		const auto & nodes = ctx.G.getAll<N::Hex>();

		expect(nodes.size() == 165, "HEX: nodes.size() != 165");

		const auto obstacles = ctx.battle.battleGetAllObstacles();
		auto anyobstacle = [&obstacles](auto fn)
		{
			return std::any_of(obstacles.begin(), obstacles.end(), fn);
		};

		using A = N::Hex::A;
		for (const auto & hex : nodes)
		{
			for (int i = 0; i < EU(A::_count); ++i)
			{
				auto a = A(i);
				auto v = hex->attr(a);

				const auto & bhex = hex->bhex;
				expect(bhex.isAvailable(), "HEX: unavailable");

				const auto access = ctx.battle.getAccessibility().at(bhex.toInt());

				switch (a)
				{
				case A::Y_COORD:
					vassert(v, N::Hex::CalcXY(bhex).second, "HEX.Y_COORD", hex->name());
					break;
				case A::X_COORD:
					vassert(v, N::Hex::CalcXY(bhex).first, "HEX.X_COORD", hex->name());
					break;
				case A::IS_PASSABLE:
					vassert(v, access == EAccessibility::ACCESSIBLE, "HEX.X_COORD", hex->name());
					break;
				case A::IS_STOPPING:
				{
					auto stopping = anyobstacle(std::mem_fn(&CObstacleInstance::stopsMovement));
					vassert(v, stopping, "HEX.IS_STOPPING: no obstacle stops movement", hex->name());
					break;
				}
				case A::IS_DAMAGING_L:
				{
					auto ldmg = anyobstacle([](const std::shared_ptr<const CObstacleInstance> & o)
					{
						if(o->obstacleType == CObstacleInstance::MOAT)
							return true;
						if(!o->triggersEffects())
							return false;
						auto s = SpellID(o->ID);
						if(s == SpellID::FIRE_WALL)
							return true;
						if(s != SpellID::LAND_MINE)
							return false;
						ASSERT(o->obstacleType == CObstacleInstance::EObstacleType::SPELL_CREATED, "expected spell created obstacle");
						const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o.get());
						return so->casterSide != BattleSide::LEFT_SIDE;
					});
					vassert(v, ldmg, "HEX.IS_DAMAGING_L: no obstacle triggers a damaging effect", hex->name());
					break;
				}
				case A::IS_DAMAGING_R:
				{
					auto rdmg = anyobstacle([](const std::shared_ptr<const CObstacleInstance> & o)
					{
						if(o->obstacleType == CObstacleInstance::MOAT)
							return true;
						if(!o->triggersEffects())
							return false;
						auto s = SpellID(o->ID);
						if(s == SpellID::FIRE_WALL)
							return true;
						if(s != SpellID::LAND_MINE)
							return false;
						ASSERT(o->obstacleType == CObstacleInstance::EObstacleType::SPELL_CREATED, "expected spell created obstacle");
						const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o.get());
						return so->casterSide != BattleSide::RIGHT_SIDE;
					});
					vassert(v, rdmg, "HEX.IS_DAMAGING_L: no obstacle triggers a damaging effect", hex->name());
					break;
				}
				case A::IS_SIEGE_GATE:
					if (ctx.battle.battleGetFortifications().wallsHealth > 0)
						vassert(v, bhex == BattleHex::GATE_OUTER || bhex == BattleHex::GATE_INNER, "HEX.IS_SIEGE_GATE: not a gate hex", hex->name());
					else
						vassert(v, 0, "HEX.IS_SIEGE_GATE: no fort on this battlefield", hex->name());
					break;
				case A::IS_SIEGE_BRIDGE:
					if (ctx.battle.battleGetFortifications().wallsHealth > 0)
						vassert(v, bhex == BattleHex::GATE_BRIDGE, "HEX.IS_SIEGE_BRIDGE: no a bridge hex", hex->name());
					else
						vassert(v, 0, "HEX.IS_SIEGE_BRIDGE: no fort on this battlefield", hex->name());
					break;
				case A::IS_OBSTACLE:
				{
					bool want = anyobstacle([&bhex](auto & o) { return o->getBlockedTiles().contains(bhex); });
					vassert(v, want, "HEX.IS_OBSTACLE: no obstacle blocks this hex", hex->name());
					break;
				}
				case A::WALL_HEALTH:
					static_assert(0 == EU(EWallState::DESTROYED));
					static_assert(1 == EU(EWallState::DAMAGED));
					static_assert(2 == EU(EWallState::INTACT));
					static_assert(3 == EU(EWallState::REINFORCED));
					if (ctx.battle.battleHexToWallPart(bhex) == EWallPart::INVALID)
						vassert(v, 0, "Hex.WALL_HEALTH: not a wall hex", hex->name());
					else
						vassert(v, EU(ctx.battle.battleGetWallState(ctx.battle.battleHexToWallPart(bhex))), "HEX.WALL_HEALTH: state mismatch", hex->name());
					break;
				default:
					throw std::runtime_error("Unexpected HEX attr: " + std::to_string(EU(a)));
				}
			}
		}
	}

	void Verify_NODE_ACTION(const Context & ctx)
	{
		for (const auto & id : ctx.G.getActiveActionIds())
			expect(ctx.G.getById<N::Action>(id, false) != nullptr, "active action not active: " + std::to_string(id));

		auto isReachable = [&ctx](const CStack & stack, const BattleHex & bh)
		{
			auto distance = ctx.distances.at(&stack).at(bh.toInt());
			return distance <= stack.getMovementRange();
		};

		using A = N::Action::A;
		for (const auto & action : ctx.G.getAll<N::Action>())
		{
			using AT = S15::ActionType;

			const CStack & actor = action->by->cstack;
			const auto endBhex = action->endsAt.at(0)->bhex;

			// NOTE: the method name can *move* is misleading.
			// It is actually checks if stack can *act*:
			expect(actor.canMove(), "ACTION: cannot act");

			for (int i = 0; i < EU(A::_count); ++i)
			{
				auto a = A(i);
				auto v = action->attr(a);

				switch(a)
				{
				case A::ACTION_TYPE:
					switch(AT(v))
					{
					// case AT::RETREAT:
					//     expect(actor == nullptr, "ACTION.ACTION_TYPE[RETREAT]: actor must be nullptr");
					//     break;
					case AT::WAIT:
						expect(actor.waited(), "ACTION.ACTION_TYPE[WAIT]: already waited");
						break;
					case AT::DEFEND:
						// nothing to check (always possible action)
						break;
					case AT::MOVE:
						expect(isReachable(actor, endBhex), "ACTION.ACTION_TYPE[MOVE]: endBhex unreachable");
						break;
					case AT::AMOVE:
						expect(isReachable(actor, endBhex), "ACTION.ACTION_TYPE[AMOVE]: endBhex unreachable");
						expect(action->target != nullptr, "ACTION.ACTION_TYPE[AMOVE]: target is nullptr");
						expect(CStack::isMeleeAttackPossible(&actor, &action->target->cstack, endBhex), "ACTION.ACTION_TYPE[AMOVE]: melee attack is impossible");
						break;
					case AT::SHOOT:
						expect(actor.canShoot(), "ACTION.ACTION_TYPE[SHOOT]: can't shoot");
						expect(!ctx.battle.battleIsUnitBlocked(&actor)
							|| actor.hasBonusOfType(BonusType::FREE_SHOOTING)
							|| actor.hasBonusOfType(BonusType::SIEGE_WEAPON), "ACTION.ACTION_TYPE[SHOOT]: blocked");
						break;
					default:
						throw std::runtime_error("Unexpected Action type: " + std::to_string(v));
						break;
					}
				case A::IS_ACTIVE:
					vassert(v, &actor == ctx.astack, "ACTION.ACTION_TYPE[IS_ACTIVE]: not active");
					break;
				default:
					throw std::runtime_error("Unexpected Action attr: " + std::to_string(EU(a)));
					break;
				}
			}
		}
	}


	void Verify_EDGE_UNIT_ACTS_BEFORE_UNIT(const Context & ctx)
	{
		auto countActsBefore = [&ctx](const battle::Unit * src, const battle::Unit * dst) {
			const auto dstIt = std::ranges::find(ctx.queue, dst);
			return std::ranges::count_if(ctx.queue.begin(), dstIt, [src](const battle::Unit * unit) {
				return unit == src;
			});
		};

		for(const auto & edge : ctx.G.getAll<E::Unit_ActsBefore_Unit>())
		{
			const auto * src = &edge->srcNode->cstack;
			const auto * dst = &edge->dstNode->cstack;
			expect(edge->attr(E::Unit_ActsBefore_Unit::A::TIMES) == countActsBefore(src, dst), "EDGE_UNIT_ACTS_BEFORE_UNIT.TIMES mismatch");
		}
	}

	void Verify_EDGE_UNIT_BLOCKS_UNIT(const Context & ctx)
	{
		for(const auto & edge : ctx.G.getAll<E::Unit_Blocks_Unit>())
		{
			const auto & blocked = edge->srcNode->cstack;
			const auto & blocker = edge->dstNode->cstack;

			expect(blocked.canShoot(), "EDGE_UNIT_BLOCKS_UNIT: blocked unit cannot shoot");
			expect(!blocked.canShootBlocked(), "EDGE_UNIT_BLOCKS_UNIT: blocked unit can shoot while blocked");
			expect(!blocked.hasBonusOfType(BonusType::SIEGE_WEAPON), "EDGE_UNIT_BLOCKS_UNIT: war machine cannot be blocked");
			expect(blocker.unitSide() != blocked.unitSide() || blocked.hasBonusOfType(BonusType::ATTACKS_NEAREST_CREATURE), "EDGE_UNIT_BLOCKS_UNIT: friendly unit blocks non-berserk shooter");

			const auto surroundingHexes = blocked.getSurroundingHexes();
			expect(std::ranges::any_of(surroundingHexes, [&blocker](const BattleHex & bhex) {
				return blocker.coversPos(bhex);
			}), "EDGE_UNIT_BLOCKS_UNIT: blocker is not adjacent to blocked unit");
		}
	}

	void Verify_EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION(const Context & ctx)
	{
		for (const auto & edge : ctx.G.getAll<E::Unit_BecomesMeleeThreatAfter_Action>())
		{
			const auto & unit = edge->srcNode;
			const auto & actor = edge->dstNode->by;
			// NOTE: this will fail if either stack is berserk
			expect(unit->cstack.unitSide() != actor->cstack.unitSide(), "EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION: same side");

			const auto & dstHex = edge->dstNode->endsAt.at(0);
			const auto speed = unit->cstack.getMovementRange();
			const auto & reachability = ctx.battle.getReachability(&unit->cstack);
			const auto & nbhexes = actor->cstack.getSurroundingHexes(dstHex->bhex);
			// XXX: if attacker is wide it may be unable to reach a nbhex, yet still occupy it and attack
			bool threat = std::ranges::any_of(nbhexes, [&speed, &reachability](const BattleHex & bh) {
				return reachability.distances.at(bh.toInt()) <= speed;
			});

			expect(threat, "EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION: unit cannot reach any hex adjacent to action destination");
		}
	}

	void Verify_EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION(const Context & ctx)
	{
		for (const auto & edge : ctx.G.getAll<E::Unit_BecomesShootThreatAfter_Action>())
		{
			const auto & stack = edge->srcNode->cstack;
			expect(stack.canShoot(), "EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION: unit cannot shoot");
			expect(!stack.canShootBlocked(), "EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION: unit can shoot while blocked");
			expect(!stack.hasBonusOfType(BonusType::SIEGE_WEAPON), "EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION: war machine cannot be blocked");

			if (stack-)
			const auto & unit = edge->srcNode;
			const auto & actor = edge->dstNode->by->cstack;
			const auto & dstHex = edge->dstNode->endsAt.at(0);
			const auto speed = unit->cstack.getMovementRange();
			const auto & reachability = ctx.battle.getReachability(ReachabilityInfo::Parameters(&unit->cstack, dstHex->bhex));
			const auto & nbhexes = actor.getSurroundingHexes(dstHex->bhex);
			bool threat = std::ranges::any_of(nbhexes, [&speed, &reachability](const BattleHex & bh) {
				return reachability.distances.at(bh.toInt()) <= speed;
			});

			expect(threat, "EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION: unit cannot reach any hex adjacent to action destination");
		}
	}
}

// This function used during model development and is never called otherwise
void Verify(const State * state) // NOSONAR - function used for debugging only
{

	const auto ctx = BuildContext(state);

	expect(ctx.astack || ctx.ended, "astack is NULL, but ended is not true");

	for (int i = 0; i < EU(S15::Graph::ElementType::_count); ++i)
	{
		using ET = S15::Graph::ElementType;
		const auto et = ET(i);

		switch(et)
		{
		case ET::NODE_GLOBAL:
			Verify_NODE_GLOBAL(ctx);
			break;
		case ET::NODE_PLAYER:
			Verify_NODE_PLAYER(ctx);
			break;
		case ET::NODE_UNIT:
			Verify_NODE_UNIT(ctx);
			break;
		case ET::NODE_HEX:
			Verify_NODE_HEX(ctx);
			break;
		case ET::NODE_ACTION:
			Verify_NODE_ACTION(ctx);
			break;
		case ET::EDGE_GLOBAL_TO_PLAYER:
		case ET::EDGE_PLAYER_TO_GLOBAL:
		case ET::EDGE_GLOBAL_TO_UNIT:
		case ET::EDGE_UNIT_TO_GLOBAL:
		case ET::EDGE_GLOBAL_TO_HEX:
		case ET::EDGE_HEX_TO_GLOBAL:
		case ET::EDGE_GLOBAL_TO_ACTION:
		case ET::EDGE_PLAYER_OWNS_UNIT:
			// nothing special about these
			break;
		case ET::EDGE_UNIT_OWNED_BY_PLAYER:
			for (const auto & edge : ctx.G.getAll<E::Unit_OwnedBy_Player>())
				expect(edge->srcNode->cstack.getOwner() == ctx.battle.sideToPlayer(edge->dstNode->side), "owner mismatch");
			break;
		case ET::EDGE_UNIT_OCCUPIES_HEX:
			for (const auto & edge : ctx.G.getAll<E::Unit_Occupies_Hex>())
				expect(edge->srcNode->cstack.getHexes().contains(edge->dstNode->bhex), "stack does not occupy bhex");
			break;
		case ET::EDGE_HEX_OCCUPIED_BY_UNIT:
			for (const auto & edge : ctx.G.getAll<E::Hex_OccupiedBy_Unit>())
				expect(edge->dstNode->cstack.getHexes().contains(edge->srcNode->bhex), "stack does not occupy bhex");
			break;
		case ET::EDGE_ACTION_BY_UNIT:
			// Already checked that action->actor can perform the specific action
			// Here, check only that action->actor corresponds to this edge
			for (const auto & edge : ctx.G.getAll<E::Action_By_Unit>())
				expect(edge->dstNode == edge->srcNode->by, "EDGE_ACTION_BY_UNIT: actor mismatch");
			break;
		case ET::EDGE_UNIT_HAS_ACTION:
			for (const auto & edge : ctx.G.getAll<E::Unit_Has_Action>())
				expect(edge->srcNode == edge->dstNode->by, "EDGE_UNIT_HAS_ACTION: actor mismatch");
			break;
		case ET::EDGE_HEX_ADJACENT_HEX:
			for (const auto & edge : ctx.G.getAll<E::Hex_Adjacent_Hex>())
				expect(edge->attr(E::Hex_Adjacent_Hex::A::DIRECTION) == EU(BattleHex::mutualPosition(edge->srcNode->bhex, edge->dstNode->bhex)), "EDGE_HEX_ADJACENT_HEX: direction mismatch");
			break;
		case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
			Verify_EDGE_UNIT_ACTS_BEFORE_UNIT(ctx);
			break;
		case ET::EDGE_UNIT_MELEE_DMG_UNIT:
		case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
			// too complex
			break;
		case ET::EDGE_UNIT_BLOCKS_UNIT:
			Verify_EDGE_UNIT_BLOCKS_UNIT(ctx);
			break;
		case ET::EDGE_ACTION_ENDS_AT_HEX:
		case ET::EDGE_HEX_IS_END_OF_ACTION:
			// Nothing to check
		case ET::EDGE_ACTION_BLOCKS_UNIT:
			// Already checked (EDGE_UNIT_BLOCKS_UNIT)
			break;
		case ET::EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION:
			Verify_EDGE_UNIT_BECOMES_MELEE_THREAT_AFTER_ACTION(ctx);
			break;
		case ET::EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION:
			Verify_EDGE_UNIT_BECOMES_SHOOT_THREAT_AFTER_ACTION(ctx);
			break;
		case ET::EDGE_UNIT_IS_MELEED_BY_ACTION:
		case ET::EDGE_UNIT_IS_SHOT_BY_ACTION:
		case ET::EDGE_UNIT_BECOMES_MELEE_TARGET_AFTER_ACTION:
		case ET::EDGE_UNIT_BECOMES_SHOOT_TARGET_AFTER_ACTION:
		case ET::EDGE_HEX_BECOMES_MELEE_TARGET_AFTER_ACTION:
		case ET::EDGE_HEX_BECOMES_SHOOT_TARGET_AFTER_ACTION:
		default:
			throw std::runtime_error("Unexpected ElementType: " + std::to_string(i));
			break;
		}
	}
}

//     if(ended)
//     {
//         static_assert(EU(Side::LEFT) == EU(BattleSide::ATTACKER));
//         static_assert(EU(Side::RIGHT) == EU(BattleSide::DEFENDER));

//         auto fin = battle.battleIsFinished();

//         // XXX: The logic in battleIsFinished is flawed and returns no value
//         //      (i.e. "not finished") if both sides have units, which can
//         //      happen if the WE some has retreated as a regular action (not via reset).
//         // ASSERT(fin.has_value(), "ended, but battleIsFinished returns no value?");

//         if(fin.has_value())
//         {
//             // NONE means draw (no units on battlefield) -- our value will be null in this case
//             (fin == BattleSide::NONE) ? vassert(global->attr(GA::BATTLE_WINNER), S15::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (draw)")
//                                       : vassert(global->attr(GA::BATTLE_WINNER), EU(fin.value()), "GA.BATTLE_WINNER");
//         }
//         else
//         {
//             // we have retreated *as an action*
//             // There seems to be no way to ask vcmi "which side retreated"
//         }

//         auto activeside = EU(battle.battleGetMySide() == BattleSide::ATTACKER ? Side::LEFT : Side::RIGHT);
//         vassert(global->attr(GA::BATTLE_SIDE_ACTIVE_PLAYER), activeside, "GA.BATTLE_SIDE_ACTIVE_PLAYER");
//     }
//     else
//     {
//         static_assert(EU(Side::LEFT) == EU(BattleSide::LEFT_SIDE));
//         static_assert(EU(Side::RIGHT) == EU(BattleSide::RIGHT_SIDE));
//         ASSERT(astack != nullptr, "not ended, but no astack either");
//         vassert(global->attr(GA::BATTLE_WINNER), S15::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (battle ongoing)");
//         vassert(global->attr(GA::BATTLE_SIDE_ACTIVE_PLAYER), EU(astack->unitSide()), "GA.BATTLE_SIDE_ACTIVE_PLAYER");
//     }
//     auto alogs = state->supdata->getAttackLogs();

//     {
//         auto gatecorpse = false;
//         auto bridgecorpse = false;

//         if(battle.battleGetFortifications().wallsHealth > 0) {
//             for(const CStack * cstack : battle.battleGetAllStacks(false)) {
//                 if(cstack->alive())
//                     continue;

//                 if(cstack->coversPos(BattleHex::GATE_INNER) || cstack->coversPos(BattleHex::GATE_OUTER))
//                     gatecorpse = true;

//                 if(cstack->coversPos(BattleHex::GATE_BRIDGE))
//                     bridgecorpse = true;
//             }
//         }

//         auto corpsemask = CorpseFlags(global->attr(GA::SIEGE_CORPSES));
//         if(gatecorpse)
//             vassert(corpsemask.test(0), 1, "GA.SIEGE_CORPSES<0>");
//         if(bridgecorpse)
//             vassert(corpsemask.test(1), 1, "GA.SIEGE_CORPSES<1>");
//     }

//     {
//         auto f = battle.battleGetFortifications();
//         auto towermask = TowerFlags(global->attr(GA::SIEGE_TOWERS));
//         if(f.upperTowerHealth > 0 && battle.battleGetWallState(EWallPart::UPPER_TOWER) != EWallState::DESTROYED)
//             vassert(towermask.test(0), 1, "GA.SIEGE_TOWERS<0>");
//         if(f.citadelHealth > 0 && battle.battleGetWallState(EWallPart::KEEP) != EWallState::DESTROYED)
//             vassert(towermask.test(1), 1, "GA.SIEGE_TOWERS<1>");
//         if(f.lowerTowerHealth > 0 && battle.battleGetWallState(EWallPart::BOTTOM_TOWER) != EWallState::DESTROYED)
//             vassert(towermask.test(2), 1, "GA.SIEGE_TOWERS<2>");
//     }

//     for(int ihex = 0; ihex < 165; ihex++)
//     {
//         int x = ihex % 15;
//         int y = ihex / 15;
//         const auto & hex = G.getById<N::Hex>(ihex);
//         const auto & bh = hex->bhex;
//         expect(bh == BattleHex(x + 1, y), "hex->bhex mismatch");

//         auto ainfo = battle.getAccessibility();
//         auto aa = ainfo.at(bh.toInt());

//         for(int i = 0; i < EU(HA::_count); i++)
//         {
//             auto attr = static_cast<HA>(i);
//             auto v = hex->attrs.at(i);
//             const auto * cstack = ctx.bhexstacks.at(ihex);
//             const auto occupants = G.getAllEdgesSrcByDst<E::Unit_Occupies_Hex>(hex);

//             if(cstack)
//             {
//                 expect(std::ranges::distance(occupants) == 1, "expected 1 occupant, got: %d", std::ranges::distance(occupants));
//                 expect(&occupants.at(0)->cstack == cstack, "occupant cstack mismatch");
//             }
//             else
//             {
//                 expect(occupants.empty(), "cstack is nullptr, occupants are present");
//             }

//             switch(attr)
//             {
//                 case HA::Y_COORD:
//                     expect(v == y, "HEX.Y_COORD: %d != %d", v, y);
//                     break;
//                 case HA::X_COORD:
//                     expect(v == x, "HEX.X_COORD: %d != %d", v, x);
//                     break;
//                 case HA::STATE_MASK:
//                 {
//                     auto obstacles = battle.battleGetAllObstaclesOnPos(bh, false);
//                     auto anyobstacle = [&obstacles](auto fn)
//                     {
//                         return std::any_of(
//                             obstacles.begin(),
//                             obstacles.end(),
//                             [&fn](const std::shared_ptr<const CObstacleInstance> & obstacle)
//                             {
//                                 return fn(obstacle.get());
//                             }
//                         );
//                     };

//                     auto mask = S15::HexStateMask(v);
//                     BattleSide side = astack ? astack->unitSide() : BattleSide::ATTACKER; // XXX: Hex defaults to 0 if there is no astack

//                     if(mask.test(EU(S15::HexState::PASSABLE)))
//                     {
//                         expect(
//                             aa == EAccessibility::ACCESSIBLE || (EU(side) && aa == EAccessibility::GATE),
//                             "HEX.STATE_MASK: PASSABLE bit is set, but accessibility is %d (side: %d)",
//                             EU(aa),
//                             EU(side)
//                         );
//                     }
//                     else
//                     {
//                         if(aa == EAccessibility::OBSTACLE || aa == EAccessibility::ALIVE_STACK)
//                             break;

//                         switch(aa)
//                         {
//                             case EAccessibility::ACCESSIBLE:
//                                 throw std::runtime_error("HEX.STATE_MASK: PASSABLE bit not set, but accessibility is ACCESSIBLE");
//                                 break;
//                             case EAccessibility::ALIVE_STACK:
//                             case EAccessibility::OBSTACLE:
//                             case EAccessibility::DESTRUCTIBLE_WALL:
//                             case EAccessibility::GATE:
//                                 break;
//                             case EAccessibility::UNAVAILABLE:
//                                 // only Fort and Boat battles can have unavailable hexes
//                                 expect(
//                                     battle.battleGetFortifications().wallsHealth > 0 || battle.battleTerrainType() == TerrainId::WATER,
//                                     "Found UNAVAILABLE accessibility on non-fort, non-boat battlefield: tertype=%d",
//                                     EU(battle.battleTerrainType())
//                                 );
//                                 break;
//                             case EAccessibility::SIDE_COLUMN:
//                                 // side hexes should are not included in the observation
//                                 throw std::runtime_error("HEX.STATE_MASK: SIDE_COLUMN accessibility found");
//                                 break;
//                             default:
//                                 throw std::runtime_error("Unexpected accessibility: " + std::to_string(EU(aa)));
//                         }
//                     }

//                     if(mask.test(EU(S15::HexState::STOPPING)))
//                     {
//                         auto stopping = anyobstacle(std::mem_fn(&CObstacleInstance::stopsMovement));
//                         expect(stopping, "HEX.STATE_MASK: STOPPING bit is set, but no obstacle stops movement");
//                     }

//                     if(mask.test(EU(S15::HexState::DAMAGING_L)))
//                     {
//                         auto damaging = anyobstacle(
//                             [side](const CObstacleInstance * o)
//                             {
//                                 if(o->obstacleType == CObstacleInstance::MOAT)
//                                     return true;
//                                 if(!o->triggersEffects())
//                                     return false;
//                                 auto s = SpellID(o->ID);
//                                 if(s == SpellID::FIRE_WALL)
//                                     return true;
//                                 if(s != SpellID::LAND_MINE)
//                                     return false;
//                                 const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
//                                 auto bside = static_cast<bool>(side);
//                                 return (side == so->casterSide) ? bside : !bside;
//                             }
//                         );
//                         expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
//                     }

//                     if(mask.test(EU(S15::HexState::DAMAGING_R)))
//                     {
//                         auto damaging = anyobstacle(
//                             [side](const CObstacleInstance * o)
//                             {
//                                 if(o->obstacleType == CObstacleInstance::MOAT)
//                                     return true;
//                                 if(!o->triggersEffects())
//                                     return false;
//                                 auto s = SpellID(o->ID);
//                                 if(s == SpellID::FIRE_WALL)
//                                     return true;
//                                 if(s != SpellID::LAND_MINE)
//                                     return false;
//                                 const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
//                                 auto bside = static_cast<bool>(side);
//                                 return (side == so->casterSide) ? !bside : bside;
//                             }
//                         );
//                         expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
//                     }
//                 }
//                 break;
//                 case HA::WALL_HEALTH:
//                 {
//                     auto wp = battle.battleHexToWallPart(hex->bhex);
//                     if(wp != EWallPart::BOTTOM_WALL && wp != EWallPart::BELOW_GATE && wp != EWallPart::GATE && wp != EWallPart::OVER_GATE && wp != EWallPart::UPPER_WALL)
//                         return;
//                     auto ws = battle.battleGetWallState(wp);
//                     if(ws == EWallState::NONE)
//                         vassert(v, EU(S15::WallHP::HP0), "HEX.WALL_HEALTH");
//                     else
//                         vassert(v, EU(ws), "HEX.WALL_HEALTH");
//                 }
//                 break;
//                 default:
//                     THROW_FORMAT("Unexpected HexAttribute: %d", EU(attr));
//             }
//         }
//     }

//     for(const auto & unit : G.getAll<N::Unit>())
//     {
//         const auto & stack = unit->cstack;

//         for(int i = 0; i < EU(UA::_count); i++)
//         {
//             auto attr = static_cast<UA>(i);
//             auto v = unit->attr(attr);

//             bool isShooter = G.getOneEdgeBySrc<E::Unit_ShootDmg_Unit>(unit, false) != nullptr;

//             switch(attr)
//             {
//                 case UA::SIDE:
//                     static_assert(EU(S::Side::LEFT) == EU(BattleSide::LEFT_SIDE));
//                     static_assert(EU(S::Side::RIGHT) == EU(BattleSide::RIGHT_SIDE));
//                     vassert(v, EU(stack.unitSide()), "UNIT.SIDE");
//                     break;
//                 case UA::SLOT:
//                     vassert(v, stack.unitSlot(), "UNIT.SLOT");
//                     break;
//                 case UA::QUANTITY:
//                     vassert(v, stack.getCount(), "UNIT.QUANTITY");
//                     break;
//                 case UA::ATTACK:
//                     vassert(v, stack.getAttack(isShooter), "UNIT.ATTACK");
//                     break;
//                 case UA::DEFENSE:
//                     vassert(v, stack.getDefense(isShooter), "UNIT.DEFENSE");
//                     break;
//                 case UA::SHOTS:
//                     vassert(v, isShooter ? stack.shots.available() : 0, "UNIT.SHOTS");
//                     break;
//                 case UA::DMG_MIN:
//                     vassert(v, stack.getMinDamage(isShooter), "UNIT.DMG_MIN");
//                     break;
//                 case UA::DMG_MAX:
//                     vassert(v, stack.getMaxDamage(isShooter), "UNIT.DMG_MAX");
//                     break;
//                 case UA::HP:
//                     vassert(v, stack.getTotalHealth(), "UNIT.HP");
//                     break;
//                 case UA::HP_LEFT:
//                     vassert(v, stack.unitSlot(), "UNIT.HP_LEFT");
//                     break;
//                 case UA::SPEED:
//                     vassert(v, stack.unitSlot(), "UNIT.SPEED");
//                     break;
//                 case UA::VALUE_ONE:
//                 case UA::VALUE_REL:
//                 case UA::VALUE_REL0:
//                 case UA::VALUE_KILLED_REL:
//                 case UA::VALUE_KILLED_ACC_REL0:
//                 case UA::VALUE_LOST_REL:
//                 case UA::VALUE_LOST_ACC_REL0:
//                 case UA::DMG_DEALT_REL:
//                 case UA::DMG_DEALT_ACC_REL0:
//                 case UA::DMG_RECEIVED_REL:
//                 case UA::DMG_RECEIVED_ACC_REL0:
//                     break;
//                 case UA::FLAGS1:
//                 {
//                                 break;
//                             case SF1::SLEEPING:
//                                 cstack->unitType()->getId() == CreatureID::AMMO_CART
//                                     ? vassert(vf, false, "HEX.STACK_FLAGS1.SLEEPING")
//                                     : vassert(vf, cstack->hasBonusOfType(BonusType::NOT_ACTIVE), "HEX.STACK_FLAGS1.SLEEPING");
//                                 break;
//                             case SF1::BLOCKED:
//                             {
//                                 auto want = cstack->canShoot() && battle.battleIsUnitBlocked(cstack) && !cstack->hasBonusOfType(BonusType::FREE_SHOOTING)
//                                          && !cstack->hasBonusOfType(BonusType::SIEGE_WEAPON);
//                                 vassert(vf, want, "HEX.STACK_FLAGS1.BLOCKED");
//                             }
//                             break;
//                             case SF1::BLOCKING:
//                             {
//                                 auto adjUnits = battle.battleAdjacentUnits(cstack);
//                                 bool want = std::ranges::any_of(
//                                     adjUnits,
//                                     [&battle, &cstack](const auto & adjstack)
//                                     {
//                                         return adjstack->unitSide() != cstack->unitSide() && adjstack->canShoot() && battle.battleIsUnitBlocked(adjstack)
//                                             && !adjstack->hasBonusOfType(BonusType::FREE_SHOOTING) && !adjstack->hasBonusOfType(BonusType::SIEGE_WEAPON);
//                                     }
//                                 );

//                                 vassert(vf, want, "HEX.STACK_FLAGS1.BLOCKING");
//                             }
//                             break;
//                             case SF1::IS_WIDE:
//                                 vassert(vf, cstack->occupiedHex().isAvailable(), "HEX.STACK_FLAGS1.IS_WIDE");
//                                 break;
//                             case SF1::FLYING:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::FLYING), "HEX.STACK_FLAGS1.FLYING");
//                                 break;
//                             case SF1::ADDITIONAL_ATTACK:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::ADDITIONAL_ATTACK), "HEX.STACK_FLAGS1.ADDITIONAL_ATTACK");
//                                 break;
//                             case SF1::NO_MELEE_PENALTY:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "HEX.STACK_FLAGS1.NO_MELEE_PENALTY");
//                                 break;
//                             case SF1::TWO_HEX_ATTACK_BREATH:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "HEX.STACK_FLAGS1.TWO_HEX_ATTACK_BREATH");
//                                 break;
//                             case SF1::BLOCKS_RETALIATION:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "HEX.STACK_FLAGS1.BLOCKS_RETALIATION");
//                                 break;
//                             case SF1::SHOOTER:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::SHOOTER), "HEX.STACK_FLAGS1.SHOOTER");
//                                 // canShoot returns false if ammo = 0
//                                 // vassert(vf, cstack->canShoot(), "HEX.STACK_FLAGS1.SHOOTER (canShoot)");
//                                 break;
//                             case SF1::NON_LIVING:
//                             {
//                                 auto undead = cstack->hasBonusOfType(BonusType::UNDEAD);
//                                 auto nonliving = cstack->hasBonusOfType(BonusType::NON_LIVING);
//                                 vassert(vf, undead || nonliving, "HEX.STACK_FLAGS1.NON_LIVING", cstack->getDescription());
//                             }
//                             break;
//                             case SF1::WAR_MACHINE:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::SIEGE_WEAPON), "HEX.STACK_FLAGS1.WAR_MACHINE");
//                                 break;
//                             case SF1::FIREBALL:
//                                 vassert(
//                                     vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::FIREBALL)), "HEX.STACK_FLAGS1.FIREBALL"
//                                 );
//                                 break;
//                             case SF1::DEATH_CLOUD:
//                                 vassert(
//                                     vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::DEATH_CLOUD)), "HEX.STACK_FLAGS1.DEATH_CLOUD"
//                                 );
//                                 break;
//                             case SF1::THREE_HEADED_ATTACK:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::THREE_HEADED_ATTACK), "HEX.STACK_FLAGS1.THREE_HEADED_ATTACK");
//                                 break;
//                             case SF1::ALL_AROUND_ATTACK:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::ATTACKS_ALL_ADJACENT), "HEX.STACK_FLAGS1.ALL_AROUND_ATTACK");
//                                 break;
//                             case SF1::RETURN_AFTER_STRIKE:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::RETURN_AFTER_STRIKE), "HEX.STACK_FLAGS1.RETURN_AFTER_STRIKE");
//                                 break;
//                             case SF1::ENEMY_DEFENCE_REDUCTION:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::ENEMY_DEFENCE_REDUCTION), "HEX.STACK_FLAGS1.ENEMY_DEFENCE_REDUCTION");
//                                 break;
//                             case SF1::LIFE_DRAIN:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::LIFE_DRAIN), "HEX.STACK_FLAGS1.LIFE_DRAIN");
//                                 break;
//                             case SF1::DOUBLE_DAMAGE_CHANCE:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::DOUBLE_DAMAGE_CHANCE), "HEX.STACK_FLAGS1.DOUBLE_DAMAGE_CHANCE");
//                                 break;
//                             case SF1::DEATH_STARE:
//                                 vassert(vf, cstack->hasBonusOfType(BonusType::DEATH_STARE), "HEX.STACK_FLAGS1.DEATH_STARE");
//                                 break;
//                             default:
//                                 THROW_FORMAT("Unexpected StackFlag: %d", EI(f));
//                         }
//                     }
//                 }
//                 break;
//                 case UA::STACK_FLAGS2:
//                 {
//                     if(!isNA(v, cstack, "HEX.STACK_FLAGS"))
//                     {
//                         for(int j = 0; j < EI(StackFlag2::_count); j++)
//                         {
//                             auto f = static_cast<StackFlag2>(j);
//                             auto vf = hex->stack->flag(f);

//                             switch(f)
//                             {
//                                 case SF2::AGE:
//                                     vassert(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE");
//                                     break;
//                                 case SF2::AGE_ATTACK:
//                                     vassert(
//                                         vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::BIND:
//                                     vassert(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND");
//                                     break;
//                                 case SF2::BIND_ATTACK:
//                                     vassert(
//                                         vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::BLIND:
//                                 {
//                                     auto blind = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BLIND));
//                                     auto paralyzed = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::PARALYZE));
//                                     vassert(vf, blind || paralyzed, "HEX.STACK_FLAGS2.BLIND");
//                                 }
//                                 break;
//                                 case SF2::BLIND_ATTACK:
//                                 {
//                                     auto blind = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BLIND));
//                                     auto paralyze = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::PARALYZE));
//                                     vassert(vf, blind || paralyze, "HEX.STACK_FLAGS2.BLIND_ATTACK");
//                                 }
//                                 break;
//                                 case SF2::CURSE:
//                                     vassert(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE");
//                                     break;
//                                 case SF2::CURSE_ATTACK:
//                                     vassert(
//                                         vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::DISPEL_ATTACK:
//                                     vassert(
//                                         vf,
//                                         cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::DISPEL_HELPFUL_SPELLS)),
//                                         "HEX.STACK_FLAGS2.DISPEL_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::PETRIFY:
//                                     vassert(
//                                         vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::STONE_GAZE)), "HEX.STACK_FLAGS2.PETRIFY"
//                                     );
//                                     break;
//                                 case SF2::PETRIFY_ATTACK:
//                                     vassert(
//                                         vf,
//                                         cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::STONE_GAZE)),
//                                         "HEX.STACK_FLAGS2.PETRIFY_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::POISON:
//                                     vassert(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON");
//                                     break;
//                                 case SF2::POISON_ATTACK:
//                                     vassert(
//                                         vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON_ATTACK"
//                                     );
//                                     break;
//                                 case SF2::WEAKNESS:
//                                     vassert(
//                                         vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::WEAKNESS)), "HEX.STACK_FLAGS2.WEAKNESS"
//                                     );
//                                     break;
//                                 case SF2::WEAKNESS_ATTACK:
//                                     vassert(
//                                         vf,
//                                         cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::WEAKNESS)),
//                                         "HEX.STACK_FLAGS2.WEAKNESS_ATTACK"
//                                     );
//                                     break;
//                                 default:
//                                     THROW_FORMAT("Unexpected StackFlag2: %d", EI(f));
//                             }
//                         }
//                     }
//                 }
//                 break;
//                 default:
//                     THROW_FORMAT("Unexpected HexAttribute: %d", EI(attr));
//             }
//         }
//  }
// }

}
