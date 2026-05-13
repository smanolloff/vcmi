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
    const BattleHex & bhex,
    const EAccessibility accessibility,
    const BattleSide side,
    const std::vector<std::shared_ptr<const CObstacleInstance>> & obstacles,
    int wallHP,
    bool isGateOpen
)
    : bhex(bhex)
    , id(CalcId(bhex))
{
    auto [x, y] = CalcXY(bhex);

    setattr(HA::Y_COORD, y);
    setattr(HA::X_COORD, x);
    setattr(HA::WALL_HEALTH, wallHP);
    setStateMask(accessibility, obstacles, side, isGateOpen);
    finalize();
}


std::string Hex::name() const
{
    std::stringstream ss;
    ss << detail::Hex_Base::name() << "(" << attr(HA::Y_COORD) << "," << attr(HA::X_COORD) << ")";
    return ss.str();
}

void Hex::finalize()
{
    attrs.at(EU(HA::STATE_MASK)) = static_cast<int>(statemask.to_ulong());
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
                statemask.set(EU(HS::OBSTACLE));
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
            statemask.set(EU(HS::PASSABLE));
            break;

        case EAccessibility::OBSTACLE:
        case EAccessibility::UNAVAILABLE:
            statemask.set(EU(HS::OBSTACLE));
            // no break
        case EAccessibility::ALIVE_STACK:
        case EAccessibility::DESTRUCTIBLE_WALL:
            statemask.reset(EU(HS::PASSABLE));
            break;

        case EAccessibility::GATE:
            side == BattleSide::DEFENDER
                ? statemask.set(EU(HS::PASSABLE))
                : statemask.reset(EU(HS::PASSABLE));
            break;

        default:
            THROW_FORMAT("Unexpected hex accessibility for bhex %d: %d", bhex.toInt() % EU(accessibility));
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
