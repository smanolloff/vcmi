/*
 * stack.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/nodes/unit.h"
#include "BAI/v15/global_stats.h"
#include "schema/v15/constants.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{

namespace S15 = Schema::V15;
using StackAttribute = Schema::V15::Graph::NodeAttributes::Unit;
using StackFlag1 = Schema::V15::StackFlag1;
using StackFlag2 = Schema::V15::StackFlag2;
using StackFlags1 = Schema::V15::StackFlags1;
using StackFlags2 = Schema::V15::StackFlags2;

using Queue = Graph::Nodes::Queue;
using BitQueue = Graph::Nodes::BitQueue;
using Stack = Graph::Nodes::Unit;
}
