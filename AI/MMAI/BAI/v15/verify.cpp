#include "StdInc.h"
#include "BAI/v15/graph/edges/generic.h"
#include "BAI/v15/graph/edges/unit_shoot_dmg_unit.h"
#include "BAI/v15/state.h"
#include "CStack.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleAttackInfo.h"
#include "battle/CObstacleInstance.h"
#include "battle/CPlayerBattleCallback.h"
#include "battle/IBattleInfoCallback.h"
#include "constants/EntityIdentifiers.h"
#include "constants/Enumerations.h"
#include "mapObjects/CGTownInstance.h"
#include "schema/base.h"
#include "vcmi/spells/Caster.h"

#include "common.h"

#include "schema/v15/constants.h"
#include "schema/v15/types.h"


namespace MMAI::BAI::V15
{
namespace S15 = Schema::V15;
using ET = S15::Graph::ElementType;

namespace N = Graph::Nodes;
namespace E = Graph::Edges;
using UnitPtr = std::shared_ptr<const N::Unit>;
using HexPtr = std::shared_ptr<const N::Hex>;
using ActionPtr = std::shared_ptr<const N::Action>;
using TowerFlags = N::Global::TowerFlags;
using CorpseFlags = N::Global::CorpseFlags;
using ET = S15::Graph::ElementType;
using AT = S15::ActionType;

using GA = S15::Graph::NodeAttributes::Global;
using PA = S15::Graph::NodeAttributes::Player;
using UA = S15::Graph::NodeAttributes::Unit;
using HA = S15::Graph::NodeAttributes::Hex;
using AA = S15::Graph::NodeAttributes::Action;


namespace
{
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

    struct Context
    {
        const CPlayerBattleCallback & battle;
        std::vector<const CStack *> allstacks;
        std::array<const CStack *, 7> l_CStacks{};
        std::array<const CStack *, 7> r_CStacks{};
        std::vector<const CStack *> l_CStacksAll;
        std::vector<const CStack *> r_CStacksAll;
        std::vector<const CStack *> l_CStacksExtra;
        std::vector<const CStack *> r_CStacksExtra;
        std::vector<const CStack *> l_CStacksSummons;
        std::vector<const CStack *> r_CStacksSummons;
        std::vector<const CStack *> l_CStacksMachines;
        std::vector<const CStack *> r_CStacksMachines;
        std::array<const CStack *, 165> hexstacks{};

        std::map<const CStack *, ReachabilityInfo> rinfos;
    };

    std::vector<const CStack *> getAllStacksForSide(const Context & ctx, bool side)
    {
        return side ? ctx.r_CStacksAll : ctx.l_CStacksAll;
    }

    void ensureValueMatch(int have, int want, const std::string_view attrname, const std::string & desc = "")
    {
        desc.empty() ? expect(have == want, "%s: have: %d, want: %d", attrname, have, want)
                     : expect(have == want, "%s: have: %d, want: %d (%s)", attrname, have, want, desc.c_str());
    };

    // Return (attr == N/A), but after performing some checks
    bool isNA(int v, const CStack * stack, const std::string_view attrname)
    {
        if(v == S15::NULL_VALUE_UNENCODED)
        {
            expect(!stack, "%s: N/A but stack != nullptr", attrname);
            return true;
        }
        expect(stack, "%s: != N/A but stack = nullptr", attrname);
        return false;
    };

    bool checkReachable(const Context & ctx, BattleHex bh, bool v, const CStack * stack)
    {
        auto distance = ctx.rinfos.at(stack).distances.at(bh.toInt());
        auto canreach = (stack->getMovementRange() >= distance);

        // XXX: if v=false, returns true when UNreachable
        //      if v=true returns true when reachable
        return v ? canreach : !canreach;
    };

    void ensureReachability(const Context & ctx, BattleHex bh, bool v, const CStack * stack, const char * attrname)
    {
        expect(checkReachable(ctx, bh, v, stack), "%s: (bhex=%d) reachability expected: %d", attrname, bh.toInt(), v);
    };

