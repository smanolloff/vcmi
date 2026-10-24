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

#include <vector>

namespace MMAI::BAI::V15
{
std::vector<double> temperatureAdjustedProbabilities(const std::vector<float> & probs, float temperature);
}
