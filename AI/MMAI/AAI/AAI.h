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

#pragma once

#include "lib/AI_Base.h"

namespace MMAI::AAI {
    class DLL_EXPORT AAI : public CAdventureAI {
    public:
        AAI();
        ~AAI();

        std::shared_ptr<CCallback> cb;
        bool human;
        PlayerColor playerID;
        std::string dllName;
        std::shared_ptr<CBattleGameInterface> battleAI;

        std::string getBattleAIName() const override;

        /*
         * Hybrid call-ins (conecrning both AAI and BAI)
         */

        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override;
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;

        /*
         * AAI call-ins
         */

        std::optional<BattleAction> makeSurrenderRetreatDecision(const BattleID &bid, const BattleStateInfoForRetreat &info) override;
        void advmapSpellCast(const CGHeroInstance * caster, SpellID spellID) override;
        void artifactAssembled(const ArtifactLocation & al) override;
        void artifactDisassembled(const ArtifactLocation & al) override;
        void artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) override;
        void artifactPut(const ArtifactLocation & al) override;
        void artifactRemoved(const ArtifactLocation & al) override;
        void askToAssembleArtifact(const ArtifactLocation & dst) override;
        void availableArtifactsChanged(const CGBlackMarket * bm = nullptr) override;
        void availableCreaturesChanged(const CGDwelling * town) override;
        void battleResultsApplied() override;
        void battleStartBefore(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override;
        void beforeObjectPropertyChanged(const SetObjectProperty * sop) override;
        void buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) override;
        void bulkArtMovementStart(size_t numOfArts) override;
        void centerView(int3 pos, int focusTime) override;
        void commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) override; //TODO
        void finish() override;
        void gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) override;
        void garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) override;
        void heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) override;
        void heroCreated(const CGHeroInstance *) override;
        void heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) override;
        void heroGotLevel(const CGHeroInstance *hero, PrimarySkill pskill, std::vector<SecondarySkill> &skills, QueryID queryID) override; //pskill is gained primary skill, interface has to choose one of given skills and call callback with selection id
        void heroInGarrisonChange(const CGTownInstance * town) override;
        void heroManaPointsChanged(const CGHeroInstance * hero) override;
        void heroMovePointsChanged(const CGHeroInstance * hero) override;
        void heroMoved(const TryMoveHero & details, bool verbose = true) override;
        void heroPrimarySkillChanged(const CGHeroInstance * hero, PrimarySkill which, si64 val) override;
        void heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) override;
        void heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) override;
        void heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) override;
        void initGameInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CCallback> CB) override;
        void initGameInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CCallback> CB, std::any baggage) override;
        void loadGame(BinaryDeserializer & h) override; //loading
        void newObject(const CGObjectInstance * obj) override;
        void objectPropertyChanged(const SetObjectProperty * sop) override;
        void objectRemoved(const CGObjectInstance * obj, const PlayerColor &initiator) override;
        void objectRemovedAfter() override; //eg. collected resource, picked artifact, beaten hero
        void playerBlocked(int reason, bool start) override;
        void playerBonusChanged(const Bonus & bonus, bool gain) override;
        void playerStartsTurn(PlayerColor player) override;
        void receivedResource() override;
        void requestRealized(PackageApplied * pa) override;
        void requestSent(const CPackForServer * pack, int requestID) override;
        void saveGame(BinarySerializer & h) override; //saving
        void showBlockingDialog(const std::string &text, const std::vector<Component> &components, QueryID askID, const int soundID, bool selection, bool cancel, bool safeToAutoaccept) override; //Show a dialog, player must take decision. If selection then he has to choose between one of given components, if cancel he is allowed to not choose. After making choice, CCallback::selectionMade should be called with number of selected component (1 - n) or 0 for cancel (if allowed) and askID.
        void showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) override; //all stacks operations between these objects become allowed, interface has to call onEnd when done
        void showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) override;
        void showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) override;
        void showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) override;
        void showMarketWindow(const IMarket *market, const CGHeroInstance *visitor, QueryID queryID) override;
        void showPuzzleMap() override;
        void showQuestLog() override;
        void showRecruitmentDialog(const CGDwelling *dwelling, const CArmedInstance *dst, int level, QueryID queryID) override;
        void showShipyardDialog(const IShipyard * obj) override;
        void showTavernWindow(const CGObjectInstance * object, const CGHeroInstance * visitor, QueryID queryID) override;
        void showTeleportDialog(const CGHeroInstance * hero, TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) override;
        void showThievesGuildWindow(const CGObjectInstance * obj) override;
        void showUniversityWindow(const IMarket *market, const CGHeroInstance *visitor, QueryID queryID) override;
        void showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) override;
        void tileHidden(const std::unordered_set<int3> & pos) override;
        void tileRevealed(const std::unordered_set<int3> & pos) override;
        void viewWorldMap() override;
        void yourTurn(QueryID queryID) override;

        /*
         * BAI call-ins
         */

        void actionFinished(const BattleID &bid, const BattleAction &action) override;
        void actionStarted(const BattleID &bid, const BattleAction &action) override;
        void activeStack(const BattleID &bid, const CStack *stack) override;
        void battleAttack(const BattleID &bid, const BattleAttack *ba) override;
        void battleCatapultAttacked(const BattleID &bid, const CatapultAttack &ca) override;
        void battleGateStateChanged(const BattleID &bid, const EGateState state) override;
        void battleLogMessage(const BattleID &bid, const std::vector<MetaString> &lines) override;
        void battleNewRound(const BattleID &bid) override;
        void battleNewRoundFirst(const BattleID &bid) override;
        void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> &obstacles) override;
        void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        void battleStackMoved(const BattleID &bid, const CStack *stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) override;
        void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect &sse) override;
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect &bte) override; //called for various one-shot effects
        void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> &units) override;
        void yourTacticPhase(const BattleID &bid, int distance) override;
    private:
        std::any baggage;

        // XXX: those mess up the regular log colors => leave blank
        std::string ansicolor = "";
        std::string ansireset = "";

        std::string color = "?";
        std::string addrstr = "?";
        std::string battleAiName;

        bool side;

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;
        void trace(const std::string &text) const;
        void log(const ELogLevel::ELogLevel level, const std::string &text) const;

        void error(const std::function<std::string()> &cb) const;
        void warn(const std::function<std::string()> &cb) const;
        void info(const std::function<std::string()> &cb) const;
        void debug(const std::function<std::string()> &cb) const;
        void trace(const std::function<std::string()> &cb) const;
        void log(const ELogLevel::ELogLevel level, const std::function<std::string()> &cb) const;

        template<typename ... Args> void error(const std::string &format, Args ... args) const;
        template<typename ... Args> void warn(const std::string &format, Args ... args) const;
        template<typename ... Args> void info(const std::string &format, Args ... args) const;
        template<typename ... Args> void debug(const std::string &format, Args ... args) const;
        template<typename ... Args> void trace(const std::string &format, Args ... args) const;
        template<typename ... Args> void log(const ELogLevel::ELogLevel level, const std::string &format, Args ... args) const;
        template<typename ... Args> void _log(const ELogLevel::ELogLevel level, const std::string &format, Args ... args) const;
    };
}
