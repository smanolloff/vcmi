/*
 * BattleProcessor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "ArtifactUtils.h"
#include "GameSettings.h"
#include "Global.h"
#include "StdInc.h"
#include "BattleProcessor.h"

#include "BattleActionProcessor.h"
#include "BattleFlowProcessor.h"
#include "BattleResultProcessor.h"

#include "../CGameHandler.h"
#include "../queries/QueriesProcessor.h"
#include "../queries/BattleQueries.h"

#include "../../lib/CPlayerState.h"
#include "../../lib/TerrainHandler.h"
#include "../../lib/battle/CBattleInfoCallback.h"
#include "../../lib/battle/CObstacleInstance.h"
#include "../../lib/battle/BattleInfo.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/mapping/CMap.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/modding/IdentifierStorage.h"
#include "../../lib/networkPacks/PacksForClient.h"
#include "../../lib/networkPacks/PacksForClientBattle.h"
#include "../../lib/CPlayerState.h"
#include "constants/EntityIdentifiers.h"
#include "mapObjects/CGTownInstance.h"

BattleProcessor::BattleProcessor(CGameHandler * gameHandler)
	: gameHandler(gameHandler)
	, flowProcessor(std::make_unique<BattleFlowProcessor>(this, gameHandler))
	, actionsProcessor(std::make_unique<BattleActionProcessor>(this, gameHandler))
	, resultProcessor(std::make_unique<BattleResultProcessor>(this, gameHandler))
{
}

BattleProcessor::~BattleProcessor() = default;

void BattleProcessor::engageIntoBattle(PlayerColor player)
{
	//notify interfaces
	PlayerBlocked pb;
	pb.player = player;
	pb.reason = PlayerBlocked::UPCOMING_BATTLE;
	pb.startOrEnd = PlayerBlocked::BLOCKADE_STARTED;
	gameHandler->sendAndApply(&pb);
}

void BattleProcessor::restartBattlePrimary(const BattleID & battleID, const CArmedInstance *army1, const CArmedInstance *army2, int3 tile,
								const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool creatureBank,
								const CGTownInstance *town)
{
	auto battle = gameHandler->gameState()->getBattle(battleID);

	auto lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(battle->sides[0].color));
	if(!lastBattleQuery)
		lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(battle->sides[1].color));

	assert(lastBattleQuery);

	//existing battle query for retying auto-combat
	if(lastBattleQuery)
	{
		const CGHeroInstance*heroes[2];
		heroes[0] = hero1;
		heroes[1] = hero2;

		for(int i : {0, 1})
		{
			if(heroes[i])
			{
				SetMana restoreInitialMana;
				restoreInitialMana.val = lastBattleQuery->initialHeroMana[i];
				restoreInitialMana.hid = heroes[i]->id;
				gameHandler->sendAndApply(&restoreInitialMana);
			}
		}

		lastBattleQuery->result = std::nullopt;

		// swapping armies between battle replayes causes these asserts to fail
		//assert(lastBattleQuery->belligerents[0] == battle->sides[0].armyObject);
		//assert(lastBattleQuery->belligerents[1] == battle->sides[1].armyObject);
	}

	BattleCancelled bc;
	bc.battleID = battleID;
	gameHandler->sendAndApply(&bc);

	startBattlePrimary(army1, army2, tile, hero1, hero2, creatureBank, town);
}

void BattleProcessor::startBattlePrimary(const CArmedInstance *army1, const CArmedInstance *army2, int3 tile,
								const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool creatureBank,
								const CGTownInstance *town)
{
	assert(gameHandler->gameState()->getBattle(army1->getOwner()) == nullptr);
	assert(gameHandler->gameState()->getBattle(army2->getOwner()) == nullptr);

	const CArmedInstance *armies[2];
	const CGHeroInstance *heroes[2];

	// std::tie(army1, army2, hero1, hero2) = gymPreBattleHook(army1, army2, hero1, hero2);
	gymPreBattleHook(army1, army2, hero1, hero2);

	armies[0] = army1;
	armies[1] = army2;
	heroes[0] = hero1;
	heroes[1] = hero2;

	auto battleID = setupBattle(tile, armies, heroes, creatureBank, town); //initializes stacks, places creatures on battlefield, blocks and informs player interfaces

	const auto * battle = gameHandler->gameState()->getBattle(battleID);
	assert(battle);

	//add battle bonuses based from player state only when attacks neutral creatures
	const auto * attackerInfo = gameHandler->getPlayerState(army1->getOwner(), false);
	if(attackerInfo && !army2->getOwner().isValidPlayer())
	{
		for(auto bonus : attackerInfo->battleBonuses)
		{
			GiveBonus giveBonus(GiveBonus::ETarget::OBJECT);
			giveBonus.id = hero1->id;
			giveBonus.bonus = bonus;
			gameHandler->sendAndApply(&giveBonus);
		}
	}

	auto lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(battle->sides[0].color));
	if(!lastBattleQuery)
		lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(battle->sides[1].color));

	if (lastBattleQuery)
	{
		lastBattleQuery->battleID = battleID;
	}
	else
	{
		auto newBattleQuery = std::make_shared<CBattleQuery>(gameHandler, battle);

		// store initial mana to reset if battle has been restarted
		for(int i : {0, 1})
			if(heroes[i])
				newBattleQuery->initialHeroMana[i] = heroes[i]->mana;

		gameHandler->queries->addQuery(newBattleQuery);
	}

	flowProcessor->onBattleStarted(*battle);
}

void BattleProcessor::gymPreBattleHook(const CArmedInstance *&army1, const CArmedInstance *&army2, const CGHeroInstance *&hero1, const CGHeroInstance *&hero2) {
	auto gh = gameHandler;
	bool swappingSides = (gh->swapSides > 0 && (gh->battlecounter % gh->swapSides) == 0);
	// printf("battlecounter: %d, swapSides: %d, rem: %d\n", gh->battlecounter, gh->swapSides, gh->battlecounter % gh->swapSides);

	gh->battlecounter++;

	if (!gh->stats && gh->statsMode != "disabled") {
		gh->stats = std::make_unique<Stats>(
			gh->allheroes.size(),
			gh->statsStorage,
			gh->statsPersistFreq,
			gh->statsSampling,
			gh->statsScoreVar
		);
	}

	if (swappingSides)
		gh->redside = !gameHandler->redside;

	// printf("gh->randomHeroes = %d\n", gh->randomHeroes);

	if (gh->randomHeroes > 0) {
		if (gh->stats && gh->statsSampling) {
			auto [id1, id2] = gh->stats->sample2(gh->statsMode == "red" ? gh->redside : !gh->redside);
			hero1 = gh->allheroes.at(id1);
			hero2 = gh->allheroes.at(id2);
			// printf("sampled: hero1: %d, hero2: %d (perspective: %s)\n", id1, id2, gh->statsMode.c_str());
		} else {
			if (gh->allheroes.size() % 2 != 0)
				throw std::runtime_error("Heroes size must be even");

			if (gh->herocounter % gh->allheroes.size() == 0) {
				gh->herocounter = 0;

				gh->useTrueRng
					? std::shuffle(gh->allheroes.begin(), gh->allheroes.end(), gh->truerng)
					: std::shuffle(gh->allheroes.begin(), gh->allheroes.end(), gh->pseudorng);

				// for (int i=0; i<gh->allheroes.size(); i++)
				// 	printf("gh->allheroes[%d] = %s\n", i, gh->allheroes.at(i)->getNameTextID().c_str());
			}
			// printf("gh->herocounter = %d\n", gh->herocounter);

			// XXX: heroes must be different (objects must have different tempOwner)
			hero1 = gh->allheroes.at(gh->herocounter);
			hero2 = gh->allheroes.at(gh->herocounter+1);

			if (gh->battlecounter % gh->randomHeroes == 0)
				gh->herocounter += 2;
		}

		// XXX: adding war machines by index of pre-created per-hero artifact instances
		// 0=ballista, 1=cart, 2=tent
		auto machineslots = std::map<ArtifactID, ArtifactPosition> {
			{ArtifactID::BALLISTA, ArtifactPosition::MACH1},
			{ArtifactID::AMMO_CART, ArtifactPosition::MACH2},
			{ArtifactID::FIRST_AID_TENT, ArtifactPosition::MACH3},
		};

		auto dist = std::uniform_int_distribution<>(0, 99);
		for (auto h : {hero1, hero2}) {
			for (auto m : gh->allmachines.at(h)) {
		        auto it = machineslots.find(m->getTypeId());
		        if (it == machineslots.end())
		        	throw std::runtime_error("Could not find warmachine");

	            auto apos = it->second;
				auto roll = gh->useTrueRng ? dist(gh->truerng) : dist(gh->pseudorng);
				if (roll < gh->warmachineChance) {
					if (!h->getArt(apos)) const_cast<CGHeroInstance*>(h)->putArtifact(apos, m);
				} else {
					if (h->getArt(apos)) const_cast<CGHeroInstance*>(h)->removeArtifact(apos);
				}
			}
		}

	}

	// Set temp owner of both heroes to player0 and player1
	// XXX: causes UB after battle, unless it is replayed (ok for training)
	// XXX: if redside=1 (right), hero2 should have owner=0 (red)
	//      if redside=0 (left), hero1 should have owner=0 (red)
	const_cast<CGHeroInstance*>(hero1)->tempOwner = PlayerColor(gh->redside);
	const_cast<CGHeroInstance*>(hero2)->tempOwner = PlayerColor(!gh->redside);

	// printf("swappingSides: %d, redside: %d, hero1->tmpOwner: %d, hero2->tmpOwner: %d\n", swappingSides, gh->redside, hero1->tempOwner.getNum(), hero2->tempOwner.getNum());

	// modification by reference
	army1 = static_cast<const CArmedInstance*>(hero1);
	army2 = static_cast<const CArmedInstance*>(hero2);
}

void BattleProcessor::startBattleI(const CArmedInstance *army1, const CArmedInstance *army2, int3 tile, bool creatureBank)
{
	// TODO: use map->allHeroes instead?
	// XXX: assuming this is called only ONCE (no smart pointers for warmachines)
	for (const auto &obj : gameHandler->gameState()->map->objects) {
		if (obj->ID != Obj::HERO) continue;

		auto h = dynamic_cast<const CGHeroInstance *>(obj.get());
		gameHandler->allheroes.push_back(h);
		gameHandler->allmachines[h] = {
			ArtifactUtils::createNewArtifactInstance(ArtifactID::BALLISTA),
			ArtifactUtils::createNewArtifactInstance(ArtifactID::AMMO_CART),
			ArtifactUtils::createNewArtifactInstance(ArtifactID::FIRST_AID_TENT)
		};
	}

	for (const auto &obj : gameHandler->gameState()->map->towns) {
		gameHandler->alltowns.push_back(obj.get());
	}

	startBattlePrimary(army1, army2, tile,
		army1->ID == Obj::HERO ? static_cast<const CGHeroInstance*>(army1) : nullptr,
		army2->ID == Obj::HERO ? static_cast<const CGHeroInstance*>(army2) : nullptr,
		creatureBank);
}

void BattleProcessor::startBattleI(const CArmedInstance *army1, const CArmedInstance *army2, bool creatureBank)
{
	startBattleI(army1, army2, army2->visitablePos(), creatureBank);
}

BattleID BattleProcessor::setupBattle(int3 tile, const CArmedInstance *armies[2], const CGHeroInstance *heroes[2], bool creatureBank, const CGTownInstance *town)
{
	const auto & t = *gameHandler->getTile(tile);
	TerrainId terrain = t.terType->getId();
	if (gameHandler->gameState()->map->isCoastalTile(tile)) //coastal tile is always ground
		terrain = ETerrainId::SAND;

	BattleField terType = gameHandler->gameState()->battleGetBattlefieldType(tile, gameHandler->getRandomGenerator());
	if (heroes[0] && heroes[0]->boat && heroes[1] && heroes[1]->boat)
		terType = BattleField(*VLC->identifiers()->getIdentifier("core", "battlefield.ship_to_ship"));

	auto gh = gameHandler;

	if (gh->randomObstacles > 0 && (gh->battlecounter % gh->randomObstacles == 0)) {
		gh->lastSeed = gh->useTrueRng
			? gh->truerng()
			: gh->getRandomGenerator().nextInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
	}

	if (gh->townChance > 0) {
		auto dist = std::uniform_int_distribution<>(0, 99);
		auto roll = gh->useTrueRng ? dist(gh->truerng) : dist(gh->pseudorng);

		if (roll < gh->townChance) {
			if (gh->towncounter % gh->alltowns.size() == 0) {
				gh->towncounter = 0;
				gh->useTrueRng
					? std::shuffle(gh->alltowns.begin(), gh->alltowns.end(), gh->truerng)
					: std::shuffle(gh->alltowns.begin(), gh->alltowns.end(), gh->pseudorng);
			}

			town = gh->alltowns.at(gh->towncounter);
			++gh->towncounter;
		} else {
			town = nullptr;
		}
	}

	//send info about battles
	BattleStart bs;
	bs.info = BattleInfo::setupBattle(tile, terrain, terType, armies, heroes, creatureBank, town, gh->lastSeed);
	bs.battleID = gameHandler->gameState()->nextBattleID;

	engageIntoBattle(bs.info->sides[0].color);
	engageIntoBattle(bs.info->sides[1].color);

	auto lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(bs.info->sides[0].color));
	if(!lastBattleQuery)
		lastBattleQuery = std::dynamic_pointer_cast<CBattleQuery>(gameHandler->queries->topQuery(bs.info->sides[1].color));
	bool isDefenderHuman = bs.info->sides[1].color.isValidPlayer() && gameHandler->getPlayerState(bs.info->sides[1].color)->isHuman();
	bool isAttackerHuman = gameHandler->getPlayerState(bs.info->sides[0].color)->isHuman();

	bool onlyOnePlayerHuman = isDefenderHuman != isAttackerHuman;

	if (VLC->settings()->getBoolean(EGameSettings::COMBAT_INFINITE_QUICK_COMBAT_REPLAYS))
		bs.info->replayAllowed = true;
	else
		bs.info->replayAllowed = lastBattleQuery == nullptr && onlyOnePlayerHuman;

	gameHandler->sendAndApply(&bs);

	return bs.battleID;
}

bool BattleProcessor::checkBattleStateChanges(const CBattleInfoCallback & battle)
{
	//check if drawbridge state need to be changes
	if (battle.battleGetSiegeLevel() > 0)
		updateGateState(battle);

	if (resultProcessor->battleIsEnding(battle))
		return true;

	//check if battle ended
	if (auto result = battle.battleIsFinished())
	{
		setBattleResult(battle, EBattleResult::NORMAL, *result);
		return true;
	}

	return false;
}

void BattleProcessor::updateGateState(const CBattleInfoCallback & battle)
{
	// GATE_BRIDGE - leftmost tile, located over moat
	// GATE_OUTER - central tile, mostly covered by gate image
	// GATE_INNER - rightmost tile, inside the walls

	// GATE_OUTER or GATE_INNER:
	// - if defender moves unit on these tiles, bridge will open
	// - if there is a creature (dead or alive) on these tiles, bridge will always remain open
	// - blocked to attacker if bridge is closed

	// GATE_BRIDGE
	// - if there is a unit or corpse here, bridge can't open (and can't close in fortress)
	// - if Force Field is cast here, bridge can't open (but can close, in any town)
	// - deals moat damage to attacker if bridge is closed (fortress only)

	bool hasForceFieldOnBridge = !battle.battleGetAllObstaclesOnPos(BattleHex(BattleHex::GATE_BRIDGE), true).empty();
	bool hasStackAtGateInner   = battle.battleGetUnitByPos(BattleHex(BattleHex::GATE_INNER), false) != nullptr;
	bool hasStackAtGateOuter   = battle.battleGetUnitByPos(BattleHex(BattleHex::GATE_OUTER), false) != nullptr;
	bool hasStackAtGateBridge  = battle.battleGetUnitByPos(BattleHex(BattleHex::GATE_BRIDGE), false) != nullptr;
	bool hasWideMoat           = vstd::contains_if(battle.battleGetAllObstaclesOnPos(BattleHex(BattleHex::GATE_BRIDGE), false), [](const std::shared_ptr<const CObstacleInstance> & obst)
	{
		return obst->obstacleType == CObstacleInstance::MOAT;
	});

	BattleUpdateGateState db;
	db.state = battle.battleGetGateState();
	db.battleID = battle.getBattle()->getBattleID();

	if (battle.battleGetWallState(EWallPart::GATE) == EWallState::DESTROYED)
	{
		db.state = EGateState::DESTROYED;
	}
	else if (db.state == EGateState::OPENED)
	{
		bool hasStackOnLongBridge = hasStackAtGateBridge && hasWideMoat;
		bool gateCanClose = !hasStackAtGateInner && !hasStackAtGateOuter && !hasStackOnLongBridge;

		if (gateCanClose)
			db.state = EGateState::CLOSED;
		else
			db.state = EGateState::OPENED;
	}
	else // CLOSED or BLOCKED
	{
		bool gateBlocked = hasForceFieldOnBridge || hasStackAtGateBridge;

		if (gateBlocked)
			db.state = EGateState::BLOCKED;
		else
			db.state = EGateState::CLOSED;
	}

	if (db.state != battle.battleGetGateState())
		gameHandler->sendAndApply(&db);
}

bool BattleProcessor::makePlayerBattleAction(const BattleID & battleID, PlayerColor player, const BattleAction &ba)
{
	const auto * battle = gameHandler->gameState()->getBattle(battleID);

	if (!battle)
		return false;

	bool result = actionsProcessor->makePlayerBattleAction(*battle, player, ba);
	if (gameHandler->gameState()->getBattle(battleID) != nullptr && !resultProcessor->battleIsEnding(*battle))
		flowProcessor->onActionMade(*battle, ba);
	return result;
}

void BattleProcessor::setBattleResult(const CBattleInfoCallback & battle, EBattleResult resultType, int victoriusSide)
{
	resultProcessor->setBattleResult(battle, resultType, victoriusSide);
	resultProcessor->endBattle(battle);
}

bool BattleProcessor::makeAutomaticBattleAction(const CBattleInfoCallback & battle, const BattleAction &ba)
{
	return actionsProcessor->makeAutomaticBattleAction(battle, ba);
}

void BattleProcessor::endBattleConfirm(const BattleID & battleID)
{
	auto battle = gameHandler->gameState()->getBattle(battleID);
	assert(battle);

	if (!battle)
		return;

	resultProcessor->endBattleConfirm(*battle);
}

void BattleProcessor::battleAfterLevelUp(const BattleID & battleID, const BattleResult &result)
{
	resultProcessor->battleAfterLevelUp(battleID, result);
}
