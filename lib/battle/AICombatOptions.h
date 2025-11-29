/*
 * AIOptions.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

struct AICombatOptions
{
	bool enableSpellsUsage = true;

	//TODO: below options exist in original H3, consider usefulness of mixed human-AI combat when enabling autocombat inside battle
	// bool enableUnitsUsage = true;
	// bool enableCatapultUsage = true;
	// bool enableBallistaUsage = true;
	// bool enableFirstAidTendUsage = true;

	// AI-specific options are transported as std::any as they may
	// include arbitary types which are not known here
	// (e.g. for optional AIs that are excluded from the VCMI core codebase)
	std::any other;
};