    // as opposed to ensureHexShootableOrNA, this hexattr works with a mask
    // values are 0 or 1 and this check requires a valid target
    void ensureShootability(const Context & ctx, BattleHex bh, int v, const CStack * cstack, const char * attrname)
    {
        auto canshoot = ctx.battle.battleCanShoot(cstack);
        auto estacks = getAllStacksForSide(ctx, !EU(cstack->unitSide()));

        const auto it = std::ranges::find_if( // NOLINT(readability-qualified-auto)
            estacks,
            [&bh](auto estack)
            {
                return estack && estack->coversPos(bh);
            }
        );

        const auto * estack = it == estacks.end() ? nullptr : *it;

        // XXX: the estack on `bh` might be "hidden" from the state
        //      in which case the mask for shooting will be 0 although
        //      there IS a stack to shoot on this hex
        if(v)
        {
            expect(estack, "%s: =%d, but estack is nullptr", attrname, bh.toInt());
            expect(canshoot, "%s: =%d but canshoot=%d", attrname, v, canshoot);
        }
        else
        {
            // stack must be unable to shoot
            // OR there must be no target at hex
            expect(!canshoot || !estack, "%s: =%d but canshoot=%d and estack is not null", attrname, v, canshoot);
        }
    };

}

// This function used during model development and is never called otherwise
void Verify(const State * state) // NOSONAR - function used for debugging only
{
    const auto & battle = state->battle;
    const CStack * astack = nullptr;

    Context ctx{.battle = battle};

    ctx.allstacks = battle.battleGetStacks();
    std::ranges::sort(
        ctx.allstacks,
        [](const CStack * a, const CStack * b)
        {
            return a->unitId() < b->unitId();
        }
    );

    for(auto & cstack : ctx.allstacks)
    {
        if(cstack->unitId() == battle.battleActiveUnit()->unitId())
            astack = cstack;

        if(cstack->unitSlot() < 0)
        {
            if(cstack->unitSlot() == SlotID::SUMMONED_SLOT_PLACEHOLDER)
                cstack->unitSide() == BattleSide::DEFENDER ? ctx.r_CStacksSummons.push_back(cstack) : ctx.l_CStacksSummons.push_back(cstack);
            else if(cstack->unitSlot() == SlotID::WAR_MACHINES_SLOT)
                cstack->unitSide() == BattleSide::DEFENDER ? ctx.r_CStacksMachines.push_back(cstack) : ctx.l_CStacksMachines.push_back(cstack);
        }
        else
        {
            cstack->unitSide() == BattleSide::DEFENDER ? ctx.r_CStacks.at(cstack->unitSlot()) = cstack : ctx.l_CStacks.at(cstack->unitSlot()) = cstack;
        }

        ctx.rinfos.try_emplace(cstack, battle.getReachability(cstack));

        for(const auto & bh : cstack->getHexes())
        {
            if(!bh.isAvailable())
                continue; // war machines rear hex, arrow towers
            expect(!ctx.hexstacks.at(Graph::Nodes::Hex::CalcId(bh)), "hex occupied by multiple stacks?");
            ctx.hexstacks.at(Graph::Nodes::Hex::CalcId(bh)) = cstack;
        }
    }

    ctx.l_CStacksAll.insert(ctx.l_CStacksAll.end(), ctx.l_CStacks.begin(), ctx.l_CStacks.end());
    ctx.l_CStacksAll.insert(ctx.l_CStacksAll.end(), ctx.l_CStacksSummons.begin(), ctx.l_CStacksSummons.end());
    ctx.l_CStacksAll.insert(ctx.l_CStacksAll.end(), ctx.l_CStacksMachines.begin(), ctx.l_CStacksMachines.end());

    ctx.r_CStacksAll.insert(ctx.r_CStacksAll.end(), ctx.r_CStacks.begin(), ctx.r_CStacks.end());
    ctx.r_CStacksAll.insert(ctx.r_CStacksAll.end(), ctx.r_CStacksSummons.begin(), ctx.r_CStacksSummons.end());
    ctx.r_CStacksAll.insert(ctx.r_CStacksAll.end(), ctx.r_CStacksMachines.begin(), ctx.r_CStacksMachines.end());

    ctx.l_CStacksExtra.insert(ctx.l_CStacksExtra.end(), ctx.l_CStacksSummons.begin(), ctx.l_CStacksSummons.end());
    ctx.l_CStacksExtra.insert(ctx.l_CStacksExtra.end(), ctx.l_CStacksMachines.begin(), ctx.l_CStacksMachines.end());

    ctx.r_CStacksExtra.insert(ctx.r_CStacksExtra.end(), ctx.r_CStacksSummons.begin(), ctx.r_CStacksSummons.end());
    ctx.r_CStacksExtra.insert(ctx.r_CStacksExtra.end(), ctx.r_CStacksMachines.begin(), ctx.r_CStacksMachines.end());

    const auto SideStacks = std::map<bool, std::vector<const CStack *> *>{
        {false, &ctx.l_CStacksAll},
        {true,  &ctx.r_CStacksAll}
    };

    const auto ended = state->supdata->ended;

    if(!astack)
        expect(ended, "astack is NULL, but ended is not true");
    else if(ended)
    {
        // at battle-end, activeStack is usually the ENEMY stack
        // XXX: this expect will incorrectly throw if we retreated as a regular action
        //      (in which case our stack will be active, but we would have lost the battle)
        // expect(state->supdata->victory == (astack->getOwner() == battle.battleGetMySide()), "state->supdata->victory is %d, but astack->side=%d and myside=%d", state->supdata->victory, astack->getOwner(), battle.battleGetMySide());

        // at battle-end, even regardless of the actual active stack,
        // battlefield->astack must be nullptr
        expect(state->supdata->getIsBattleEnded(), "ended, but state->supdata->getIsBattleEnded() is false");
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

    const auto & global = G.getAll<N::Global>().at(0);

    if(ended)
    {
        static_assert(EU(Side::LEFT) == EU(BattleSide::ATTACKER));
        static_assert(EU(Side::RIGHT) == EU(BattleSide::DEFENDER));

        auto fin = battle.battleIsFinished();

        // XXX: The logic in battleIsFinished is flawed and returns no value
        //      (i.e. "not finished") if both sides have units, which can
        //      happen if the WE some has retreated as a regular action (not via reset).
        // ASSERT(fin.has_value(), "ended, but battleIsFinished returns no value?");

        if(fin.has_value())
        {
            // NONE means draw (no units on battlefield) -- our value will be null in this case
            (fin == BattleSide::NONE) ? ensureValueMatch(global->attr(GA::BATTLE_WINNER), S15::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (draw)")
                                      : ensureValueMatch(global->attr(GA::BATTLE_WINNER), EU(fin.value()), "GA.BATTLE_WINNER");
        }
        else
        {
            // we have retreated *as an action*
            // There seems to be no way to ask vcmi "which side retreated"
        }

        auto activeside = EU(battle.battleGetMySide() == BattleSide::ATTACKER ? Side::LEFT : Side::RIGHT);
        ensureValueMatch(global->attr(GA::BATTLE_SIDE_ACTIVE_PLAYER), activeside, "GA.BATTLE_SIDE_ACTIVE_PLAYER");
    }
    else
    {
        static_assert(EU(Side::LEFT) == EU(BattleSide::LEFT_SIDE));
        static_assert(EU(Side::RIGHT) == EU(BattleSide::RIGHT_SIDE));
        ASSERT(astack != nullptr, "not ended, but no astack either");
        ensureValueMatch(global->attr(GA::BATTLE_WINNER), S15::NULL_VALUE_UNENCODED, "GA.BATTLE_WINNER (battle ongoing)");
        ensureValueMatch(global->attr(GA::BATTLE_SIDE_ACTIVE_PLAYER), EU(astack->unitSide()), "GA.BATTLE_SIDE_ACTIVE_PLAYER");
    }
    auto alogs = state->supdata->getAttackLogs();

    {
        auto gatecorpse = false;
        auto bridgecorpse = false;

        if(battle.battleGetFortifications().wallsHealth > 0) {
            for(const CStack * cstack : battle.battleGetAllStacks(false)) {
                if(cstack->alive())
                    continue;

                if(cstack->coversPos(BattleHex::GATE_INNER) || cstack->coversPos(BattleHex::GATE_OUTER))
                    gatecorpse = true;

                if(cstack->coversPos(BattleHex::GATE_BRIDGE))
                    bridgecorpse = true;
            }
        }

        auto corpsemask = CorpseFlags(global->attr(GA::SIEGE_CORPSES));
        if(gatecorpse)
            ensureValueMatch(corpsemask.test(0), 1, "GA.SIEGE_CORPSES<0>");
        if(bridgecorpse)
            ensureValueMatch(corpsemask.test(1), 1, "GA.SIEGE_CORPSES<1>");
    }

    {
        auto f = battle.battleGetFortifications();
        auto towermask = TowerFlags(global->attr(GA::SIEGE_TOWERS));
        if(f.upperTowerHealth > 0 && battle.battleGetWallState(EWallPart::UPPER_TOWER) != EWallState::DESTROYED)
            ensureValueMatch(towermask.test(0), 1, "GA.SIEGE_TOWERS<0>");
        if(f.citadelHealth > 0 && battle.battleGetWallState(EWallPart::KEEP) != EWallState::DESTROYED)
            ensureValueMatch(towermask.test(1), 1, "GA.SIEGE_TOWERS<1>");
        if(f.lowerTowerHealth > 0 && battle.battleGetWallState(EWallPart::BOTTOM_TOWER) != EWallState::DESTROYED)
            ensureValueMatch(towermask.test(2), 1, "GA.SIEGE_TOWERS<2>");
    }

    for(int ihex = 0; ihex < 165; ihex++)
    {
        int x = ihex % 15;
        int y = ihex / 15;
        const auto & hex = G.getById<N::Hex>(ihex);
        const auto & bh = hex->bhex;
        expect(bh == BattleHex(x + 1, y), "hex->bhex mismatch");

        auto ainfo = battle.getAccessibility();
        auto aa = ainfo.at(bh.toInt());

        for(int i = 0; i < EU(HA::_count); i++)
        {
            auto attr = static_cast<HA>(i);
            auto v = hex->attrs.at(i);
            const auto * cstack = ctx.hexstacks.at(ihex);
            const auto occupants = G.getAllEdgesSrcByDst<E::Unit_Occupies_Hex>(hex);

            if(cstack)
            {
                expect(std::ranges::distance(occupants) == 1, "expected 1 occupant, got: %d", std::ranges::distance(occupants));
                expect(&occupants.at(0)->cstack == cstack, "occupant cstack mismatch");
            }
            else
            {
                expect(occupants.empty(), "cstack is nullptr, occupants are present");
            }

            switch(attr)
            {
                case HA::Y_COORD:
                    expect(v == y, "HEX.Y_COORD: %d != %d", v, y);
                    break;
                case HA::X_COORD:
                    expect(v == x, "HEX.X_COORD: %d != %d", v, x);
                    break;
                case HA::STATE_MASK:
                {
                    auto obstacles = battle.battleGetAllObstaclesOnPos(bh, false);
                    auto anyobstacle = [&obstacles](auto fn)
                    {
                        return std::any_of(
                            obstacles.begin(),
                            obstacles.end(),
                            [&fn](const std::shared_ptr<const CObstacleInstance> & obstacle)
                            {
                                return fn(obstacle.get());
                            }
                        );
                    };

                    auto mask = S15::HexStateMask(v);
                    BattleSide side = astack ? astack->unitSide() : BattleSide::ATTACKER; // XXX: Hex defaults to 0 if there is no astack

                    if(mask.test(EU(S15::HexState::PASSABLE)))
                    {
                        expect(
                            aa == EAccessibility::ACCESSIBLE || (EU(side) && aa == EAccessibility::GATE),
                            "HEX.STATE_MASK: PASSABLE bit is set, but accessibility is %d (side: %d)",
                            EU(aa),
                            EU(side)
                        );
                    }
                    else
                    {
                        if(aa == EAccessibility::OBSTACLE || aa == EAccessibility::ALIVE_STACK)
                            break;

                        switch(aa)
                        {
                            case EAccessibility::ACCESSIBLE:
                                throw std::runtime_error("HEX.STATE_MASK: PASSABLE bit not set, but accessibility is ACCESSIBLE");
                                break;
                            case EAccessibility::ALIVE_STACK:
                            case EAccessibility::OBSTACLE:
                            case EAccessibility::DESTRUCTIBLE_WALL:
                            case EAccessibility::GATE:
                                break;
                            case EAccessibility::UNAVAILABLE:
                                // only Fort and Boat battles can have unavailable hexes
                                expect(
                                    battle.battleGetFortifications().wallsHealth > 0 || battle.battleTerrainType() == TerrainId::WATER,
                                    "Found UNAVAILABLE accessibility on non-fort, non-boat battlefield: tertype=%d",
                                    EU(battle.battleTerrainType())
                                );
                                break;
                            case EAccessibility::SIDE_COLUMN:
                                // side hexes should are not included in the observation
                                throw std::runtime_error("HEX.STATE_MASK: SIDE_COLUMN accessibility found");
                                break;
                            default:
                                throw std::runtime_error("Unexpected accessibility: " + std::to_string(EU(aa)));
                        }
                    }

                    if(mask.test(EU(S15::HexState::STOPPING)))
                    {
                        auto stopping = anyobstacle(std::mem_fn(&CObstacleInstance::stopsMovement));
                        expect(stopping, "HEX.STATE_MASK: STOPPING bit is set, but no obstacle stops movement");
                    }

                    if(mask.test(EU(S15::HexState::DAMAGING_L)))
                    {
                        auto damaging = anyobstacle(
                            [side](const CObstacleInstance * o)
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
                                const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
                                auto bside = static_cast<bool>(side);
                                return (side == so->casterSide) ? bside : !bside;
                            }
                        );
                        expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
                    }

                    if(mask.test(EU(S15::HexState::DAMAGING_R)))
                    {
                        auto damaging = anyobstacle(
                            [side](const CObstacleInstance * o)
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
                                const auto * so = dynamic_cast<const SpellCreatedObstacle *>(o);
                                auto bside = static_cast<bool>(side);
                                return (side == so->casterSide) ? !bside : bside;
                            }
                        );
                        expect(damaging, "HEX.STATE_MASK: DAMAGING bit is set, but no obstacle triggers a damaging effect");
                    }
                }
                break;
                case HA::WALL_HEALTH:
                {
                    auto wp = battle.battleHexToWallPart(hex->bhex);
                    if(wp != EWallPart::BOTTOM_WALL && wp != EWallPart::BELOW_GATE && wp != EWallPart::GATE && wp != EWallPart::OVER_GATE && wp != EWallPart::UPPER_WALL)
                        return;
                    auto ws = battle.battleGetWallState(wp);
                    if(ws == EWallState::NONE)
                        ensureValueMatch(v, Schema::V15::NULL_VALUE_UNENCODED, "HEX.WALL_HEALTH");
                    else
                        ensureValueMatch(v, EU(ws), "HEX.WALL_HEALTH");
                }
                break;
                default:
                    THROW_FORMAT("Unexpected HexAttribute: %d", EU(attr));
            }
        }
    }

    for(const auto & unit : G.getAll<N::Unit>())
    {
        const auto & stack = unit->cstack;

        for(int i = 0; i < EU(UA::_count); i++)
        {
            auto attr = static_cast<UA>(i);
            auto v = unit->attr(attr);

            bool isShooter = G.getOneEdgeBySrc<E::Unit_ShootDmg_Unit>(unit, false) != nullptr;

            switch(attr)
            {
                case UA::SIDE:
                    static_assert(EU(Schema::Side::LEFT) == EU(BattleSide::LEFT_SIDE));
                    static_assert(EU(Schema::Side::RIGHT) == EU(BattleSide::RIGHT_SIDE));
                    ensureValueMatch(v, EU(stack.unitSide()), "Unit.SIDE");
                    break;
                case UA::SLOT:
                    ensureValueMatch(v, stack.unitSlot(), "Unit.SLOT");
                    break;
                case UA::QUANTITY:
                    ensureValueMatch(v, stack.getCount(), "Unit.QUANTITY");
                    break;
                case UA::ATTACK:
                    ensureValueMatch(v, stack.getAttack(isShooter), "Unit.ATTACK");
                    break;
                case UA::DEFENSE:
                    ensureValueMatch(v, stack.getDefense(isShooter), "Unit.DEFENSE");
                    break;
                case UA::SHOTS:
                    ensureValueMatch(v, isShooter ? stack.shots.available() : 0, "Unit.SHOTS");
                    break;
                case UA::DMG_MIN:
                    ensureValueMatch(v, stack.getMinDamage(isShooter), "Unit.DMG_MIN");
                    break;
                case UA::DMG_MAX:
                    ensureValueMatch(v, stack.getMaxDamage(isShooter), "Unit.DMG_MAX");
                    break;
                case UA::HP:
                    ensureValueMatch(v, stack.getTotalHealth(), "Unit.HP");
                    break;
                case UA::HP_LEFT:
                    ensureValueMatch(v, stack.unitSlot(), "Unit.HP_LEFT");
                    break;
                case UA::SPEED:
                    ensureValueMatch(v, stack.unitSlot(), "Unit.SPEED");
                    break;
                case UA::VALUE_ONE:
                case UA::VALUE_REL:
                case UA::VALUE_REL0:
                case UA::VALUE_KILLED_REL:
                case UA::VALUE_KILLED_ACC_REL0:
                case UA::VALUE_LOST_REL:
                case UA::VALUE_LOST_ACC_REL0:
                case UA::DMG_DEALT_REL:
                case UA::DMG_DEALT_ACC_REL0:
                case UA::DMG_RECEIVED_REL:
                case UA::DMG_RECEIVED_ACC_REL0:
                    break;
                case UA::FLAGS1:
                {
                                break;
                            case SF1::SLEEPING:
                                cstack->unitType()->getId() == CreatureID::AMMO_CART
                                    ? ensureValueMatch(vf, false, "HEX.STACK_FLAGS1.SLEEPING")
                                    : ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::NOT_ACTIVE), "HEX.STACK_FLAGS1.SLEEPING");
                                break;
                            case SF1::BLOCKED:
                            {
                                auto want = cstack->canShoot() && battle.battleIsUnitBlocked(cstack) && !cstack->hasBonusOfType(BonusType::FREE_SHOOTING)
                                         && !cstack->hasBonusOfType(BonusType::SIEGE_WEAPON);
                                ensureValueMatch(vf, want, "HEX.STACK_FLAGS1.BLOCKED");
                            }
                            break;
                            case SF1::BLOCKING:
                            {
                                auto adjUnits = battle.battleAdjacentUnits(cstack);
                                bool want = std::ranges::any_of(
                                    adjUnits,
                                    [&battle, &cstack](const auto & adjstack)
                                    {
                                        return adjstack->unitSide() != cstack->unitSide() && adjstack->canShoot() && battle.battleIsUnitBlocked(adjstack)
                                            && !adjstack->hasBonusOfType(BonusType::FREE_SHOOTING) && !adjstack->hasBonusOfType(BonusType::SIEGE_WEAPON);
                                    }
                                );

                                ensureValueMatch(vf, want, "HEX.STACK_FLAGS1.BLOCKING");
                            }
                            break;
                            case SF1::IS_WIDE:
                                ensureValueMatch(vf, cstack->occupiedHex().isAvailable(), "HEX.STACK_FLAGS1.IS_WIDE");
                                break;
                            case SF1::FLYING:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::FLYING), "HEX.STACK_FLAGS1.FLYING");
                                break;
                            case SF1::ADDITIONAL_ATTACK:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::ADDITIONAL_ATTACK), "HEX.STACK_FLAGS1.ADDITIONAL_ATTACK");
                                break;
                            case SF1::NO_MELEE_PENALTY:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::NO_MELEE_PENALTY), "HEX.STACK_FLAGS1.NO_MELEE_PENALTY");
                                break;
                            case SF1::TWO_HEX_ATTACK_BREATH:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::TWO_HEX_ATTACK_BREATH), "HEX.STACK_FLAGS1.TWO_HEX_ATTACK_BREATH");
                                break;
                            case SF1::BLOCKS_RETALIATION:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::BLOCKS_RETALIATION), "HEX.STACK_FLAGS1.BLOCKS_RETALIATION");
                                break;
                            case SF1::SHOOTER:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::SHOOTER), "HEX.STACK_FLAGS1.SHOOTER");
                                // canShoot returns false if ammo = 0
                                // ensureValueMatch(vf, cstack->canShoot(), "HEX.STACK_FLAGS1.SHOOTER (canShoot)");
                                break;
                            case SF1::NON_LIVING:
                            {
                                auto undead = cstack->hasBonusOfType(BonusType::UNDEAD);
                                auto nonliving = cstack->hasBonusOfType(BonusType::NON_LIVING);
                                ensureValueMatch(vf, undead || nonliving, "HEX.STACK_FLAGS1.NON_LIVING", cstack->getDescription());
                            }
                            break;
                            case SF1::WAR_MACHINE:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::SIEGE_WEAPON), "HEX.STACK_FLAGS1.WAR_MACHINE");
                                break;
                            case SF1::FIREBALL:
                                ensureValueMatch(
                                    vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::FIREBALL)), "HEX.STACK_FLAGS1.FIREBALL"
                                );
                                break;
                            case SF1::DEATH_CLOUD:
                                ensureValueMatch(
                                    vf, cstack->hasBonusOfType(BonusType::SPELL_LIKE_ATTACK, SpellID(SpellID::DEATH_CLOUD)), "HEX.STACK_FLAGS1.DEATH_CLOUD"
                                );
                                break;
                            case SF1::THREE_HEADED_ATTACK:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::THREE_HEADED_ATTACK), "HEX.STACK_FLAGS1.THREE_HEADED_ATTACK");
                                break;
                            case SF1::ALL_AROUND_ATTACK:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::ATTACKS_ALL_ADJACENT), "HEX.STACK_FLAGS1.ALL_AROUND_ATTACK");
                                break;
                            case SF1::RETURN_AFTER_STRIKE:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::RETURN_AFTER_STRIKE), "HEX.STACK_FLAGS1.RETURN_AFTER_STRIKE");
                                break;
                            case SF1::ENEMY_DEFENCE_REDUCTION:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::ENEMY_DEFENCE_REDUCTION), "HEX.STACK_FLAGS1.ENEMY_DEFENCE_REDUCTION");
                                break;
                            case SF1::LIFE_DRAIN:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::LIFE_DRAIN), "HEX.STACK_FLAGS1.LIFE_DRAIN");
                                break;
                            case SF1::DOUBLE_DAMAGE_CHANCE:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::DOUBLE_DAMAGE_CHANCE), "HEX.STACK_FLAGS1.DOUBLE_DAMAGE_CHANCE");
                                break;
                            case SF1::DEATH_STARE:
                                ensureValueMatch(vf, cstack->hasBonusOfType(BonusType::DEATH_STARE), "HEX.STACK_FLAGS1.DEATH_STARE");
                                break;
                            default:
                                THROW_FORMAT("Unexpected StackFlag: %d", EI(f));
                        }
                    }
                }
                break;
                case UA::STACK_FLAGS2:
                {
                    if(!isNA(v, cstack, "HEX.STACK_FLAGS"))
                    {
                        for(int j = 0; j < EI(StackFlag2::_count); j++)
                        {
                            auto f = static_cast<StackFlag2>(j);
                            auto vf = hex->stack->flag(f);

                            switch(f)
                            {
                                case SF2::AGE:
                                    ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE");
                                    break;
                                case SF2::AGE_ATTACK:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::AGE)), "HEX.STACK_FLAGS2.AGE_ATTACK"
                                    );
                                    break;
                                case SF2::BIND:
                                    ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND");
                                    break;
                                case SF2::BIND_ATTACK:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BIND)), "HEX.STACK_FLAGS2.BIND_ATTACK"
                                    );
                                    break;
                                case SF2::BLIND:
                                {
                                    auto blind = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::BLIND));
                                    auto paralyzed = cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::PARALYZE));
                                    ensureValueMatch(vf, blind || paralyzed, "HEX.STACK_FLAGS2.BLIND");
                                }
                                break;
                                case SF2::BLIND_ATTACK:
                                {
                                    auto blind = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::BLIND));
                                    auto paralyze = cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::PARALYZE));
                                    ensureValueMatch(vf, blind || paralyze, "HEX.STACK_FLAGS2.BLIND_ATTACK");
                                }
                                break;
                                case SF2::CURSE:
                                    ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE");
                                    break;
                                case SF2::CURSE_ATTACK:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::CURSE)), "HEX.STACK_FLAGS2.CURSE_ATTACK"
                                    );
                                    break;
                                case SF2::DISPEL_ATTACK:
                                    ensureValueMatch(
                                        vf,
                                        cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::DISPEL_HELPFUL_SPELLS)),
                                        "HEX.STACK_FLAGS2.DISPEL_ATTACK"
                                    );
                                    break;
                                case SF2::PETRIFY:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::STONE_GAZE)), "HEX.STACK_FLAGS2.PETRIFY"
                                    );
                                    break;
                                case SF2::PETRIFY_ATTACK:
                                    ensureValueMatch(
                                        vf,
                                        cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::STONE_GAZE)),
                                        "HEX.STACK_FLAGS2.PETRIFY_ATTACK"
                                    );
                                    break;
                                case SF2::POISON:
                                    ensureValueMatch(vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON");
                                    break;
                                case SF2::POISON_ATTACK:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::POISON)), "HEX.STACK_FLAGS2.POISON_ATTACK"
                                    );
                                    break;
                                case SF2::WEAKNESS:
                                    ensureValueMatch(
                                        vf, cstack->hasBonusFrom(BonusSource::SPELL_EFFECT, SpellID(SpellID::WEAKNESS)), "HEX.STACK_FLAGS2.WEAKNESS"
                                    );
                                    break;
                                case SF2::WEAKNESS_ATTACK:
                                    ensureValueMatch(
                                        vf,
                                        cstack->hasBonusOfType(BonusType::SPELL_AFTER_ATTACK, SpellID(SpellID::WEAKNESS)),
                                        "HEX.STACK_FLAGS2.WEAKNESS_ATTACK"
                                    );
                                    break;
                                default:
                                    THROW_FORMAT("Unexpected StackFlag2: %d", EI(f));
                            }
                        }
                    }
                }
                break;
                default:
                    THROW_FORMAT("Unexpected HexAttribute: %d", EI(attr));
            }
        }
    }

    // Mask is undefined at battle end
    if(ended)
        return;
}

}
