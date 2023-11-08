#include "NetPacks.h"
#include "StdInc.h"
#include "AAI.h"
#include "gameState/CGameState.h"

#include <boost/thread.hpp>
#include <boost/chrono.hpp>

// TODO: refine includes


namespace MMAI {

AAI::AAI() {
  print("*** (constructor) ***");
}

AAI::~AAI() {}

void AAI::print(const std::string &text) const
{
  logAi->info("AAI  [%p]: %s", this, text);
}

// Called by GymEnv on every "reset()" call
void AAI::resetcb() {
    print("called");

    // Unblock "battleEnd"
    print("acquire lock");
    boost::unique_lock<boost::mutex> lock(m);

    if (awaitingReset) {
      // AAI::battleEnd is blocked -- unblock it
      // (so it can answer the waiting query with "replay battle")
      print("cond.notify_one()");
      cond.notify_one();
    } else {
      // means BAI::activeStack is blocked
      // => just call cppcb with a "Retreat" action to unblock it
      // (then a battleEnd will be called as usual)
      assert(bai->awaitingAction);
      bai->actioncb(0);
    }

    print("return");
}

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
  print("*** initGameInterface -- BUT NO BAGGAGE ***");
  initGameInterface(env, CB, std::any{});
}

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage) {
  print("*** initGameInterface ***");
  cb = CB;
  cbc = CB;
  cb->waitTillRealize = true;
  cb->unlockGsWhenWaiting = true;

  assert(baggage.has_value());
  assert(baggage.type() == typeid(CBProvider*));
  cbprovider = std::any_cast<CBProvider*>(baggage);

  // print("*** call cbprovider->resetcbcb([this]() { cbprovider->resetcb() })");
  cbprovider->resetcbcb([this]() { this->resetcb(); });
}

void AAI::yourTurn() {
  print("*** yourTurn ***");

  std::make_unique<boost::thread>([this]() {
    boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

    auto heroes = cb->getHeroesInfo();
    assert(!heroes.empty());
    auto h = heroes[0];

    cb->moveHero(h, int3{2,1,0}, false);
  });
}

void AAI::battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) {
  print("*** battleStart ***");

  // just copied code from CAdventureAI::battleStart
  // only difference is argument to initBattleInterface()
  assert(!battleAI);
  assert(cbc);

  auto aiName = getBattleAIName();
  assert(aiName == "MyAI");

  battleAI = CDynLibHandler::getNewBattleAI(aiName);

  bai = std::static_pointer_cast<BAI>(battleAI);
  bai->myInitBattleInterface(env, cbc, cbprovider);

  battleAI->battleStart(army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void AAI::battleEnd(const BattleResult * br, QueryID queryID) {
  print("*** battleEnd ***");

  bool won = br->winner == cb->battleGetMySide();
  won ? print("battle result: victory") : print("battle result: defeat");

  boost::unique_lock<boost::mutex> lock(m);
  awaitingReset = true;

  // NOTE: although VCAI does cb->selectionMade in a new thread, we must not
  // there seems to be a race cond and our selection randomly
  // arrives at the server with a queryID=1 unless we do it sequentially
  cb->selectionMade(1, queryID);

  // this will call battle AI's battleEnd, which will inform
  // gym of the results and be destroyed afterwards
  CAdventureAI::battleEnd(br, queryID);

  // We're waiting for a call to GYM's reset which
  // should invoke our resetcb
  cond.wait(lock);

  awaitingReset = false;
}

//
// Dummy methods
//

void AAI::saveGame(BinarySerializer & h, const int version) {
  // print("*** saveGame ***");
}

void AAI::loadGame(BinaryDeserializer & h, const int version) {
  // print("*** loadGame ***");
}

void AAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) {
  // print("*** commanderGotLevel ***");
}

void AAI::finish() {
  // print("*** finish ***");
}

void AAI::heroGotLevel(const CGHeroInstance * hero, PrimarySkill::PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) {
  // print("*** heroGotLevel ***");
}

void AAI::showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel) {
  // print("*** showBlockingDialog ***");
}

void AAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) {
  // print("*** showGarrisonDialog ***");
}

void AAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) {
  // print("*** showMapObjectSelectDialog ***");
}

void AAI::showTeleportDialog(TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) {
  // print("*** showTeleportDialog ***");
}

void AAI::showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) {
  // print("*** showWorldViewEx ***");
}

void AAI::advmapSpellCast(const CGHeroInstance * caster, int spellID) {
  // print("*** advmapSpellCast ***");
}

void AAI::artifactAssembled(const ArtifactLocation & al) {
  // print("*** artifactAssembled ***");
}

