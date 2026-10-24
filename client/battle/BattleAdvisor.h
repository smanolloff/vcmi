/*
 * BattleAdvisor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/Color.h"

class BattleWindow;
class BattleInterface;
class BattleAdvisorProbabilityOverlay;
class BattleAdvisorValueOverlay;

ColorRGBA battleActionProbabilityColor(float probability);

/// Advisor controls and overlays attached to the battle window.
class BattleAdvisor
{
	BattleWindow & window;
	BattleInterface & owner;
	std::shared_ptr<BattleAdvisorProbabilityOverlay> waitProbabilityOverlay;
	std::shared_ptr<BattleAdvisorProbabilityOverlay> defendProbabilityOverlay;
	std::shared_ptr<BattleAdvisorValueOverlay> battleValueOverlay;
	uint64_t actionProbabilityRevision = std::numeric_limits<uint64_t>::max();
	bool actionProbabilityHeatmapVisible = false;
	bool actionProbabilityPercentagesVisible = false;

public:
	BattleAdvisor(BattleWindow & window, BattleInterface & owner);
	void attachOverlays();
	void createValueOverlay();
	void refresh();
	void updateValuePosition();
	void setTacticsMode(bool tactics);
};
