/*
 * generic.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

// Generic edges are edges without any attributes

class GenericEdge : public S15::Graph::IEdge
{
    using ET = S15::Graph::ElementType;

    explicit GenericEdge(ET edgeType, ET srcType, ET dstType, int srcIndex, int dstIndex)
    : edgeType(edgeType)
    , srcType(srcType)
    , dstType(dstType)
    , srcIndex(srcIndex)
    , dstIndex(dstIndex)
    {};

    ET elementType() const override { return edgeType; }
    std::vector<float> encodedAttributes() const override { return {}; }
    std::pair<ET, ET> nodeTypes() const override { return {srcType, dstType}; }
    std::pair<int, int> nodeIndexes() const override { return {srcIndex, dstIndex}; }
private:
    const S15::Graph::ElementType edgeType;
    const S15::Graph::ElementType srcType;
    const S15::Graph::ElementType dstType;
    const int srcIndex;
    const int dstIndex;
};
}
