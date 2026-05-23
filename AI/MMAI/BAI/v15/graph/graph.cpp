#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "schema/v15/graph.h"

#include "BAI/v15/fastbfs.h"

namespace MMAI::BAI::V15::Graph
{

using ET = S15::Graph::ElementType;

Graph::Graph(const CPlayerBattleCallback & battle)
: battle(battle)
, accessibility(battle.getAccessibility())
, fastbfs(FastBFS(battle, accessibility))
{};


std::vector<const S15::Graph::INode*>
Graph::getNodes(Schema::V15::Graph::ElementType t) const
{
    auto convert = [](const auto & entries)
    {
        std::vector<const S15::Graph::INode*> res;
        res.reserve(entries.size());
        for (const auto & e : entries)
            res.push_back(e.get());
        return res;
    };

    switch (t)
    {
        case ET::NODE_GLOBAL:
            return convert(getAll<Nodes::Global>());
        case ET::NODE_PLAYER:
            return convert(getAll<Nodes::Player>());
        case ET::NODE_UNIT:
            return convert(getAll<Nodes::Unit>());
        case ET::NODE_HEX:
            return convert(getAll<Nodes::Hex>());
        case ET::NODE_ACTION:
            return convert(getAll<Nodes::Action>());
        default:
            throw std::runtime_error(
                "Unexpected node element type: " + std::to_string(EU(t))
            );
    }
}

std::vector<const S15::Graph::IEdge*>
Graph::getEdges(Schema::V15::Graph::ElementType t) const
{
    auto convert = [](const auto & entries)
    {
        std::vector<const S15::Graph::IEdge*> res;
        res.reserve(entries.size());
        for (const auto & e : entries)
            res.push_back(e.get());
        return res;
    };

    switch (t)
    {
        case ET::EDGE_GLOBAL_HAS_PLAYER:
            return convert(getAll<Edges::Global_Has_Player>());
        case ET::EDGE_GLOBAL_HAS_UNIT:
            return convert(getAll<Edges::Global_Has_Unit>());
        case ET::EDGE_GLOBAL_HAS_HEX:
            return convert(getAll<Edges::Global_Has_Hex>());
        case ET::EDGE_PLAYER_OWNS_UNIT:
            return convert(getAll<Edges::Player_Owns_Unit>());
        case ET::EDGE_HEX_ADJACENT_HEX:
            return convert(getAll<Edges::Hex_Adjacent_Hex>());
        case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
            return convert(getAll<Edges::Unit_ActsBefore_Unit>());
        case ET::EDGE_UNIT_MELEE_DMG_UNIT:
            return convert(getAll<Edges::Unit_MeleeDmg_Unit>());
        case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
            return convert(getAll<Edges::Unit_ShootDmg_Unit>());
        case ET::EDGE_UNIT_BLOCKS_UNIT:
            return convert(getAll<Edges::Unit_Blocks_Unit>());
        case ET::EDGE_UNIT_OCCUPIES_HEX:
            return convert(getAll<Edges::Unit_Occupies_Hex>());
        case ET::EDGE_ACTION_BY_UNIT:
            return convert(getAll<Edges::Action_By_Unit>());
        case ET::EDGE_ACTION_ENDS_AT_HEX:
            return convert(getAll<Edges::Action_EndsAt_Hex>());
        case ET::EDGE_ACTION_BLOCKS_UNIT:
            return convert(getAll<Edges::Action_Blocks_Unit>());
        case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
            return convert(getAll<Edges::Action_ExposesToMeleeFrom_Unit>());
        case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
            return convert(getAll<Edges::Action_ExposesToShootFrom_Unit>());
        case ET::EDGE_ACTION_MELEES_UNIT:
            return convert(getAll<Edges::Action_Melees_Unit>());
        case ET::EDGE_ACTION_SHOOTS_UNIT:
            return convert(getAll<Edges::Action_Shoots_Unit>());
        case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
            return convert(getAll<Edges::Action_EnablesMeleeAt_Unit>());
        case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
            return convert(getAll<Edges::Action_EnablesShootAt_Unit>());
        case ET::EDGE_ACTION_ENABLES_MELEE_AT_HEX:
            return convert(getAll<Edges::Action_EnablesMeleeAt_Hex>());
        case ET::EDGE_ACTION_ENABLES_SHOOT_AT_HEX:
            return convert(getAll<Edges::Action_EnablesShootAt_Hex>());
        default:
            throw std::runtime_error("Unexpected edge element type: " + std::to_string(EU(t)));
    }
}

std::vector<int> Graph::getActiveActionIds() const
{
    auto res = std::vector<int>{};
    int i = 0;
    for (const auto & action : getAll<Nodes::Action>())
    {
        if (action->isActive)
            res.push_back(i);
        ++i;
    }

    return res;
}


void Graph::verify() const
{
    auto _verify = [](const auto & entries)
    {
        for (const auto & e : entries)
            e->verify();
    };

    for (int i = 0; i < EU(ET::_count); ++i)
    {
        switch (ET(i))
        {
            case ET::NODE_GLOBAL:
                _verify(getAll<Nodes::Global>());
                break;
            case ET::NODE_PLAYER:
                _verify(getAll<Nodes::Player>());
                break;
            case ET::NODE_UNIT:
                _verify(getAll<Nodes::Unit>());
                break;
            case ET::NODE_HEX:
                _verify(getAll<Nodes::Hex>());
                break;
            case ET::NODE_ACTION:
                _verify(getAll<Nodes::Action>());
                break;
            case ET::EDGE_GLOBAL_HAS_PLAYER:
                _verify(getAll<Edges::Global_Has_Player>());
                break;
            case ET::EDGE_GLOBAL_HAS_UNIT:
                _verify(getAll<Edges::Global_Has_Unit>());
                break;
            case ET::EDGE_GLOBAL_HAS_HEX:
                _verify(getAll<Edges::Global_Has_Hex>());
                break;
            case ET::EDGE_PLAYER_OWNS_UNIT:
                _verify(getAll<Edges::Player_Owns_Unit>());
                break;
            case ET::EDGE_HEX_ADJACENT_HEX:
                _verify(getAll<Edges::Hex_Adjacent_Hex>());
                break;
            case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
                _verify(getAll<Edges::Unit_ActsBefore_Unit>());
                break;
            case ET::EDGE_UNIT_MELEE_DMG_UNIT:
                _verify(getAll<Edges::Unit_MeleeDmg_Unit>());
                break;
            case ET::EDGE_UNIT_SHOOT_DMG_UNIT:
                _verify(getAll<Edges::Unit_ShootDmg_Unit>());
                break;
            case ET::EDGE_UNIT_BLOCKS_UNIT:
                _verify(getAll<Edges::Unit_Blocks_Unit>());
                break;
            case ET::EDGE_UNIT_OCCUPIES_HEX:
                _verify(getAll<Edges::Unit_Occupies_Hex>());
                break;
            case ET::EDGE_ACTION_BY_UNIT:
                _verify(getAll<Edges::Action_By_Unit>());
                break;
            case ET::EDGE_ACTION_ENDS_AT_HEX:
                _verify(getAll<Edges::Action_EndsAt_Hex>());
                break;
            case ET::EDGE_ACTION_BLOCKS_UNIT:
                _verify(getAll<Edges::Action_Blocks_Unit>());
                break;
            case ET::EDGE_ACTION_EXPOSES_TO_MELEE_FROM_UNIT:
                _verify(getAll<Edges::Action_ExposesToMeleeFrom_Unit>());
                break;
            case ET::EDGE_ACTION_EXPOSES_TO_SHOOT_FROM_UNIT:
                _verify(getAll<Edges::Action_ExposesToShootFrom_Unit>());
                break;
            case ET::EDGE_ACTION_MELEES_UNIT:
                _verify(getAll<Edges::Action_Melees_Unit>());
                break;
            case ET::EDGE_ACTION_SHOOTS_UNIT:
                _verify(getAll<Edges::Action_Shoots_Unit>());
                break;
            case ET::EDGE_ACTION_ENABLES_MELEE_AT_UNIT:
                _verify(getAll<Edges::Action_EnablesMeleeAt_Unit>());
                break;
            case ET::EDGE_ACTION_ENABLES_SHOOT_AT_UNIT:
                _verify(getAll<Edges::Action_EnablesShootAt_Unit>());
                break;
            case ET::EDGE_ACTION_ENABLES_MELEE_AT_HEX:
                _verify(getAll<Edges::Action_EnablesMeleeAt_Hex>());
                break;
            case ET::EDGE_ACTION_ENABLES_SHOOT_AT_HEX:
                _verify(getAll<Edges::Action_EnablesShootAt_Hex>());
                break;
            default:
                throw std::runtime_error("verify: unexpected edge element type: " + std::to_string(i));
        }
    }
}

EnumFlags<S15::Graph::ElementType> Graph::getFlags() const
{
    return flags;
}

void Graph::setFlag(S15::Graph::ElementType et)
{
    flags.set(et);
}

const AccessibilityInfo & Graph::getAccessibility() const
{
    return accessibility;
}

const FastBFS & Graph::getFastBFS() const
{
    return fastbfs;
}

} // namespace
