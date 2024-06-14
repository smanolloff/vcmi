// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#include <cstdio>
#include <boost/thread.hpp>
#include <stdexcept>

#include "CCallback.h"
#include "battle/BattleAction.h"
#include "gameState/CGameState.h"
#include "mapObjects/CGHeroInstance.h"

#include "common.h"
#include "AAI.h"

namespace MMAI::AAI {
    AAI::AAI() {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++");
    }

    AAI::~AAI() {
        info("--- (destructor) ---");
    }

    void AAI::error(const std::string &text) const { logAi->error("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::warn(const std::string &text) const { logAi->warn("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::info(const std::string &text) const { logAi->info("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }
    void AAI::debug(const std::string &text) const { logAi->debug("AAI-%s [%s%s%s] %s", addrstr, ansicolor, color, ansireset, text); }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB) {
        error("*** initGameInterface -- BUT NO BAGGAGE ***");
        initGameInterface(env, CB, std::any{});
    }

    void AAI::initGameInterface(std::shared_ptr<Environment> env, std::shared_ptr<CCallback> CB, std::any baggage_) {
        info("*** initGameInterface ***");

        color = CB->getPlayerID()->toString();
        baggage = baggage_;

        ASSERT(baggage_.has_value(), "baggage has no value");
        ASSERT(baggage_.type() == typeid(Schema::Baggage*), "baggage of unexpected type");
        auto baggage__ = std::any_cast<Schema::Baggage*>(baggage);
        ASSERT(baggage__, "baggage contains a nullptr");

        if (color == "red") {
            // ansicolor = "\033[41m";  // red background
            battleAiName = baggage__->battleAINameRed;
        } else if (color == "blue") {
            // ansicolor = "\033[44m";  // blue background
            battleAiName = baggage__->battleAINameBlue;
        } else {
            // Maps and everything basically assumes red human player attacking blue human player
            // Swapping armies and sides still uses only RED and BLUE as players
            // Other players should never be asked to lead a battle
            throw std::runtime_error("Tried to initialize AAI for player " + color);
            battleAiName = "BUG_IF_REQUESTED";
        }

        debug("(init) battleAiName: " + battleAiName);

        cb = CB;
        cbc = CB;

        // XXX: not sure if needed
        cb->waitTillRealize = true;
        cb->unlockGsWhenWaiting = true;
    };

    void AAI::yourTurn(QueryID queryID) {
        info("*** yourTurn *** (" + std::to_string(queryID.getNum()) + ")");

        std::make_unique<boost::thread>([this, queryID]() {
            boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);

            info("Answering query " + std::to_string(queryID) + " to start turn");
            cb->selectionMade(0, queryID);

            auto heroes = cb->getHeroesInfo();
            assert(!heroes.empty());
            auto h = heroes.at(0);

            // Move 1 tile to the right
            cb->moveHero(h, h->pos + int3{1,0,0}, false);
        });
    }

    void AAI::battleStart(const BattleID &bid, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side_, bool replayAllowed) {
        info("*** battleStart ***");

        side = side_;

        const CGHeroInstance* hero;

        // Battles are ALWAYS between a RED hero and a BLUE hero
        // If --random-heroes is provided, side_, hero1 and hero2 will be different
        // Regardless hero1 and hero2's real owner, RED and BLUE AAIs will
        // receive them as battleStart arguments as if they were the owners.
        // (hero1->tempOwner is set to 0 (RED) or 1 (BLUE) for that purpose)

        // XXX: fix wrong color if --swap-sides option is used
        if (side == BattleSide::ATTACKER) {
            hero = dynamic_cast<const CGHeroInstance*>(army1);
            info("Will play with " + hero->getNameTextID() + " on the left side (ATTACKER) in this battle");
        } else {
            hero = dynamic_cast<const CGHeroInstance*>(army2);
            info("Will play with " + hero->getNameTextID() + " on the right side (DEFENDER) in this battle");
        }

        // XXX: VCMI's hero IDs do cannot be inferred by the map's JSON
        //      The gym maps use the hero's experience as a unique ref
        // debug("(battleStart) hero1->tempOwner: " + std::to_string(hero1->tempOwner));
        // debug("(battleStart) hero2->tempOwner: " + std::to_string(hero2->tempOwner));
        // debug("(battleStart) hero(army)->tempOwner: " + std::to_string(hero->tempOwner));
        // debug("(battleStart) hero1->getOwner(): " + std::to_string(hero1->getOwner()));
        // debug("(battleStart) hero2->getOwner(): " + std::to_string(hero2->getOwner()));
        // debug("(battleStart) hero(army)->getOwner(): " + std::to_string(hero->getOwner()));

        // just copied code from CAdventureAI::battleStart
        // only difference is argument to initBattleInterface()
        assert(!battleAI);
        assert(cbc);

        auto ainame = getBattleAIName();
        battleAI = CDynLibHandler::getNewBattleAI(ainame);
        battleAI->initBattleInterface(env, cbc, baggage, color);
        battleAI->battleStart(bid, army1, army2, tile, hero1, hero2, side_, replayAllowed);
    }

    void AAI::battleEnd(const BattleID &bid, const BattleResult * br, QueryID queryID) {
        info("*** battleEnd (QueryID: " + std::to_string(static_cast<int>(queryID)) + ") ***");

        battleAI->battleEnd(bid, br, queryID);

        if (cb->getBattle(bid)->battleGetMySide() == BattlePerspective::LEFT_SIDE) {
            ASSERT(queryID != -1, "QueryID is -1, but we are ATTACKER");
            info("Answering query " + std::to_string(queryID) + " to re-play battle");

            std::make_unique<boost::thread>([this, queryID]() {
                boost::shared_lock<boost::shared_mutex> gsLock(CGameState::mutex);
                cb->selectionMade(1, queryID);
            });
        } else {
            // My patch in CGameHandler::endBattle allows replay even when
            // both sides are non-neutrals. Could not figure out how to
            // send the query only to the attacker.

            // The defender should not answer replay battle queries
            info("Ignoring query " + std::to_string(queryID));
        }

        battleAI.reset();
    }

    std::string AAI::getBattleAIName() const {
        debug("*** getBattleAIName ***");

        ASSERT(!battleAiName.empty(), "battleAIName is not initialized yet");
        debug("getBattleAIName: " + battleAiName);
        return battleAiName;
    }

    void AAI::battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) {
        battleAI->battleTriggerEffect(bid, bte);
    }

    void AAI::saveGame(BinarySerializer & h) {
        debug("*** saveGame ***");
    }

    void AAI::loadGame(BinaryDeserializer & h) {
        debug("*** loadGame ***");
    }

    void AAI::commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) {
        debug("*** commanderGotLevel ***");
    }

