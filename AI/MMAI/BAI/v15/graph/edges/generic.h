/*
 * generic.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/edges/base.h"
#include "BAI/v15/graph/nodes/hex.h"
#include "BAI/v15/graph/nodes/unit.h"
#include "BAI/v15/graph/nodes/action.h"
#include "schema/v15/constants.h"

namespace MMAI::BAI::V15::Graph::Edges
{
namespace S15 = Schema::V15;

/*
 * The macro is useful for generic edges which have no attributes.
 *
 * GENERIC_EDGE_ELEMENT(NodeA, Edge, NodeB) expands to:
 *
 *     using NodeA_Adjacent_NodeB_Traits = S15::EncodingTraits<S15::EdgeEncoding_NodeA_Edge_NodeB>;
 *     using NodeA_Edge_NodeB_Base = Base<Nodes::NodeA, Nodes::NodeB, NodeA_Edge_NodeB_Traits>;
 *     class NodeA_Edge_NodeA : public S15::EncodingTraits<
 *         NodeA,
 *         NodeB,
 *         S15::EncodingTraits<S15::EdgeEncoding_NodeA_Edge_NodeB>
 *     > {};
 */
#define GENERIC_EDGE_ELEMENT(NodeA, Edge, NodeB) \
class NodeA##_##Edge##_##NodeB : public Base<\
    Nodes::NodeA, \
    Nodes::NodeB, \
    S15::EncodingTraits<S15::EdgeEncoding_##NodeA##_##Edge##_##NodeB> \
> {}

GENERIC_EDGE_ELEMENT(Action, ExposesTo, Unit);
GENERIC_EDGE_ELEMENT(Action, Threatens, Unit);
GENERIC_EDGE_ELEMENT(Action, Damages, Unit);
GENERIC_EDGE_ELEMENT(Action, EndsAt, Hex);
GENERIC_EDGE_ELEMENT(Action, By, Unit);
GENERIC_EDGE_ELEMENT(Unit, Blocks, Unit);
GENERIC_EDGE_ELEMENT(Unit, CanMelee, Unit);
GENERIC_EDGE_ELEMENT(Unit, CanShoot, Unit);
GENERIC_EDGE_ELEMENT(Unit, Threatens, Hex);
GENERIC_EDGE_ELEMENT(Unit, Occupies, Hex);

}
