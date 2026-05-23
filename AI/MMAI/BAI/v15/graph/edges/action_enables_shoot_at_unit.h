#pragma once

#include "BAI/v15/graph/util.h"
#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/action.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
    using Action_EnablesShootAt_Unit_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Action_EnablesShootAt_Unit>;
    using Action_EnablesShootAt_Unit_Base = Base<Nodes::Action, Nodes::Unit, Action_EnablesShootAt_Unit_Traits>;
}

class Action_EnablesShootAt_Unit : public detail::Action_EnablesShootAt_Unit_Base
{
public:
    static std::shared_ptr<const Action_EnablesShootAt_Unit> Create(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Unit> & dstNode,
        float mult)
    {
        return std::make_shared<const Action_EnablesShootAt_Unit>(srcNode, dstNode, mult);
    };

    Action_EnablesShootAt_Unit(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Unit> & dstNode,
        float mult)
    : detail::Action_EnablesShootAt_Unit_Base(srcNode, dstNode)
    , mult(mult)
    {
        setattr(A::DMG_MULT, permille(mult, 1));
        static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
    }

    const float mult;
};
}
