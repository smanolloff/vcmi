#pragma once

#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/action.h"
#include "BAI/v15/graph/nodes/hex.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
    using Action_EnablesShootAt_Hex_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Action_EnablesShootAt_Hex>;
    using Action_EnablesShootAt_Hex_Base = Base<Nodes::Action, Nodes::Hex, Action_EnablesShootAt_Hex_Traits>;
}

class Action_EnablesShootAt_Hex : public detail::Action_EnablesShootAt_Hex_Base
{
public:
    static std::shared_ptr<const Action_EnablesShootAt_Hex> Create(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Hex> & dstNode,
        float mult)
    {
        return std::make_shared<const Action_EnablesShootAt_Hex>(srcNode, dstNode, mult);
    };

    Action_EnablesShootAt_Hex(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Hex> & dstNode,
        float mult)
    : detail::Action_EnablesShootAt_Hex_Base(srcNode, dstNode)
    , mult(mult)
    {
        setattr(A::DMG_MULT, permille(mult, 1));
        static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
    }

    const float mult;
};
}
