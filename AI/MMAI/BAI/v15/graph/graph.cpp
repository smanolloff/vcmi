#include "BAI/v15/graph/graph.h"
#include "BAI/v15/graph/edges/generic.h"
#include "battle/ReachabilityInfo.h"
#include "schema/v15/graph.h"

#include "BAI/v15/fastbfs.h"

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

std::vector<std::tuple<int, int>> Graph::getActiveNodeToActionIds() const
{
    std::vector<std::tuple<int, int>> res{};
    res.reserve(50); // educated guess for a unit's average number of actions

    int i = 0;
    for (const auto & action : getAll<Nodes::Action>())
    {
        if (action->isActive)
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

    bool hasWideMoat = vstd::contains_if(battle.battleGetAllObstaclesOnPos(BattleHex(BattleHex::GATE_BRIDGE), false), [](const std::shared_ptr<const CObstacleInstance> & obst)
    {
        return obst->obstacleType == CObstacleInstance::MOAT;
    });

    auto fastbfs = FastBFS(battle, getAccessibility(), hasWideMoat);

    for (const auto & unit : getAll<Nodes::Unit>()) {
        const auto & cstack = unit->cstack;
        const auto & rinfo = battle.getReachability(&cstack);

        // DEBUG
        auto distances = fastbfs.run(
            cstack.getPosition(),
            cstack.getPosition(),
            cstack.unitSide(),
            unit->isFlying,
            cstack.doubleWide(),
            unit->speed
        );

        for (int i = 0; i < rinfo.distances.size(); ++i)
        {
            const auto a = rinfo.distances[i];
            const auto b = distances.at(i);

            if (unit->isFlying)
            {
                // full battlefield is computed for flyers
                if (a == ReachabilityInfo::INFINITE_DIST)
                {
                    if (b != FastBFS::INFINITE_DIST)
                    {
                        battle.getReachability(&cstack);
                        fastbfs.run(cstack.getPosition(), cstack.getPosition(), cstack.unitSide(), unit->isFlying, cstack.doubleWide(), unit->speed);
                        auto fastbfs2 = FastBFS(battle, getAccessibility(), hasWideMoat);
                    }
                }
                else
                {
                    if (a != b)
                    {
                        battle.getReachability(&cstack);
                        fastbfs.run(cstack.getPosition(), cstack.getPosition(), cstack.unitSide(), unit->isFlying, cstack.doubleWide(), unit->speed);
                        auto fastbfs2 = FastBFS(battle, getAccessibility(), hasWideMoat);
                    }
                }

                // a == ReachabilityInfo::INFINITE_DIST
                //     ? ASSERT(b == FastBFS::INFINITE_DIST, "bfs mismatch")
                //     : ASSERT(a == b, "bfs mismatch");
            }
            else
            {
                if (a > unit->speed)
                {
                    if (b != FastBFS::INFINITE_DIST)
                    {
                        battle.getReachability(&cstack);
                        fastbfs.run(cstack.getPosition(), cstack.getPosition(), cstack.unitSide(), unit->isFlying, cstack.doubleWide(), unit->speed);
                        auto fastbfs2 = FastBFS(battle, getAccessibility(), hasWideMoat);
                        std::cout << "y";
                    }
                }
                else
                {
                    if (a != b)
                    {
                        battle.getReachability(&cstack);
                        fastbfs.run(cstack.getPosition(), cstack.getPosition(), cstack.unitSide(), unit->isFlying, cstack.doubleWide(), unit->speed);
                        auto fastbfs2 = FastBFS(battle, getAccessibility(), hasWideMoat);
                        std::cout << "x";
                    }
                }

                // a > unit->speed
                //     ? ASSERT(b == FastBFS::INFINITE_DIST, "bfs mismatch")
                //     : ASSERT(a == b, "bfs mismatch");
            }
        }
        // /DEBUG

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
