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
#include "BAI/v15/graph/element.h"
#include "schema/v15/graph.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Nodes
{
namespace S15 = Schema::V15;
using AA = S15::Graph::NodeAttributes::Action;
using CombatResult = Schema::V15::CombatResult;
using TowerFlags = std::bitset<3>;
using CorpseFlags = std::bitset<2>;

class Action : public Element<S15::EncodingTraits<Schema::V15::ActionEncoding>, S15::Graph::INode>
{
public:
    explicit Action(int index, int action)
    : index_(index)
    {
        attrs.fill(S15::NULL_VALUE_UNENCODED);

        setattr(AA::ID, action);
        static_assert(EU(AA::_count) == 1, "whistleblower in case attributes change");
    }

    int nodeIndex() const override {
        return index_;
    }

    void addattr(PA a, int value)
    {
        attrs.at(EU(a)) += value;
    }
private:
    std::array<int, EU(PA::_count)> attrs = {};
    const int index_;
};
}
