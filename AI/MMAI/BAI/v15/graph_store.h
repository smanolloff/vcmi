/*
 * edge_store.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/v15/graph.h"

namespace MMAI::BAI::V15
{

template <typename T>
class NodeStore
{
public:
    explicit NodeStore();
    void add(T node);

private:
    std::vector<T> nodes;
};

template <typename T>
class EdgeStore
{
public:
    explicit EdgeStore();
    void add(T edge, int srcind, int dstind);

private:
    std::vector<T> edges;
    std::pair<std::vector<int>, std::vector<int>> edgeIndex;
};

namespace NA = Schema::V15::Graph::NodeAttributes;
namespace EA = Schema::V15::Graph::EdgeAttributes;

class GraphStore
{
public:
    explicit GraphStore();

    template <typename T>
    void addNode(T node);

    template <typename T>
    NodeStore<T> & getNodes();

    template <typename T>
    void addEdge(T edge, int srcind, int dstind);

    template <typename T>
    EdgeStore<T> & getEdges();

private:
    std::tuple<
        NodeStore<NA::Global>,
        NodeStore<NA::Player>,
        NodeStore<NA::Unit>,
        NodeStore<NA::Hex>,
        NodeStore<NA::Action>
    > nodes;

    std::tuple<
        EdgeStore<EA::Action_ExposesTo_Unit>,
        EdgeStore<EA::Action_Threatens_Unit>,
        EdgeStore<EA::Action_Damages_Unit>,
        EdgeStore<EA::Action_EndsAt_Hex>,
        EdgeStore<EA::Action_By_Unit>,
        EdgeStore<EA::Unit_Blocks_Unit>,
        EdgeStore<EA::Unit_MeleeDmg_Unit>,
        EdgeStore<EA::Unit_RangedDmg_Unit>,
        EdgeStore<EA::Unit_CanMelee_Unit>,
        EdgeStore<EA::Unit_CanShoot_Unit>,
        EdgeStore<EA::Unit_ActsBefore_Unit>,
        EdgeStore<EA::Unit_Threatens_Hex>,
        EdgeStore<EA::Unit_Occupies_Hex>,
        EdgeStore<EA::Hex_Adjacent_Hex>
    > edges;
};

}
