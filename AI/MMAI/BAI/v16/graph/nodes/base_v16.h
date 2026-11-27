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

namespace MMAI::BAI::V16::Graph::Nodes
{
namespace S16 = Schema::V16;

template<typename EncTraits>
class Base : public Element<S16::Graph::INode, EncTraits>
{
public:
	// bring names into scope
	// (needed due to dependent name lookup rules in C++ templates)
	using Element<S16::Graph::INode, EncTraits>::attrs;
	using Element<S16::Graph::INode, EncTraits>::setattr;
	using A = typename EncTraits::A;
	using extra_index_type = void;

	Base() = default;

	Base(const Base &) = delete;
	Base & operator=(const Base &) = delete;
	Base(Base &&) = delete;
	Base & operator=(Base &&) = delete;
};
}
