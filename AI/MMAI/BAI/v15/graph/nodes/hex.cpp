#include "BAI/v15/graph/nodes/hex.h"

#include "AI/MMAI/common.h"
#include "schema/v15/types.h"
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

Hex::Hex(const Args & args)
: bhex(args.bhex)
, id(CalcId(args.bhex))
{
    auto [x, y] = CalcXY(bhex);

    guardflags.set(); // See note for attr()/setattr() in Hex.h

    setattr(A::Y_COORD, y);
    setattr(A::X_COORD, x);
    setattr(A::WALL_HEALTH, EU(args.wallHP));
    setStateMask(args.accessibility, args.obstacles, args.side, args.isGateOpen);
}


std::string Hex::name() const
{
    std::stringstream ss;
    ss << detail::Hex_Base::name() << "(" << attr(A::Y_COORD) << "," << attr(A::X_COORD) << ")";
    return ss.str();
}

void Hex::setStateMask(
    EAccessibility accessibility,
    const std::vector<std::shared_ptr<const CObstacleInstance>>& obstacles,
    BattleSide side,
    bool isGateOpen
)
{
    for(const auto& obstacle : obstacles)
        setMoatFlags(obstacle.get(), isGateOpen, side);

    switch(accessibility)
    {
        case EAccessibility::ACCESSIBLE:
            setattr(A::IS_PASSABLE, 1);
            break;
        case EAccessibility::OBSTACLE:
        case EAccessibility::UNAVAILABLE:
            setattr(A::IS_OBSTACLE, 1);
            break;
        case EAccessibility::GATE:
            setattr(A::IS_PASSABLE, side == BattleSide::DEFENDER);
            break;
        case EAccessibility::ALIVE_STACK:
        case EAccessibility::DESTRUCTIBLE_WALL:
            break; // nothing to set
        default:
            THROW_FORMAT("Unexpected hex accessibility for bhex %d: %d", bhex.toInt() % EU(accessibility));
    }

    if(bhex == BattleHex::GATE_INNER || bhex == BattleHex::GATE_OUTER)
        setattr(A::IS_SIEGE_GATE, 1);
    else if(bhex == BattleHex::GATE_BRIDGE)
        setattr(A::IS_SIEGE_BRIDGE, 1);
}

void Hex::setMoatFlags(
    const CObstacleInstance * obstacle,
    bool isGateOpen,
    BattleSide side)
{
    switch(obstacle->obstacleType)
    {
        case CObstacleInstance::MOAT:
            if(!(bhex == BattleHex::GATE_BRIDGE && isGateOpen))
            {
                setattr(A::IS_STOPPING, 1);
                setattr(A::IS_DAMAGING_L, 1);
                setattr(A::IS_DAMAGING_R, 1);
            }
            break;

        case CObstacleInstance::SPELL_CREATED:
            switch(SpellID(obstacle->ID))
            {
                case SpellID::QUICKSAND:
                    setattr(A::IS_STOPPING, 1);
                    break;

                case SpellID::LAND_MINE:
                    if(side == dynamic_cast<const SpellCreatedObstacle*>(obstacle)->casterSide)
                        setattr(side == BattleSide::DEFENDER ? A::IS_DAMAGING_L : A::IS_DAMAGING_R, 1);
                    else
                        setattr(side == BattleSide::DEFENDER ? A::IS_DAMAGING_R : A::IS_DAMAGING_L, 1);
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

int Hex::attr(Attribute a) const
{
    return attrs.at(EU(a));
}

void Hex::setattr(Attribute a, int value)
{
    attrs.at(EU(a)) = value;
}

}