void AAI::artifactDisassembled(const ArtifactLocation & al) {
  // print("*** artifactDisassembled ***");
}

void AAI::artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) {
  // print("*** artifactMoved ***");
}

void AAI::artifactPut(const ArtifactLocation & al) {
  // print("*** artifactPut ***");
}

void AAI::artifactRemoved(const ArtifactLocation & al) {
  // print("*** artifactRemoved ***");
}

void AAI::availableArtifactsChanged(const CGBlackMarket * bm) {
  // print("*** availableArtifactsChanged ***");
}

void AAI::availableCreaturesChanged(const CGDwelling * town) {
  // print("*** availableCreaturesChanged ***");
}

void AAI::battleResultsApplied() {
  // print("*** battleResultsApplied ***");
}

void AAI::beforeObjectPropertyChanged(const SetObjectProperty * sop) {
  // print("*** beforeObjectPropertyChanged ***");
}

void AAI::buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) {
  // print("*** buildChanged ***");
}

void AAI::centerView(int3 pos, int focusTime) {
  // print("*** centerView ***");
}

void AAI::gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) {
  // print("*** gameOver ***");
}

void AAI::garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) {
  // print("*** garrisonsChanged ***");
}

void AAI::heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) {
  // print("*** heroBonusChanged ***");
}

void AAI::heroCreated(const CGHeroInstance *) {
  // print("*** heroCreated ***");
}

void AAI::heroInGarrisonChange(const CGTownInstance * town) {
  // print("*** heroInGarrisonChange ***");
}

void AAI::heroManaPointsChanged(const CGHeroInstance * hero) {
  // print("*** heroManaPointsChanged ***");
}

void AAI::heroMovePointsChanged(const CGHeroInstance * hero) {
  // print("*** heroMovePointsChanged ***");
}

void AAI::heroMoved(const TryMoveHero & details, bool verbose) {
  // print("*** heroMoved ***");
}

void AAI::heroPrimarySkillChanged(const CGHeroInstance * hero, int which, si64 val) {
  // print("*** heroPrimarySkillChanged ***");
}

void AAI::heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) {
  // print("*** heroSecondarySkillChanged ***");
}

void AAI::heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) {
  // print("*** heroVisit ***");
}

void AAI::heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) {
  // print("*** heroVisitsTown ***");
}

void AAI::newObject(const CGObjectInstance * obj) {
  // print("*** newObject ***");
}

void AAI::objectPropertyChanged(const SetObjectProperty * sop) {
  // print("*** objectPropertyChanged ***");
}

void AAI::objectRemoved(const CGObjectInstance * obj) {
  // print("*** objectRemoved ***");
}

void AAI::playerBlocked(int reason, bool start) {
  // print("*** playerBlocked ***");
}

void AAI::playerBonusChanged(const Bonus & bonus, bool gain) {
  // print("*** playerBonusChanged ***");
}

void AAI::receivedResource() {
  // print("*** receivedResource ***");
}

void AAI::requestRealized(PackageApplied * pa) {
  // print("*** requestRealized ***");
}

void AAI::requestSent(const CPackForServer * pack, int requestID) {
  // print("*** requestSent ***");
}

void AAI::showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) {
  // print("*** showHillFortWindow ***");
}

void AAI::showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) {
  // print("*** showInfoDialog ***");
}

void AAI::showMarketWindow(const IMarket * market, const CGHeroInstance * visitor) {
  // print("*** showMarketWindow ***");
}

void AAI::showPuzzleMap() {
  // print("*** showPuzzleMap ***");
}

void AAI::showRecruitmentDialog(const CGDwelling * dwelling, const CArmedInstance * dst, int level) {
  // print("*** showRecruitmentDialog ***");
}

void AAI::showShipyardDialog(const IShipyard * obj) {
  // print("*** showShipyardDialog ***");
}

void AAI::showTavernWindow(const CGObjectInstance * townOrTavern) {
  // print("*** showTavernWindow ***");
}

void AAI::showThievesGuildWindow(const CGObjectInstance * obj) {
  // print("*** showThievesGuildWindow ***");
}

void AAI::showUniversityWindow(const IMarket * market, const CGHeroInstance * visitor) {
  // print("*** showUniversityWindow ***");
}

void AAI::tileHidden(const std::unordered_set<int3> & pos) {
  // print("*** tileHidden ***");
}

void AAI::tileRevealed(const std::unordered_set<int3> & pos) {
  // print("*** tileRevealed ***");
}

void AAI::heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) {
  // print("*** heroExchangeStarted ***");
}

std::string AAI::getBattleAIName() const {
  return "MyAI";
}

} // namespace MAI
