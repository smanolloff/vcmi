/*
 * render.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "AI/MMAI/common.h" // IWYU pragma: keep

#include "BAI/v16/state_v16.h"

namespace MMAI::BAI::V16
{
std::string Render(const State * state, const std::shared_ptr<const Graph::Nodes::Action> & action);
}
