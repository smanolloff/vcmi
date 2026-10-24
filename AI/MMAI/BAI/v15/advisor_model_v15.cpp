/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "nn_model_v15.h"
#include "nn_model_util_v15.h"
#include "action_probabilities_v15.h"
#include "schema/v15/types.h"

namespace MMAI::BAI::V15
{
namespace S15 = MMAI::Schema::V15;
using namespace ModelUtil;

void NNModel::evaluateActionProbabilities(const MMAI::Schema::IState * s)
{
	NestedLogTag _("evaluateActionProbabilities");
	auto any = s->getSupplementaryData();
	if(s->version() != version)
		throwf("evaluateActionProbabilities: unsupported IState version: want: %d, have: %d", version, s->version());
	if(!any.has_value())
		throw std::runtime_error("evaluateActionProbabilities: supplementary data is empty");

	auto err = MMAI::Schema::AnyCastError(any, typeid(const S15::ISupplementaryData *));
	if(!err.empty())
		throwf("evaluateActionProbabilities: anycast failed: %s", err);
	const auto * sup = std::any_cast<const S15::ISupplementaryData *>(any);
	if(!sup || sup->getType() != S15::ISupplementaryData::Type::REGULAR)
		throwf("evaluateActionProbabilities: invalid supplementary data");
	const auto * graph = sup->getGraph();
	if(!graph)
		throwf("evaluateActionProbabilities: null graph");

	auto inputs = prepareInputs(graph);
	auto outputs = container->session->Run(Ort::RunOptions(), inputNames.data(), inputs.data(), inputs.size(), outputNames.data(), outputNames.size());
	if(outputs.size() != 4)
		throwf("evaluateActionProbabilities: bad output size: want: 4, have: %d", outputs.size());

	const auto activeActionIds = graph->getActiveActionIds();
	if(activeActionIds.empty())
		throwf("evaluateActionProbabilities: no active actions");
	const auto activeProbs = toVec1D<float>("active_probs", outputs[1], static_cast<int64_t>(activeActionIds.size()));
	const auto value = toScalar<float>("value", outputs[2]);
	const auto activeActionIdsOut = toVec1D<int64_t>("active_action_ids_out", outputs[3], static_cast<int64_t>(activeActionIds.size()));
	if(activeActionIdsOut != activeActionIds)
		throwf("evaluateActionProbabilities: active action ids output mismatch");

	publishActionProbabilities(graph, activeActionIds, std::vector<double>(activeProbs.begin(), activeProbs.end()), value);
}

}
