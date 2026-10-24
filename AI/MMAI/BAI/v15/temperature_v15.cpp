/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "temperature_v15.h"
#include "nn_model_util_v15.h"

namespace MMAI::BAI::V15
{
using ModelUtil::throwf;

std::vector<double> temperatureAdjustedProbabilities(const std::vector<float> & probs, float temperature)
{
	if(temperature < 0.0f)
		throwf("sample: negative temperature");
	if(probs.empty())
		throwf("sample: no probabilities");

	bool hasPositiveProbability = false;
	for(const auto probability : probs)
	{
		if(!std::isfinite(probability) || probability < 0.0f)
			throwf("sample: invalid probability: %f", probability);
		hasPositiveProbability |= probability > 0.0f;
	}
	if(!hasPositiveProbability)
		throwf("sample: all probabilities are 0");

	if(temperature < 1e-5f)
	{
		auto result = std::vector<double>(probs.size(), 0.0);
		const auto best = std::ranges::max_element(probs);
		result.at(std::distance(probs.begin(), best)) = 1.0;
		return result;
	}

	auto logWeights = std::vector<double>(probs.size(), -std::numeric_limits<double>::infinity());
	double maxLogWeight = -std::numeric_limits<double>::infinity();
	for(std::size_t i = 0; i < probs.size(); ++i)
	{
		if(probs[i] > 0.0f)
		{
			logWeights[i] = std::log(probs[i]) / temperature;
			maxLogWeight = std::max(maxLogWeight, logWeights[i]);
		}
	}

	auto result = std::vector<double>(probs.size(), 0.0);
	double sum = 0.0;
	for(std::size_t i = 0; i < probs.size(); ++i)
	{
		if(std::isfinite(logWeights[i]))
		{
			result[i] = std::exp(logWeights[i] - maxLogWeight);
			sum += result[i];
		}
	}
	if(sum <= 0.0)
		throwf("sample: invalid probability sum: %f", sum);
	for(auto & probability : result)
		probability /= sum;
	return result;
}

}
