#pragma once

#include "BAI/v15/graph/element.h"
#include "schema/v15/constants.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

template <typename SrcNode, typename DstNode, typename EncTraits>
class Base : public Element<S15::Graph::IEdge, EncTraits>
{
public:
    // bring names into scope
    // (needed due to dependent name lookup rules in C++ templates)
    using Element<S15::Graph::IEdge, EncTraits>::attrs;
    using Element<S15::Graph::IEdge, EncTraits>::setattr;
    using A = typename EncTraits::attr_type;

    Base(
        const SrcNode & srcNode,
        const DstNode & dstNode
    ) : srcNode(srcNode), dstNode(dstNode)
    {
        attrs.fill(S15::NULL_VALUE_UNENCODED);
    }

    Schema::V15::Graph::Endpoints endpoints() const override
    {
        return {srcNode.get(), dstNode.get()};
    }

    const SrcNode & srcNode;
    const DstNode & dstNode;
};
}
