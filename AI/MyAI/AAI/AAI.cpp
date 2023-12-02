#include "NetPacks.h"
#include "StdInc.h"
#include "AAI.h"
#include "gameState/CGameState.h"
#include "mytypes.h"
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>

#include <boost/thread.hpp>

namespace MMAI {

AAI::AAI() {
  info("*** (constructor) ***");
}

AAI::~AAI() {
  info("--- (destructor) ---");
}

void AAI::error(const std::string &text) const { logAi->error("AAI %s", text); }
void AAI::warn(const std::string &text) const { logAi->warn("AAI %s", text); }
void AAI::info(const std::string &text) const { logAi->info("AAI %s", text); }
void AAI::debug(const std::string &text) const { logAi->debug("AAI %s", text); }

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
  error("*** initGameInterface -- BUT NO BAGGAGE ***");
  initGameInterface(env, CB, std::any{});
}

void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage) {
  info("*** initGameInterface ***");
  cb = CB;
  cbc = CB;
  cb->waitTillRealize = true;
  cb->unlockGsWhenWaiting = true;

  ASSERT(baggage.has_value(), "baggage has no value");
  ASSERT(baggage.type() == typeid(MMAIExport::CBProvider*), "baggage of unexpected type");
  cbprovider = std::any_cast<MMAIExport::CBProvider*>(baggage);
}

MMAIExport::Action AAI::getNonRenderAction(const MMAIExport::Result * result) {
  auto action = cbprovider->f_getAction(result);

  while (action == MMAIExport::ACTION_RENDER_ANSI) {
    auto res = MMAIExport::Result(bai->renderANSI());
    action = cbprovider->f_getAction(&res);
  }

  return action;
}

MMAIExport::Action AAI::getAction(const MMAIExport::Result * result) {
  auto action = getNonRenderAction(result);

  if (action == MMAIExport::ACTION_RESET) {
    // AAI::getAction is called only by BAI, only during battle
    // FIXME: retreat may be impossible, a _real_ reset should be implemented
    info("Will retreat in order to reset battle");
    ASSERT(!bai->result->ended, "expected active battle");
    action = MMAIExport::ACTION_RETREAT;
  }

  return action;
}

void AAI::yourTurn() {
  info("*** yourTurn ***");

  std::make_unique<boost::thread>([this]() {
    boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

    auto heroes = cb->getHeroesInfo();
    assert(!heroes.empty());
    auto h = heroes[0];

    cb->moveHero(h, int3{2,1,0}, false);
  });
}

void AAI::battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) {
  info("*** battleStart ***");

  // just copied code from CAdventureAI::battleStart
  // only difference is argument to initBattleInterface()
  assert(!battleAI);
  assert(cbc);

  auto aiName = getBattleAIName();
  ASSERT(aiName == "MyAI", "wrong battle AI: " + aiName);

  battleAI = CDynLibHandler::getNewBattleAI(aiName);

  bai = std::static_pointer_cast<BAI>(battleAI);

  MMAIExport::F_GetAction fga = [this](const MMAIExport::Result * r) { return this->getAction(r); };
  bai->myInitBattleInterface(env, cbc, fga);

  battleAI->battleStart(army1, army2, tile, hero1, hero2, side, replayAllowed);
}

void AAI::battleEnd(const BattleResult * br, QueryID queryID) {
  info("*** battleEnd ***");

  // TODO: must add check that *we* have retreated and not the enemy
  if (bai->action->action != MMAIExport::ACTION_RETREAT) {
    // battleEnd after MOVE
    // => call f_getAction (expecting RESET)

    // first call bai->battleEnd to update result
    // (in case there are render actions)
    bai->battleEnd(br, queryID);

    auto action = getNonRenderAction(bai->result.get());

    info("Reset battle");
    // Any non-render action *must* be a reset (battle has ended)
    ASSERT(action == MMAIExport::ACTION_RESET, "unexpected action: " + std::to_string(action));
  }

  battleAI.reset();
  cb->selectionMade(1, queryID);
}

//
// Dummy methods
//

void AAI::saveGame(BinarySerializer & h, const int version) {
  debug("*** saveGame ***");
}

void AAI::loadGame(BinaryDeserializer & h, const int version) {
  debug("*** loadGame ***");
}

void AAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) {
  debug("*** commanderGotLevel ***");
}

void AAI::finish() {
  debug("*** finish ***");
}

void AAI::heroGotLevel(const CGHeroInstance * hero, PrimarySkill::PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) {
  debug("*** heroGotLevel ***");
}

void AAI::showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel) {
  debug("*** showBlockingDialog ***");
}

void AAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) {
  debug("*** showGarrisonDialog ***");
}

void AAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) {
  debug("*** showMapObjectSelectDialog ***");
}

void AAI::showTeleportDialog(TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) {
  debug("*** showTeleportDialog ***");
}

void AAI::showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) {
  debug("*** showWorldViewEx ***");
}

