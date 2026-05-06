#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/action.h"
#include "BAI/v15/graph/nodes/hex.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

namespace detail
{
    using Action_EndsAt_Hex_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Action_EndsAt_Hex>;
    using Action_EndsAt_Hex_Base = Base<Nodes::Action, Nodes::Hex, Action_EndsAt_Hex_Traits>;
}

class Action_EndsAt_Hex : public detail::Action_EndsAt_Hex_Base
{
public:
    Action_EndsAt_Hex(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Hex> & dstNode,
        bool isRear = false
    ) : detail::Action_EndsAt_Hex_Base(srcNode, dstNode)
    {
        setattr(A::IS_REAR, isRear);
        static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
    }
};

namespace detail
{
    using Actaction_EndsAt_Hex_Traits = S15::EncodingTraits<S15::Graph::EdgeAttributes::Actaction_EndsAt_Hex>;
    using Actaction_EndsAt_Hex_Base = Base<Nodes::Action, Nodes::Hex, Actaction_EndsAt_Hex_Traits>;
}

class Actaction_EndsAt_Hex : public detail::Actaction_EndsAt_Hex_Base
{
public:
    Actaction_EndsAt_Hex(
        const std::shared_ptr<const Nodes::Action> & srcNode,
        const std::shared_ptr<const Nodes::Hex> & dstNode,
        bool isRear = false
    ) : detail::Actaction_EndsAt_Hex_Base(srcNode, dstNode)
    {
        setattr(A::IS_REAR, isRear);
        static_assert(static_cast<size_t>(A::_count) == 1, "whistleblower in case attributes change");
    }
};
}
