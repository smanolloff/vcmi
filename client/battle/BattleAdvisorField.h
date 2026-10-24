/*
 * BattleAdvisorField.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/Rect.h"
#include "../../lib/logging/BattleActionProbabilities.h"

class BattleInterface;
class BattleFieldController;
class Canvas;
class IImage;

/// Advisor heatmap geometry and animation in battlefield-local coordinates.
class BattleAdvisorField
{
	BattleInterface & owner;
	BattleFieldController & field;
	std::shared_ptr<IImage> cellShade;
	std::vector<Rect> cellShadeOpaqueRuns;
	std::vector<Rect> cellShadeCenterRuns;
	std::array<std::vector<Rect>, 6> cellShadeAttackRuns;
	float actionProbabilityOutlinePhase = 0.0f;
	uint64_t actionProbabilityRevision = 0;
	std::optional<BattleActionProbabilities> probabilities;

	void showActionProbabilityHeatmap(Canvas & canvas, const BattleActionProbabilities & probabilities);
	void showActionProbabilityLabels(Canvas & canvas, const BattleActionProbabilities & probabilities);

public:
	BattleAdvisorField(BattleInterface & owner, BattleFieldController & field, std::shared_ptr<IImage> shade);
	void showHeatmap(Canvas & canvas);
	void showLabels(Canvas & canvas);
	void tick(uint32_t msPassed);
};
