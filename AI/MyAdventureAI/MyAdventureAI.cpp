/*
 * VCAI.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "MyAdventureAI.h"

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

// TODO: refine includes
#include "../../lib/UnlockGuard.h"
#include "../../lib/mapObjects/MapObjects.h"
#include "../../lib/mapObjects/ObjectTemplate.h"
#include "../../lib/CConfigHandler.h"
#include "../../lib/CHeroHandler.h"
#include "../../lib/GameSettings.h"
#include "../../lib/gameState/CGameState.h"
#include "../../lib/NetPacksBase.h"
#include "../../lib/NetPacks.h"
#include "../../lib/bonuses/CBonusSystemNode.h"
#include "../../lib/bonuses/Limiters.h"
#include "../../lib/bonuses/Updaters.h"
#include "../../lib/bonuses/Propagators.h"
#include "../../lib/serializer/CTypeList.h"
#include "../../lib/serializer/BinarySerializer.h"
#include "../../lib/serializer/BinaryDeserializer.h"

MyAdventureAI::MyAdventureAI() {
  print("*** (constructor) ***");
}

MyAdventureAI::~MyAdventureAI() {}

void MyAdventureAI::print(const std::string &text) const
{
  logAi->trace("MyAdventureAI  [%p]: %s", this, text);
}

void MyAdventureAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
  print("*** initGameInterface ***");
  cb = CB;
  cbc = CB;
  cb->waitTillRealize = true;
  cb->unlockGsWhenWaiting = true;
}

void MyAdventureAI::yourTurn() {
  print("*** yourTurn called ***");

  std::make_unique<boost::thread>([this]() {
    boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

    auto heroes = cb->getHeroesInfo();
    assert(!heroes.empty());
    auto h = heroes[0];

    print("kur1");
    cb->moveHero(h, int3{2,1,0}, false);
    print("kur2");
  });
}

void MyAdventureAI::battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) {
  print("*** battleStart called ***");
  CAdventureAI::battleStart(army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void MyAdventureAI::battleEnd(const BattleResult * br, QueryID queryID) {
  print("*** battleEnd called ***");

  bool won = br->winner == cb->battleGetMySide();
  won ? print("battle result: victory") : print("battle result: defeat");

  // NOTE: although VCAI does cb->selectionMade in a new thread, we must not
  // there seems to be a race cond and our selection randomly
  // arrives at the server with a queryID=1 unless we do it sequentially
  boost::this_thread::sleep_for(boost::chrono::seconds(5));
  cb->selectionMade(1, queryID);
  CAdventureAI::battleEnd(br, queryID);
}

//
// Dummy methods
//

void MyAdventureAI::saveGame(BinarySerializer & h, const int version) {
  print("*** saveGame called ***");
}

void MyAdventureAI::loadGame(BinaryDeserializer & h, const int version) {
  print("*** loadGame called ***");
}

void MyAdventureAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) {
  print("*** commanderGotLevel called ***");
}

void MyAdventureAI::finish() {
  print("*** finish called ***");
}

void MyAdventureAI::heroGotLevel(const CGHeroInstance * hero, PrimarySkill::PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) {
  print("*** heroGotLevel called ***");
}

void MyAdventureAI::showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel) {
  print("*** showBlockingDialog called ***");
}

void MyAdventureAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) {
  print("*** showGarrisonDialog called ***");
}

void MyAdventureAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) {
  print("*** showMapObjectSelectDialog called ***");
}

void MyAdventureAI::showTeleportDialog(TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) {
  print("*** showTeleportDialog called ***");
}

void MyAdventureAI::showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) {
  print("*** showWorldViewEx called ***");
}

void MyAdventureAI::advmapSpellCast(const CGHeroInstance * caster, int spellID) {
  print("*** advmapSpellCast called ***");
}

void MyAdventureAI::artifactAssembled(const ArtifactLocation & al) {
  print("*** artifactAssembled called ***");
}

void MyAdventureAI::artifactDisassembled(const ArtifactLocation & al) {
  print("*** artifactDisassembled called ***");
}

void MyAdventureAI::artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) {
  print("*** artifactMoved called ***");
}

void MyAdventureAI::artifactPut(const ArtifactLocation & al) {
  print("*** artifactPut called ***");
}

void MyAdventureAI::artifactRemoved(const ArtifactLocation & al) {
  print("*** artifactRemoved called ***");
}

void MyAdventureAI::availableArtifactsChanged(const CGBlackMarket * bm) {
  print("*** availableArtifactsChanged called ***");
}

void MyAdventureAI::availableCreaturesChanged(const CGDwelling * town) {
  print("*** availableCreaturesChanged called ***");
}

void MyAdventureAI::battleResultsApplied() {
  print("*** battleResultsApplied called ***");
}

void MyAdventureAI::beforeObjectPropertyChanged(const SetObjectProperty * sop) {
  print("*** beforeObjectPropertyChanged called ***");
}

void MyAdventureAI::buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) {
  print("*** buildChanged called ***");
}

void MyAdventureAI::centerView(int3 pos, int focusTime) {
  print("*** centerView called ***");
}

void MyAdventureAI::gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) {
  print("*** gameOver called ***");
}

void MyAdventureAI::garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) {
  print("*** garrisonsChanged called ***");
}

void MyAdventureAI::heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) {
  print("*** heroBonusChanged called ***");
}

void MyAdventureAI::heroCreated(const CGHeroInstance *) {
  print("*** heroCreated called ***");
}

void MyAdventureAI::heroInGarrisonChange(const CGTownInstance * town) {
  print("*** heroInGarrisonChange called ***");
}

void MyAdventureAI::heroManaPointsChanged(const CGHeroInstance * hero) {
  print("*** heroManaPointsChanged called ***");
}

void MyAdventureAI::heroMovePointsChanged(const CGHeroInstance * hero) {
  print("*** heroMovePointsChanged called ***");
}

void MyAdventureAI::heroMoved(const TryMoveHero & details, bool verbose) {
  print("*** heroMoved called ***");
}

void MyAdventureAI::heroPrimarySkillChanged(const CGHeroInstance * hero, int which, si64 val) {
  print("*** heroPrimarySkillChanged called ***");
}

void MyAdventureAI::heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) {
  print("*** heroSecondarySkillChanged called ***");
}

void MyAdventureAI::heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) {
  print("*** heroVisit called ***");
}

void MyAdventureAI::heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) {
  print("*** heroVisitsTown called ***");
}

void MyAdventureAI::newObject(const CGObjectInstance * obj) {
  print("*** newObject called ***");
}

void MyAdventureAI::objectPropertyChanged(const SetObjectProperty * sop) {
  print("*** objectPropertyChanged called ***");
}

void MyAdventureAI::objectRemoved(const CGObjectInstance * obj) {
  print("*** objectRemoved called ***");
}

void MyAdventureAI::playerBlocked(int reason, bool start) {
  print("*** playerBlocked called ***");
}

void MyAdventureAI::playerBonusChanged(const Bonus & bonus, bool gain) {
  print("*** playerBonusChanged called ***");
}

void MyAdventureAI::receivedResource() {
  print("*** receivedResource called ***");
}

void MyAdventureAI::requestRealized(PackageApplied * pa) {
  print("*** requestRealized called ***");
}

void MyAdventureAI::requestSent(const CPackForServer * pack, int requestID) {
  print("*** requestSent called ***");
}

void MyAdventureAI::showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) {
  print("*** showHillFortWindow called ***");
}

void MyAdventureAI::showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) {
  print("*** showInfoDialog called ***");
}

void MyAdventureAI::showMarketWindow(const IMarket * market, const CGHeroInstance * visitor) {
  print("*** showMarketWindow called ***");
}

void MyAdventureAI::showPuzzleMap() {
  print("*** showPuzzleMap called ***");
}

void MyAdventureAI::showRecruitmentDialog(const CGDwelling * dwelling, const CArmedInstance * dst, int level) {
  print("*** showRecruitmentDialog called ***");
}

void MyAdventureAI::showShipyardDialog(const IShipyard * obj) {
  print("*** showShipyardDialog called ***");
}

void MyAdventureAI::showTavernWindow(const CGObjectInstance * townOrTavern) {
  print("*** showTavernWindow called ***");
}

void MyAdventureAI::showThievesGuildWindow(const CGObjectInstance * obj) {
  print("*** showThievesGuildWindow called ***");
}

void MyAdventureAI::showUniversityWindow(const IMarket * market, const CGHeroInstance * visitor) {
  print("*** showUniversityWindow called ***");
}

void MyAdventureAI::tileHidden(const std::unordered_set<int3> & pos) {
  print("*** tileHidden called ***");
}

void MyAdventureAI::tileRevealed(const std::unordered_set<int3> & pos) {
  print("*** tileRevealed called ***");
}

void MyAdventureAI::heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) {
  print("*** heroExchangeStarted called ***");
}

std::string MyAdventureAI::getBattleAIName() const {
  return "StupidAI";
}
