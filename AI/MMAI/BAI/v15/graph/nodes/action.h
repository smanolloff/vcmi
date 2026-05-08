/*
 * action.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

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
    using Action_Traits = S15::EncodingTraits<S15::Graph::NodeAttributes::Action>;
    using Action_Base = Base<Action_Traits>;
}

class Action : public detail::Action_Base
{
    explicit Action(S15::ActionType actionType)
    {
        setattr(A::TYPE, EU(actionType));
        static_assert(EU(A::_count) == 1, "whistleblower in case attributes change");
    }
};
}
