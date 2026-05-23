#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/action.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
    using Action_Melees_Unit_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Action_Melees_Unit>;
    using Action_Melees_Unit_Base = Base<Nodes::Action, Nodes::Unit, Action_Melees_Unit_Traits>;
}

class Action_Melees_Unit : public detail::Action_Melees_Unit_Base
{
public:
    static std::shared_ptr<const Action_Melees_Unit> Create(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Unit> & dstNode,
        bool isPrimaryTarget)
    {
        return std::make_shared<const Action_Melees_Unit>(srcNode, dstNode, isPrimaryTarget);
    };

    Action_Melees_Unit(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Unit> & dstNode,
        bool isPrimaryTarget)
    : detail::Action_Melees_Unit_Base(srcNode, dstNode)
    , isPrimaryTarget(isPrimaryTarget)
    {
        setattr(A::IS_PRIMARY_TARGET, isPrimaryTarget);
        static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
    }

    const bool isPrimaryTarget;
};
}
