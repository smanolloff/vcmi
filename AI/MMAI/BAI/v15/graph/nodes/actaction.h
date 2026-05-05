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
    using A = S15::Graph::NodeAttributes::Actaction;
    using CombatResult = Schema::V15::CombatResult;
    using TowerFlags = std::bitset<3>;
    using CorpseFlags = std::bitset<2>;

public:
    explicit Actaction(int action)
    {
        setattr(A::ID, action);
        static_assert(EU(A::_count) == 1, "whistleblower in case attributes change");
    }
};
}
