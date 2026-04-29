/*
 * hex.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "BAI/v15/graph/nodes/hex.h"
#include "BAI/v15/stack.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
using HexAction = Schema::V15::HexAction;
using HexAttribute = Schema::V15::Graph::NodeAttributes::Hex;
using HexState = Schema::V15::HexState;

using HexActionMask = std::bitset<EI(HexAction::_count)>;
using HexStateMask = std::bitset<EI(HexState::_count)>;
using HexActionHex = Graph::Nodes::HexActionHex;
using ActiveStackInfo = Graph::Nodes::ActiveStackInfo;
using Hex = Graph::Nodes::Hex;
}
