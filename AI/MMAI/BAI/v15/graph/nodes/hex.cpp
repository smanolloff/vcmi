#include "BAI/v15/graph/nodes/hex.h"

#include "AI/MMAI/common.h"
#include "vcmi/spells/Spell.h"

namespace MMAI::BAI::V15::Graph::Nodes
{

int Hex::CalcId(const BattleHex& bh)
{
    ASSERT(bh.isAvailable(), "Hex unavailable: " + std::to_string(bh.toInt()));
    return bh.getX() - 1 + (bh.getY() * 15);
}

std::pair<int, int> Hex::CalcXY(const BattleHex& bh)
{
    return {bh.getX() - 1, bh.getY()};
}

Hex::HexActionHex Hex::NearbyBattleHexes(const BattleHex& bh)
{
    static_assert(EU(HexAction::AMOVE_TR) == 0);
    static_assert(EU(HexAction::AMOVE_R) == 1);
    static_assert(EU(HexAction::AMOVE_BR) == 2);
    static_assert(EU(HexAction::AMOVE_BL) == 3);
    static_assert(EU(HexAction::AMOVE_L) == 4);
    static_assert(EU(HexAction::AMOVE_TL) == 5);
    static_assert(EU(HexAction::AMOVE_2TR) == 6);
    static_assert(EU(HexAction::AMOVE_2R) == 7);
    static_assert(EU(HexAction::AMOVE_2BR) == 8);
    static_assert(EU(HexAction::AMOVE_2BL) == 9);
    static_assert(EU(HexAction::AMOVE_2L) == 10);
    static_assert(EU(HexAction::AMOVE_2TL) == 11);

    auto nbhR = bh.cloneInDirection(BattleHex::EDir::RIGHT, false);
    auto nbhL = bh.cloneInDirection(BattleHex::EDir::LEFT, false);

    return HexActionHex{
        bh.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
        nbhR,
        bh.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),
        bh.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
        nbhL,
        bh.cloneInDirection(BattleHex::EDir::TOP_LEFT, false),
        nbhR.cloneInDirection(BattleHex::EDir::TOP_RIGHT, false),
        nbhR.cloneInDirection(BattleHex::EDir::RIGHT, false),
        nbhR.cloneInDirection(BattleHex::EDir::BOTTOM_RIGHT, false),
        nbhL.cloneInDirection(BattleHex::EDir::BOTTOM_LEFT, false),
        nbhL.cloneInDirection(BattleHex::EDir::LEFT, false),
        nbhL.cloneInDirection(BattleHex::EDir::TOP_LEFT, false)
    };
}

Hex::Hex(
    const BattleHex& bhex_,
    EAccessibility accessibility,
    const std::vector<std::shared_ptr<const CObstacleInstance>>& obstacles,
    const CStack* cstack_,
    bool isRUFR_,
    int wallHP,
    bool isGateOpen
)
    : bhex(bhex_)
    , id(CalcId(bhex_))
    , cstack(cstack_)
    , isRUFR(isRUFR_)
    , moveDestHex(isRUFR_ ? frontOf(bhex_, cstack_) : bhex_)
{
    attrs.fill(S15::NULL_VALUE_UNENCODED);

    auto [x, y] = CalcXY(bhex);

    setattr(HA::Y_COORD, y);
    setattr(HA::X_COORD, x);
    setattr(HA::IS_REAR, cstack && bhex == cstack->occupiedHex());
    setattr(HA::IS_RUFR, isRUFR);
    setattr(HA::WALL_HEALTH, wallHP);

    if(cstack)
        setStateMask(accessibility, obstacles, cstack->unitSide(), isGateOpen);
    else
        setStateMask(accessibility, obstacles, BattleSide::ATTACKER, isGateOpen);

    finalize();
}

std::string Hex::name() const
{
    return "(" + std::to_string(attr(HA::Y_COORD)) + "," + std::to_string(attr(HA::X_COORD)) + ")";
}

