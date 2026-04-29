/*
 * edge.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/v15/graph.h"
#include "BAI/v15/encoder.h"

namespace MMAI::BAI::V15::Graph
{
namespace S15 = Schema::V15;

template <typename EncTraits, typename Interface>
class Element : public Interface
{
public:
    using Attribute = typename EncTraits::attr_type;

    // XXX: or the default constructor could set attrs to NULL_UNENCODED
    // XXX: can't delete the default constructor (subclasses cant define any consturctors then)
    // Element() = delete;

    S15::Graph::ElementType elementType() const override
    {
        return EncTraits::element_type;
    }

    std::vector<float> encodedAttributes() const override
    {
        return Encoder::Encode<EncTraits>(attrs);
    }

    // std::pair<S15::Graph::NodeType, S15::Graph::NodeType> getNodeTypes() const override
    // {
    //     return {S15::Graph::NodeType::HEX, S15::Graph::NodeType::HEX};
    // }

    // std::pair<int, int> getNodeIndexes() const override
    // {
    //     return nodeIndexes;
    // }

    int attr(Attribute a) const
    {
        return attrs.at(static_cast<size_t>(a));
    }

    void setattr(Attribute a, int value)
    {
        attrs.at(static_cast<size_t>(a)) = value;
    }

    std::array<int, EncTraits::attr_count> attrs = {};
    // std::pair<int, int> nodeIndexes;
};
}
