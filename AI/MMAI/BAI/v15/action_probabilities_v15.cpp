/*
 * nn_model.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "action_probabilities_v15.h"
#include "nn_model_util_v15.h"
#include "BAI/v15/graph/nodes/action_v15.h"
#include "logging/BattleActionProbabilities.h"

namespace MMAI::BAI::V15
{
namespace S15 = MMAI::Schema::V15;
using ET = S15::Graph::ElementType;
using ModelUtil::throwf;

void publishActionProbabilities(
	const S15::Graph::IGraph * graph,
	const std::vector<int64_t> & activeActionIds,
	const std::vector<double> & adjustedProbs,
	float value)
{
	auto policy = BattleActionProbabilities{};
	policy.value = value;
	for(std::size_t i = 0; i < activeActionIds.size(); ++i)
	{
		const auto * node = graph->getNode(ET::NODE_ACTION, activeActionIds[i]);
		auto * action = dynamic_cast<const Graph::Nodes::Action *>(node);
		if(!action)
			throwf("getAction: action node has unexpected type");
		if(!action->by)
			throwf("getAction: action node has no acting unit");
		const auto actionSide = action->by->cstack.unitSide();
		if(policy.side != BattleSide::NONE && policy.side != actionSide)
			throwf("getAction: active actions have inconsistent sides");
		policy.side = actionSide;

		std::cout << "PROB: " << adjustedProbs[i] << " " << action->name() << "\n";

		const auto probability = static_cast<float>(adjustedProbs[i]);
		switch(action->actionType)
		{
			case S15::ActionType::MOVE:
			{
				auto & moveProbability = policy.moves[action->endsAt.front()->bhex];
				moveProbability = std::max(moveProbability, probability);
				break;
			}
			case S15::ActionType::AMOVE:
			{
				if(!action->target)
					throwf("getAction: attack action has no target");
				auto & targetProbability = policy.attackTargets[action->target->cstack.getPosition()];
				targetProbability = std::max(targetProbability, probability);

				bool directionFound = false;
				for(const auto & attackerHex : action->endsAt)
				{
					for(const auto & targetHex : action->target->cstack.getHexes())
					{
						const auto direction = BattleHex::mutualPosition(attackerHex->bhex, targetHex);
						if(direction == BattleHex::NONE)
							continue;
						const auto directionIndex = static_cast<std::size_t>(direction);
						auto & directionProbability = policy.meleeAttacks[attackerHex->bhex].at(directionIndex);
						if(!directionProbability || probability > *directionProbability)
						{
							directionProbability = probability;
							policy.meleeAttackTargets[attackerHex->bhex].at(directionIndex) = action->target->cstack.getPosition();
						}
						directionFound = true;
						break;
					}
					if(directionFound)
						break;
				}
				break;
			}
			case S15::ActionType::SHOOT:
			{
				if(!action->target)
					throwf("getAction: attack action has no target");
				auto & targetProbability = policy.attackTargets[action->target->cstack.getPosition()];
				targetProbability = std::max(targetProbability, probability);
				break;
			}
			case S15::ActionType::WAIT:
				policy.wait += probability;
				break;
			case S15::ActionType::DEFEND:
				policy.defend += probability;
				break;
			default:
				throwf("getAction: unexpected action type: %d", EI(action->actionType));
		}
	}
	battleActionProbabilities().setBattleActionProbabilities(std::move(policy));
}

}