void Hex::finalize()
{
    attrs.at(EU(HA::STATE_MASK)) = static_cast<int>(statemask.to_ulong());
}

BattleHex Hex::frontOf(const BattleHex& bhex, const CStack* cstack)
{
    if(!cstack)
        return bhex;

    const auto attacker = cstack->unitSide() == BattleSide::ATTACKER;
    return bhex.cloneInDirection(attacker ? BattleHex::RIGHT : BattleHex::LEFT, true);
}

void Hex::setattr(HA a, int value)
{
    attrs.at(EU(a)) = value;
}

void Hex::setStateMask(
    EAccessibility accessibility,
    const std::vector<std::shared_ptr<const CObstacleInstance>>& obstacles,
    BattleSide side,
    bool isGateOpen
)
{
    for(const auto& obstacle : obstacles)
    {
        switch(obstacle->obstacleType)
        {
            case CObstacleInstance::USUAL:
            case CObstacleInstance::ABSOLUTE_OBSTACLE:
                statemask.reset(EU(HS::PASSABLE));
                break;

            case CObstacleInstance::MOAT:
                if(!(bhex == BattleHex::GATE_BRIDGE && isGateOpen))
                {
                    statemask.set(EU(HS::STOPPING));
                    statemask.set(EU(HS::DAMAGING_L));
                    statemask.set(EU(HS::DAMAGING_R));
                }
                break;

            case CObstacleInstance::SPELL_CREATED:
                switch(SpellID(obstacle->ID))
                {
                    case SpellID::QUICKSAND:
                        statemask.set(EU(HS::STOPPING));
                        break;

                    case SpellID::LAND_MINE:
                    {
                        auto casterSide =
                            dynamic_cast<const SpellCreatedObstacle*>(obstacle.get())->casterSide;

                        if(side == casterSide)
                            statemask.set(EU(side == BattleSide::DEFENDER ? HS::DAMAGING_L : HS::DAMAGING_R));
                        else
                            statemask.set(EU(side == BattleSide::DEFENDER ? HS::DAMAGING_R : HS::DAMAGING_L));

                        break;
                    }

                    default:
                        break;
                }
                break;

            default:
                THROW_FORMAT("Unexpected obstacle type: %d", EU(obstacle->obstacleType));
        }
    }

    switch(accessibility)
    {
        case EAccessibility::ACCESSIBLE:
            ASSERT(!cstack, "accessibility is ACCESSIBLE, but a stack was found on hex");
            statemask.set(EU(HS::PASSABLE));
            break;

        case EAccessibility::OBSTACLE:
        case EAccessibility::ALIVE_STACK:
        case EAccessibility::DESTRUCTIBLE_WALL:
        case EAccessibility::UNAVAILABLE:
            statemask.reset(EU(HS::PASSABLE));
            break;

        case EAccessibility::GATE:
            side == BattleSide::DEFENDER
                ? statemask.set(EU(HS::PASSABLE))
                : statemask.reset(EU(HS::PASSABLE));
            break;

        default:
            THROW_FORMAT(
                "Unexpected hex accessibility for bhex %d: %d",
                bhex.toInt(),
                EU(accessibility)
            );
    }

    if(bhex == BattleHex::GATE_INNER || bhex == BattleHex::GATE_OUTER)
        statemask.set(EU(HS::SIEGE_GATE));
    else if(bhex == BattleHex::GATE_BRIDGE)
        statemask.set(EU(HS::SIEGE_BRIDGE));
    else if(
        bhex == BattleHex::DESTRUCTIBLE_WALL_1 ||
        bhex == BattleHex::DESTRUCTIBLE_WALL_2 ||
        bhex == BattleHex::DESTRUCTIBLE_WALL_3 ||
        bhex == BattleHex::DESTRUCTIBLE_WALL_4
    )
        statemask.set(EU(HS::SIEGE_WALL));
}

}