void AAI::advmapSpellCast(const CGHeroInstance * caster, int spellID) {
  debug("*** advmapSpellCast ***");
}

void AAI::artifactAssembled(const ArtifactLocation & al) {
  debug("*** artifactAssembled ***");
}

void AAI::artifactDisassembled(const ArtifactLocation & al) {
  debug("*** artifactDisassembled ***");
}

void AAI::artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) {
  debug("*** artifactMoved ***");
}

void AAI::artifactPut(const ArtifactLocation & al) {
  debug("*** artifactPut ***");
}

void AAI::artifactRemoved(const ArtifactLocation & al) {
  debug("*** artifactRemoved ***");
}

void AAI::availableArtifactsChanged(const CGBlackMarket * bm) {
  debug("*** availableArtifactsChanged ***");
}

void AAI::availableCreaturesChanged(const CGDwelling * town) {
  debug("*** availableCreaturesChanged ***");
}

void AAI::battleResultsApplied() {
  debug("*** battleResultsApplied ***");
}

void AAI::beforeObjectPropertyChanged(const SetObjectProperty * sop) {
  debug("*** beforeObjectPropertyChanged ***");
}

void AAI::buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) {
  debug("*** buildChanged ***");
}

void AAI::centerView(int3 pos, int focusTime) {
  debug("*** centerView ***");
}

void AAI::gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) {
  debug("*** gameOver ***");
}

void AAI::garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) {
  debug("*** garrisonsChanged ***");
}

void AAI::heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) {
  debug("*** heroBonusChanged ***");
}

void AAI::heroCreated(const CGHeroInstance *) {
  debug("*** heroCreated ***");
}

void AAI::heroInGarrisonChange(const CGTownInstance * town) {
  debug("*** heroInGarrisonChange ***");
}

void AAI::heroManaPointsChanged(const CGHeroInstance * hero) {
  debug("*** heroManaPointsChanged ***");
}

void AAI::heroMovePointsChanged(const CGHeroInstance * hero) {
  debug("*** heroMovePointsChanged ***");
}

void AAI::heroMoved(const TryMoveHero & details, bool verbose) {
  debug("*** heroMoved ***");
}

void AAI::heroPrimarySkillChanged(const CGHeroInstance * hero, int which, si64 val) {
  debug("*** heroPrimarySkillChanged ***");
}

void AAI::heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) {
  debug("*** heroSecondarySkillChanged ***");
}

void AAI::heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) {
  debug("*** heroVisit ***");
}

void AAI::heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) {
  debug("*** heroVisitsTown ***");
}

void AAI::newObject(const CGObjectInstance * obj) {
  debug("*** newObject ***");
}

void AAI::objectPropertyChanged(const SetObjectProperty * sop) {
  debug("*** objectPropertyChanged ***");
}

void AAI::objectRemoved(const CGObjectInstance * obj) {
  debug("*** objectRemoved ***");
}

void AAI::playerBlocked(int reason, bool start) {
  debug("*** playerBlocked ***");
}

void AAI::playerBonusChanged(const Bonus & bonus, bool gain) {
  debug("*** playerBonusChanged ***");
}

void AAI::receivedResource() {
  debug("*** receivedResource ***");
}

void AAI::requestRealized(PackageApplied * pa) {
  debug("*** requestRealized ***");
}

void AAI::requestSent(const CPackForServer * pack, int requestID) {
  debug("*** requestSent ***");
}

void AAI::showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) {
  debug("*** showHillFortWindow ***");
}

void AAI::showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) {
  debug("*** showInfoDialog ***");
}

void AAI::showMarketWindow(const IMarket * market, const CGHeroInstance * visitor) {
  debug("*** showMarketWindow ***");
}

void AAI::showPuzzleMap() {
  debug("*** showPuzzleMap ***");
}

void AAI::showRecruitmentDialog(const CGDwelling * dwelling, const CArmedInstance * dst, int level) {
  debug("*** showRecruitmentDialog ***");
}

void AAI::showShipyardDialog(const IShipyard * obj) {
  debug("*** showShipyardDialog ***");
}

void AAI::showTavernWindow(const CGObjectInstance * townOrTavern) {
  debug("*** showTavernWindow ***");
}

void AAI::showThievesGuildWindow(const CGObjectInstance * obj) {
  debug("*** showThievesGuildWindow ***");
}

void AAI::showUniversityWindow(const IMarket * market, const CGHeroInstance * visitor) {
  debug("*** showUniversityWindow ***");
}

void AAI::tileHidden(const std::unordered_set<int3> & pos) {
  debug("*** tileHidden ***");
}

void AAI::tileRevealed(const std::unordered_set<int3> & pos) {
  debug("*** tileRevealed ***");
}

void AAI::heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) {
  debug("*** heroExchangeStarted ***");
}

std::string AAI::getBattleAIName() const {
  return "MyAI";
}

} // namespace MAI
