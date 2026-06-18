#include "BAI/v15/graph/nodes/hex.h"

#include "AI/MMAI/common.h"
#include "vcmi/spells/Service.h"
#include "vcmi/spells/Spell.h"
#include "lib/spells/CSpellHandler.h"

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

    for(const auto& obstacle : args.obstacles)
        setMoatFlags(obstacle.get(), args.isGateOpen, args.side, args.hasNativeStack);

    setattr(A::Y_COORD, y);
    setattr(A::X_COORD, x);
    setattr(A::WALL_HEALTH, EU(args.wallHP));

    if (!args.isSiege)
    {
        setattr(A::IS_SIEGE_GATE, 0);
        setattr(A::IS_SIEGE_BRIDGE, 0);
    }
    else if (bhex == BattleHex::GATE_INNER || bhex == BattleHex::GATE_OUTER)
            setattr(A::IS_SIEGE_GATE, 1);
    else if(bhex == BattleHex::GATE_BRIDGE)
        setattr(A::IS_SIEGE_BRIDGE, 1);

    switch(args.accessibility)
    {
        case EAccessibility::ACCESSIBLE:
            setattr(A::IS_PASSABLE, 1);
            break;
        case EAccessibility::OBSTACLE:
        case EAccessibility::UNAVAILABLE:
            setattr(A::IS_OBSTACLE, 1);
            break;
        case EAccessibility::GATE:
            setattr(A::IS_PASSABLE, args.side == BattleSide::DEFENDER);
            break;
        case EAccessibility::ALIVE_STACK:
        case EAccessibility::DESTRUCTIBLE_WALL:
            break; // nothing to set
        default:
            THROW_FORMAT("Unexpected hex accessibility for bhex %d: %d", bhex.toInt() % EU(args.accessibility));
    }
}


std::string Hex::name() const
{
    std::stringstream ss;
    ss << detail::Hex_Base::name() << "(y=" << attr(A::Y_COORD) << ",x=" << attr(A::X_COORD) << ")";
    return ss.str();
}

void Hex::setMoatFlags(
    const CObstacleInstance * obstacle,
    bool isGateOpen,
    BattleSide side,
    bool hasNativeStack)
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
        {
            const auto * so = dynamic_cast<const SpellCreatedObstacle*>(obstacle);
            // const bool visible = so->visibleForSide(side, hasNativeStack);

            // XXX: this does NOT work for town land mine? (it has ID 95 i think -- not mapped)
            // TODO: check if the land mines are static and add a constant accordingly?
            // XXX: this works for QUICKSAND (ID is 10 - checked)
            // TODO: does it work for LAND_MINE (regular cast)?
            //
            if (so->stopsMovement())
                    setattr(A::IS_STOPPING, 1);

            // switch(SpellID(obstacle->ID))
            // {
            //     case SpellID::QUICKSAND:
            //         setattr(A::IS_STOPPING, 1);
            //         statemask |= S_STOPPING;
            //         break;


            // XXX: for quicksand, this should be STOPPING only for opponent
            //      for regular moats, this should be STOPPING for everyone
            //      How to check which side is affected?

            // XXX: Quicksand has no trigger => this fails
            // (it is just an invisible trap)
            const CSpell * spell = so->trigger.toSpell();
            if (spell->identifier == "landMineTrigger")
            {
                if(side == so->casterSide)
                    setattr(side == BattleSide::DEFENDER ? A::IS_DAMAGING_L : A::IS_DAMAGING_R, 1);
                else
                    setattr(side == BattleSide::DEFENDER ? A::IS_DAMAGING_R : A::IS_DAMAGING_L, 1);
            }

            break;
        }
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
