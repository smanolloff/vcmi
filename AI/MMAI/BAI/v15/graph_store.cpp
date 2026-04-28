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

#include "graph_store.h"

namespace MMAI::BAI::V15
{

GraphStore::GraphStore() {};

template <typename T>
void GraphStore::addNode(T node)
{
    std::get<NodeStore<T>>(nodes).add(std::move(node));
}

template <typename T>
NodeStore<T> & GraphStore::getNodes()
{
    return std::get<NodeStore<T>>(nodes);
}

template <typename T>
void GraphStore::addEdge(T edge, int srcind, int dstind)
{
    std::get<EdgeStore<T>>(edges).add(std::move(edge), srcind, dstind);
}

template <typename T>
EdgeStore<T> & GraphStore::getEdges()
{
    return std::get<EdgeStore<T>>(edges);
}

}
