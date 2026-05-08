#pragma once

#include "AI/MMAI/common.h"
#include "BAI/v15/graph/nodes/base.h"
#include "schema/v15/graph.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;

namespace detail
{
    using Actaction_Traits = S15::EncodingTraits<S15::Graph::NodeAttributes::Actaction>;
    using Actaction_Base = Base<Actaction_Traits>;
}

class Actaction : public detail::Actaction_Base
{
public:
    explicit Actaction(int action)
    {
        setattr(A::ID, action);
        static_assert(EU(A::_count) == 1, "whistleblower in case attributes change");
    }
};
}
