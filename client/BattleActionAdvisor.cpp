/*
 * BattleActionAdvisor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CPlayerInterface.h"

#include "Client.h"
#include "CServerHandler.h"
#include "GameEngine.h"
#include "GameInstance.h"
#include "battle/BattleInterface.h"
#include "battle/BattleStacksController.h"

#include "../lib/CConfigHandler.h"
#include "../lib/UnlockGuard.h"
#include "../lib/callback/AIFactory.h"
#include "../lib/callback/CCallback.h"

void CPlayerInterface::prepareBattleActionAdvisor(
	const BattleID & bid,
	const CCreatureSet * army1,
	const CCreatureSet * army2,
	int3 tile,
	const CGHeroInstance * hero1,
	const CGHeroInstance * hero2,
	BattleSide side)
{
	try
	{
		battleActionAdvisor = AIFactory::createBattleActionAdvisor();
		if(!battleActionAdvisor)
			return;
		battleActionAdvisor->initBattleInterface(env, cb, autocombatPreferences);
		battleActionAdvisor->battleStart(bid, army1, army2, tile, hero1, hero2, side, false);
		GAME->server().client->registerBattleInterface(battleActionAdvisor, playerID);
	}
	catch(const std::exception & e)
	{
		logAi->error("Failed to initialize MMAI action advisor: %s", e.what());
		battleActionAdvisor.reset();
	}
}

void CPlayerInterface::updateBattleActionHint()
{
	if(!battleActionAdvisor || !battleInt)
		return;
	const auto * stack = battleInt->stacksController->getActiveStack();
	if(!stack)
		return;

	try
	{
		battleActionAdvisor->activeStack(battleInt->getBattleID(), stack);
	}
	catch(const std::exception & e)
	{
		logAi->error("MMAI action advisor failed: %s", e.what());
		stopBattleActionAdvisor();
	}
}

void CPlayerInterface::evaluateBattleActionAdvisor(const BattleID & battleID, const CStack * stack)
{
	if(battleActionAdvisor && settings["battle"]["showActionProbabilityHeatmap"].Bool())
	{
		try
		{
			auto unlockInterface = vstd::makeUnlockGuard(ENGINE->interfaceMutex);
			battleActionAdvisor->activeStack(battleID, stack);
		}
		catch(const std::exception & e)
		{
			logAi->error("MMAI action advisor failed: %s", e.what());
			stopBattleActionAdvisor();
		}
	}
}

void CPlayerInterface::stopBattleActionAdvisor()
{
	if(!battleActionAdvisor)
		return;
	if(GAME && GAME->server().client)
		GAME->server().client->unregisterBattleInterface(battleActionAdvisor, playerID);
	battleActionAdvisor.reset();
}
