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
#include "AAI.h"
#include "../BAI/BAI.h"

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

// TODO: refine includes
#include "../../../lib/UnlockGuard.h"
#include "../../../lib/mapObjects/MapObjects.h"
#include "../../../lib/mapObjects/ObjectTemplate.h"
#include "../../../lib/CConfigHandler.h"
#include "../../../lib/CHeroHandler.h"
#include "../../../lib/GameSettings.h"
#include "../../../lib/gameState/CGameState.h"
#include "../../../lib/NetPacksBase.h"
#include "../../../lib/NetPacks.h"
#include "../../../lib/bonuses/CBonusSystemNode.h"
#include "../../../lib/bonuses/Limiters.h"
#include "../../../lib/bonuses/Updaters.h"
#include "../../../lib/bonuses/Propagators.h"
#include "../../../lib/serializer/CTypeList.h"
#include "../../../lib/serializer/BinarySerializer.h"
#include "../../../lib/serializer/BinaryDeserializer.h"

namespace MMAI {

AAI::AAI() {
  print("*** (constructor) ***");
}

AAI::~AAI() {}

void AAI::print(const std::string &text) const
{
  logAi->trace("AAI  [%p]: %s", this, text);
}

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
  print("*** initGameInterface -- BUT NO BAGGAGE ***");
  initGameInterface(env, CB, std::any{});
}

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage) {
  print("*** initGameInterface ***");
  cb = CB;
  cbc = CB;

  assert(baggage.has_value());
  assert(baggage.type() == typeid(CBProvider*));
  cbprovider = std::any_cast<CBProvider*>(baggage);

  cb->waitTillRealize = true;
  cb->unlockGsWhenWaiting = true;
}

void AAI::yourTurn() {
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

void AAI::battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) {
  print("*** battleStart called ***");

  // just copied code from CAdventureAI::battleStart
  // only difference is argument to initBattleInterface()
  assert(!battleAI);
  assert(cbc);

  auto aiName = getBattleAIName();
  assert(aiName == "MyAI");

  battleAI = CDynLibHandler::getNewBattleAI(aiName);

  auto tmp = std::static_pointer_cast<MMAI::BAI>(battleAI);
  tmp->myInitBattleInterface(env, cbc, cbprovider);

  battleAI->battleStart(army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void AAI::battleEnd(const BattleResult * br, QueryID queryID) {
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

void AAI::saveGame(BinarySerializer & h, const int version) {
  print("*** saveGame called ***");
}

void AAI::loadGame(BinaryDeserializer & h, const int version) {
  print("*** loadGame called ***");
}

void AAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) {
  print("*** commanderGotLevel called ***");
}

void AAI::finish() {
  print("*** finish called ***");
}

void AAI::heroGotLevel(const CGHeroInstance * hero, PrimarySkill::PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) {
  print("*** heroGotLevel called ***");
}

void AAI::showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel) {
  print("*** showBlockingDialog called ***");
}

void AAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) {
  print("*** showGarrisonDialog called ***");
}

void AAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) {
  print("*** showMapObjectSelectDialog called ***");
}

void AAI::showTeleportDialog(TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) {
  print("*** showTeleportDialog called ***");
}

void AAI::showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) {
  print("*** showWorldViewEx called ***");
}

void AAI::advmapSpellCast(const CGHeroInstance * caster, int spellID) {
  print("*** advmapSpellCast called ***");
}

void AAI::artifactAssembled(const ArtifactLocation & al) {
  print("*** artifactAssembled called ***");
}

void AAI::artifactDisassembled(const ArtifactLocation & al) {
  print("*** artifactDisassembled called ***");
}

void AAI::artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) {
  print("*** artifactMoved called ***");
}

void AAI::artifactPut(const ArtifactLocation & al) {
  print("*** artifactPut called ***");
}

void AAI::artifactRemoved(const ArtifactLocation & al) {
  print("*** artifactRemoved called ***");
}

void AAI::availableArtifactsChanged(const CGBlackMarket * bm) {
  print("*** availableArtifactsChanged called ***");
}

