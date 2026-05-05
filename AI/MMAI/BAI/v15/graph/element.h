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

#include "schema/v15/constants.h"
#include "schema/v15/graph.h"
#include "BAI/v15/encoder.h"

namespace MMAI::BAI::V15::Graph
{
namespace S15 = Schema::V15;

template <typename Interface, typename EncTraits>
class Element : public Interface
{
public:
    using encoding_traits = EncTraits;
    using Attribute = typename EncTraits::A;

    S15::Graph::ElementType elementType() const override { return EncTraits::element_type; }
    std::vector<float> encodedAttributes() const override { return Encoder::Encode<EncTraits>(attrs); }

    Element()
    {
        attrs.fill(S15::NULL_VALUE_UNENCODED);
    }

    int attr(Attribute a) const
    {
        ASSERT(guardflags.test(EU(a)), EncTraits::name + ": attribute not set: " + std::to_string(EU(a)));
        return attrs.at(EU(a));
    }

    void setattr(Attribute a, int value)
    {
        ASSERT(!guardflags.test(EU(a)), EncTraits::name + ": attribute already set: " + std::to_string(EU(a)));
        guardflags.set(EU(a));
        attrs.at(EU(a)) = value;
    }

    void addattr(Attribute a, int value)
    {
        ASSERT(guardflags.test(EU(a)), EncTraits::name + ": attribute not set: " + std::to_string(EU(a)));
        attrs.at(EU(a)) += value;
    }

    std::array<int, EncTraits::attr_count> attrs = {};
    std::bitset<EncTraits::attr_count> guardflags;
};
}
