/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "router.h"
#include "v15/BAI_v15.h"
#include "v15/nn_model_v15.h"
#include "logging/BattleActionProbabilities.h"

namespace MMAI::BAI
{
void ActionAdvisor::activeStack(const BattleID & bid, const CStack * stack)
{
	auto * advisor = dynamic_cast<V15::BAI *>(bai.get());
	if(advisor)
		advisor->evaluateActionProbabilities(stack);
}

}

namespace MMAI::BAI::V15
{
void BAI::evaluateActionProbabilities(const CStack * stack)
{
	battleActionProbabilities().clearBattleActionProbabilities();
	state->onActiveStack(stack, roundcounter);
	auto * nnModel = dynamic_cast<NNModel *>(model);
	if(!nnModel)
		return;
	nnModel->evaluateActionProbabilities(state.get());
}

}
