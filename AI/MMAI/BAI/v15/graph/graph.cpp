#include "BAI/v15/graph/graph.h"
#include "BAI/v15/fastbfs.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph
{

Graph::Graph(const CPlayerBattleCallback & battle)
: accessibility(battle.getAccessibility())
, fastbfs(FastBFS(battle, accessibility))
{};

std::vector<const S15::Graph::INode*>
Graph::getNodes(Schema::V15::Graph::ElementType t) const
{
    return withNodeStore(t, [](const auto & store)
    {
        std::vector<const S15::Graph::INode*> res;
        res.reserve(store.size());
        for (const auto & e : store.entries())
            res.push_back(e.get());
        return res;
    });
}

std::vector<const S15::Graph::IEdge*>
Graph::getEdges(Schema::V15::Graph::ElementType t) const
{
    return withEdgeStore(t, [](const auto & store)
    {
        std::vector<const S15::Graph::IEdge*> res;
        res.reserve(store.size());
        for (const auto & e : store.entries())
            res.push_back(e.get());
        return res;
    });
}

int64_t Graph::getNodeIndex(const S15::Graph::INode* inode) const
{
    return withNodeStore(inode->getType(), [&](const auto & store)
    {
        using Store = std::decay_t<decltype(store)>;
        using Node = typename Store::node_type;

        // Downcast INode* to the real node, e.g. Nodes::Player*
        const auto* node = dynamic_cast<const Node*>(inode);
        if (!node)
            throw std::runtime_error("Node type does not match element type: " + std::to_string(EU(inode->getType())));

        return store.getId(node);
    });
}

int64_t Graph::getEdgeIndex(const S15::Graph::IEdge* iedge) const
{
    return withEdgeStore(iedge->getType(), [&](const auto & store)
    {
        using Store = std::decay_t<decltype(store)>;
        using Edge = typename Store::edge_type;

        // Downcast INode* to the real node, e.g. Nodes::Player*
        const auto* edge = dynamic_cast<const Edge*>(iedge);
        if (!edge)
            throw std::runtime_error("Node type does not match element type: " + std::to_string(EU(iedge->getType())));

        return store.getId(edge);
    });
}

std::vector<int64_t> Graph::getActiveActionIds() const
{
    auto res = std::vector<int64_t>{};
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
    for (int i = 0; i < EU(ET::_count); ++i)
    {
        auto et = ET(i);
        switch (et)
        {
            case ET::NODE_GLOBAL:
            case ET::NODE_PLAYER:
            case ET::NODE_UNIT:
            case ET::NODE_HEX:
            case ET::NODE_ACTION:
                withNodeStore(et, [](const auto & store)
                {
                    for (const auto & node : store.entries())
                        node->verify();
                });
                break;
            // All other types are edges
            default:
                withEdgeStore(et, [](const auto & store)
                {
                    for (const auto & edge : store.entries())
                        edge->verify();
                });
                break;
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