    void AAI::finish() {
        debug("*** finish ***");
    }

    void AAI::heroGotLevel(const CGHeroInstance *hero, PrimarySkill pskill, std::vector<SecondarySkill> &skills, QueryID queryID) {
        debug("*** heroGotLevel ***");
    }

    void AAI::showBlockingDialog(const std::string &text, const std::vector<Component> &components, QueryID askID, const int soundID, bool selection, bool cancel, bool safeToAutoaccept) {
        debug("*** showBlockingDialog ***");
    }

    void AAI::showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) {
        debug("*** showGarrisonDialog ***");
    }

    void AAI::showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) {
        debug("*** showMapObjectSelectDialog ***");
    }

    void AAI::showTeleportDialog(const CGHeroInstance * hero, TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) {
        debug("*** showTeleportDialog ***");
    }

    void AAI::showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) {
        debug("*** showWorldViewEx ***");
    }

    void AAI::advmapSpellCast(const CGHeroInstance * caster, SpellID spellID) {
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

    void AAI::heroPrimarySkillChanged(const CGHeroInstance * hero, PrimarySkill which, si64 val) {
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

    void AAI::objectRemoved(const CGObjectInstance * obj, const PlayerColor &initiator) {
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

    void AAI::showMarketWindow(const IMarket *market, const CGHeroInstance *visitor, QueryID queryID) {
        debug("*** showMarketWindow ***");
    }

    void AAI::showPuzzleMap() {
        debug("*** showPuzzleMap ***");
    }

    void AAI::showRecruitmentDialog(const CGDwelling *dwelling, const CArmedInstance *dst, int level, QueryID queryID) {
        debug("*** showRecruitmentDialog ***");
    }

    void AAI::showShipyardDialog(const IShipyard * obj) {
        debug("*** showShipyardDialog ***");
    }

    void AAI::showTavernWindow(const CGObjectInstance * object, const CGHeroInstance * visitor, QueryID queryID) {
        debug("*** showTavernWindow ***");
    }

    void AAI::showThievesGuildWindow(const CGObjectInstance * obj) {
        debug("*** showThievesGuildWindow ***");
    }

    void AAI::showUniversityWindow(const IMarket *market, const CGHeroInstance *visitor, QueryID queryID) {
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

    std::optional<BattleAction> AAI::makeSurrenderRetreatDecision(const BattleID & battleID, const BattleStateInfoForRetreat & battleState) {
        return std::nullopt;
    }
}
