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
#include "BAI/v15/graph/nodes/hex.h"
#include "BAI/v15/graph/nodes/unit.h"
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
public:
    struct extra_index_type {
        using result_type = std::pair<int, int>;
        result_type operator()(const std::shared_ptr<const Action> & action) const {
            return {action->id, action->by->cstack.unitId()};
        }
    };

    explicit Action(
        S15::ActionType actionType,
        int id,
        const std::shared_ptr<const Nodes::Unit> & by,
        const std::vector<std::shared_ptr<const Nodes::Hex>> & endsAt,
        bool isActive)
    : actionType(actionType), id(id), by(by), endsAt(endsAt), isActive(isActive)
    {}

    std::string name() const override
    {
        std::stringstream ss;
        // XXX: don't use EU(actionType) as it results in a nonprintable char
        ss << detail::Action_Base::name() << "(" << static_cast<int>(actionType) << "," << id << "," << by->cstack.unitId() << "," << isActive << ")";
        return ss.str();
    }

    const S15::ActionType actionType;
    const int id;
    const std::shared_ptr<const Nodes::Unit> by;
    const std::vector<std::shared_ptr<const Nodes::Hex>> endsAt; // primary hex is always first
    bool isActive;
};
}
