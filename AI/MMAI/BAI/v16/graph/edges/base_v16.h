/*
 * base.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "AI/MMAI/common.h" // IWYU pragma: keep

#include "BAI/v16/graph/element_v16.h"
#include "schema/v16/graph.h"

namespace MMAI::BAI::V16::Graph::Edges
{
namespace S16 = Schema::V16;

template<typename SrcNode, typename DstNode, typename EncTraits>
class Base : public Element<S16::Graph::IEdge, EncTraits>
{
	using Base_Base = Element<S16::Graph::IEdge, EncTraits>;

public:
	using src_node_type = SrcNode;
	using dst_node_type = DstNode;

	// bring names into scope
	// (needed due to dependent name lookup rules in C++ templates)
	using Base_Base::attrs;
	using Base_Base::setattr;
	using A = typename EncTraits::A;

	Base(const std::shared_ptr<const SrcNode> & srcNode, const std::shared_ptr<const DstNode> & dstNode) : Base_Base(), srcNode(srcNode), dstNode(dstNode) {}

	Base(const Base &) = delete;
	Base & operator=(const Base &) = delete;
	Base(Base &&) = delete;
	Base & operator=(Base &&) = delete;

	std::string name() const override
	{
		std::stringstream ss;
		// ss << Base_Base::name() << "[" << srcNode->name() << "/" << srcNode.get() << "->" << dstNode->name() << "/" <<dstNode.get() <<"]";
		ss << Base_Base::name() << "[" << srcNode->name() << " -> " << dstNode->name() << "]";
		return ss.str();
	}

	Schema::V16::Graph::Endpoints endpoints() const override
	{
		return {srcNode.get(), dstNode.get()};
	}

	const std::shared_ptr<const SrcNode> srcNode;
	const std::shared_ptr<const DstNode> dstNode;
};
}
