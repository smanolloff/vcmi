#include "BAI/v15/graph/graph.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph
{

using ET = S15::Graph::ElementType;

Graph::Graph(const CPlayerBattleCallback & battle) : battle(battle)
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
        case ET::EDGE_ACTION_BLOCKS_UNIT:
            return convert(getAll<Edges::Action_Blocks_Unit>());
        case ET::EDGE_ACTION_ENDS_AT_HEX:
            return convert(getAll<Edges::Action_EndsAt_Hex>());
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

std::vector<std::tuple<int, int>> Graph::getActiveNodeToActionIds() const
{
    std::vector<std::tuple<int, int>> res{};
    res.reserve(50); // educated guess for a unit's average number of actions

    int i = 0;
    for (const auto & action : getAll<Nodes::Action>())
    {
        if (action->id >= 0)
            res.emplace_back(i, action->id);
        ++i;
    }

    return res;
}

EnumFlags<S15::Graph::ElementType> Graph::getFlags() const
{
    return flags;
}

void Graph::setFlag(S15::Graph::ElementType et)
{
    flags.set(et);
}

// XXX: the only difference between regular and per-stack accessibility
// is that the stack's own hexes are ACCESSIBLE instead of ALIVE_STACK
// For the purposes of MMAI observation we always want them as ALIVE_STACK
const AccessibilityInfo & Graph::getAccessibility() const
{
    if (!acache)
        throw std::runtime_error("getAccessibility: cache not built");

    return *acache;
}

void Graph::buildAccessibilityCache()
{
    ASSERT(!haveAccessibilityCache, "getAccessibility: cache already built");
    haveAccessibilityCache = true;
    acache = std::make_unique<AccessibilityInfo>(battle.getAccessibility());
}


const ReachabilityInfo & Graph::getReachability(const CStack & cstack) const
{
    ASSERT(haveAccessibilityCache, "getReachability: cache not built");

    const auto & it = rcache.find(cstack.unitId());
    if(it == rcache.end())
        throw std::runtime_error("getReachability: could not find entry for cstack");

    return it->second;
}

void Graph::buildReachabilityCache()
{
    ASSERT(!haveReachabilityCache, "buildReachabilityCache: cache already built");
    haveReachabilityCache = true;

    flags.require(ET::NODE_UNIT);

    for (const auto & unit : getAll<Nodes::Unit>()) {
        const auto & cstack = unit->cstack;
        const auto & rinfo = battle.getReachability(&cstack);
        rcache.try_emplace(cstack.unitId(), rinfo);
    }

    if (rcache.empty())
    {
        // This should only happen on an empty battlefield (draw?)
        // => throw only if there are units still alive
        for (const auto * cstack : battle.battleGetAllStacks(false))
            if (cstack->alive())
                throw std::runtime_error("buildReachabilityCache: graph contains no units");
    }
}

} // namespace
