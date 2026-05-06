#pragma once

#include "BAI/v15/graph/element.h"
#include "schema/v15/graph.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

template <typename SrcNode, typename DstNode, typename EncTraits>
class Base : public Element<S15::Graph::IEdge, EncTraits>
{
public:
    using src_node_type = SrcNode;
    using dst_node_type = DstNode;

    // bring names into scope
    // (needed due to dependent name lookup rules in C++ templates)
    using Element<S15::Graph::IEdge, EncTraits>::attrs;
    using Element<S15::Graph::IEdge, EncTraits>::setattr;
    using A = typename EncTraits::A;

    Base(
        const std::shared_ptr<const SrcNode> & srcNode,
        const std::shared_ptr<const DstNode> & dstNode
    ) : srcNode(srcNode), dstNode(dstNode), Element<S15::Graph::IEdge, EncTraits>()
    {}

    Base(const Base &) = delete;
    Base & operator=(const Base &) = delete;
    Base(Base &&) = delete;
    Base & operator=(Base &&) = delete;

    Schema::V15::Graph::Endpoints endpoints() const override
    {
        return {&srcNode, &dstNode};
    }

    const std::shared_ptr<const SrcNode> & srcNode;
    const std::shared_ptr<const DstNode> & dstNode;
};
}