void AAI::availableCreaturesChanged(const CGDwelling * town) {
  print("*** availableCreaturesChanged called ***");
}

void AAI::battleResultsApplied() {
  print("*** battleResultsApplied called ***");
}

void AAI::beforeObjectPropertyChanged(const SetObjectProperty * sop) {
  print("*** beforeObjectPropertyChanged called ***");
}

void AAI::buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) {
  print("*** buildChanged called ***");
}

void AAI::centerView(int3 pos, int focusTime) {
  print("*** centerView called ***");
}

void AAI::gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) {
  print("*** gameOver called ***");
}

void AAI::garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) {
  print("*** garrisonsChanged called ***");
}

void AAI::heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) {
  print("*** heroBonusChanged called ***");
}

void AAI::heroCreated(const CGHeroInstance *) {
  print("*** heroCreated called ***");
}

void AAI::heroInGarrisonChange(const CGTownInstance * town) {
  print("*** heroInGarrisonChange called ***");
}

void AAI::heroManaPointsChanged(const CGHeroInstance * hero) {
  print("*** heroManaPointsChanged called ***");
}

void AAI::heroMovePointsChanged(const CGHeroInstance * hero) {
  print("*** heroMovePointsChanged called ***");
}

void AAI::heroMoved(const TryMoveHero & details, bool verbose) {
  print("*** heroMoved called ***");
}

void AAI::heroPrimarySkillChanged(const CGHeroInstance * hero, int which, si64 val) {
  print("*** heroPrimarySkillChanged called ***");
}

void AAI::heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) {
  print("*** heroSecondarySkillChanged called ***");
}

void AAI::heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) {
  print("*** heroVisit called ***");
}

void AAI::heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) {
  print("*** heroVisitsTown called ***");
}

void AAI::newObject(const CGObjectInstance * obj) {
  print("*** newObject called ***");
}

void AAI::objectPropertyChanged(const SetObjectProperty * sop) {
  print("*** objectPropertyChanged called ***");
}

void AAI::objectRemoved(const CGObjectInstance * obj) {
  print("*** objectRemoved called ***");
}

void AAI::playerBlocked(int reason, bool start) {
  print("*** playerBlocked called ***");
}

void AAI::playerBonusChanged(const Bonus & bonus, bool gain) {
  print("*** playerBonusChanged called ***");
}

void AAI::receivedResource() {
  print("*** receivedResource called ***");
}

void AAI::requestRealized(PackageApplied * pa) {
  print("*** requestRealized called ***");
}

void AAI::requestSent(const CPackForServer * pack, int requestID) {
  print("*** requestSent called ***");
}

void AAI::showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) {
  print("*** showHillFortWindow called ***");
}

void AAI::showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) {
  print("*** showInfoDialog called ***");
}

void AAI::showMarketWindow(const IMarket * market, const CGHeroInstance * visitor) {
  print("*** showMarketWindow called ***");
}

void AAI::showPuzzleMap() {
  print("*** showPuzzleMap called ***");
}

void AAI::showRecruitmentDialog(const CGDwelling * dwelling, const CArmedInstance * dst, int level) {
  print("*** showRecruitmentDialog called ***");
}

void AAI::showShipyardDialog(const IShipyard * obj) {
  print("*** showShipyardDialog called ***");
}

void AAI::showTavernWindow(const CGObjectInstance * townOrTavern) {
  print("*** showTavernWindow called ***");
}

void AAI::showThievesGuildWindow(const CGObjectInstance * obj) {
  print("*** showThievesGuildWindow called ***");
}

void AAI::showUniversityWindow(const IMarket * market, const CGHeroInstance * visitor) {
  print("*** showUniversityWindow called ***");
}

void AAI::tileHidden(const std::unordered_set<int3> & pos) {
  print("*** tileHidden called ***");
}

void AAI::tileRevealed(const std::unordered_set<int3> & pos) {
  print("*** tileRevealed called ***");
}

void AAI::heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) {
  print("*** heroExchangeStarted called ***");
}

std::string AAI::getBattleAIName() const {
  return "MyAI";
}

} // namespace MAI
