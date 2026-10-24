/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "schema/v15/graph.h"

namespace MMAI::BAI::V15
{
void publishActionProbabilities(
	const Schema::V15::Graph::IGraph * graph,
	const std::vector<int64_t> & activeActionIds,
	const std::vector<double> & adjustedProbs,
	float value);
}
