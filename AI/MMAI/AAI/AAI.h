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

#include "export.h"
#include "BAI/BAI.h"

namespace MMAI {
    class DLL_EXPORT AAI : public CAdventureAI {
        Export::Baggage * baggage = new Export::Baggage(nullptr);
        std::shared_ptr<BAI> bai;

        std::string colorstr = "?";
        std::string colorprint = "\033[0m\033[30m\033[107m?\033[0m"; // black on white
        std::string battleAiName;
        Export::F_GetAction getActionOrig;
        Export::F_GetAction getActionWrapper;
        Export::Action getNonRenderAction(const Export::Result* result);

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;
    public:
        AAI();
        virtual ~AAI();

        std::shared_ptr<CCallback> cb;

        // impl CAdventureAI
        std::string getBattleAIName() const override;
        void battleEnd(const BattleResult * br, QueryID queryID) override;
        void battleStart(const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, bool side, bool replayAllowed) override;

        // impl CGlobalAI
        // (nothing)

        // impl CGameInterface
        void saveGame(BinarySerializer & h, const int version) override; //saving
        void loadGame(BinaryDeserializer & h, const int version) override; //loading
        void commanderGotLevel(const CCommanderInstance * commander, std::vector<ui32> skills, QueryID queryID) override; //TODO
        void finish() override;
        void heroGotLevel(const CGHeroInstance * hero, PrimarySkill::PrimarySkill pskill, std::vector<SecondarySkill> & skills, QueryID queryID) override; //pskill is gained primary skill, interface has to choose one of given skills and call callback with selection id
        void initGameInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CCallback> CB) override;
        void initGameInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CCallback> CB, std::any baggage) override;
        void showBlockingDialog(const std::string & text, const std::vector<Component> & components, QueryID askID, const int soundID, bool selection, bool cancel) override; //Show a dialog, player must take decision. If selection then he has to choose between one of given components, if cancel he is allowed to not choose. After making choice, CCallback::selectionMade should be called with number of selected component (1 - n) or 0 for cancel (if allowed) and askID.
        void showGarrisonDialog(const CArmedInstance * up, const CGHeroInstance * down, bool removableUnits, QueryID queryID) override; //all stacks operations between these objects become allowed, interface has to call onEnd when done
        void showMapObjectSelectDialog(QueryID askID, const Component & icon, const MetaString & title, const MetaString & description, const std::vector<ObjectInstanceID> & objects) override;
        void showTeleportDialog(TeleportChannelID channel, TTeleportExitsList exits, bool impassable, QueryID askID) override;
        void showWorldViewEx(const std::vector<ObjectPosInfo> & objectPositions, bool showTerrain) override;
        void yourTurn() override;

        // impl CBattleGameInterface
        // (nothing)

        // impl IBattleEventsReceiver
        // (nothing)

        // impl IGameEventsReceiver
        void advmapSpellCast(const CGHeroInstance * caster, int spellID) override;
        void artifactAssembled(const ArtifactLocation & al) override;
        void artifactDisassembled(const ArtifactLocation & al) override;
        void artifactMoved(const ArtifactLocation & src, const ArtifactLocation & dst) override;
        void artifactPut(const ArtifactLocation & al) override;
        void artifactRemoved(const ArtifactLocation & al) override;
        void availableArtifactsChanged(const CGBlackMarket * bm = nullptr) override;
        void availableCreaturesChanged(const CGDwelling * town) override;
        void battleResultsApplied() override;
        void beforeObjectPropertyChanged(const SetObjectProperty * sop) override;
        void buildChanged(const CGTownInstance * town, BuildingID buildingID, int what) override;
        void centerView(int3 pos, int focusTime) override;
        void gameOver(PlayerColor player, const EVictoryLossCheckResult & victoryLossCheckResult) override;
        void garrisonsChanged(ObjectInstanceID id1, ObjectInstanceID id2) override;
        void heroBonusChanged(const CGHeroInstance * hero, const Bonus & bonus, bool gain) override;
        void heroCreated(const CGHeroInstance *) override;
        void heroInGarrisonChange(const CGTownInstance * town) override;
        void heroManaPointsChanged(const CGHeroInstance * hero) override;
        void heroMovePointsChanged(const CGHeroInstance * hero) override;
        void heroMoved(const TryMoveHero & details, bool verbose = true) override;
        void heroPrimarySkillChanged(const CGHeroInstance * hero, int which, si64 val) override;
        void heroSecondarySkillChanged(const CGHeroInstance * hero, int which, int val) override;
        void heroVisit(const CGHeroInstance * visitor, const CGObjectInstance * visitedObj, bool start) override;
        void heroVisitsTown(const CGHeroInstance * hero, const CGTownInstance * town) override;
        void newObject(const CGObjectInstance * obj) override;
        void objectPropertyChanged(const SetObjectProperty * sop) override;
        void objectRemoved(const CGObjectInstance * obj) override;
        void playerBlocked(int reason, bool start) override;
        void playerBonusChanged(const Bonus & bonus, bool gain) override;
        void receivedResource() override;
        void requestRealized(PackageApplied * pa) override;
        void requestSent(const CPackForServer * pack, int requestID) override;
        void showHillFortWindow(const CGObjectInstance * object, const CGHeroInstance * visitor) override;
        void showInfoDialog(EInfoWindowMode type, const std::string & text, const std::vector<Component> & components, int soundID) override;
        void showMarketWindow(const IMarket * market, const CGHeroInstance * visitor) override;
        void showPuzzleMap() override;
        void showRecruitmentDialog(const CGDwelling * dwelling, const CArmedInstance * dst, int level) override;
        void showShipyardDialog(const IShipyard * obj) override;
        void showTavernWindow(const CGObjectInstance * townOrTavern) override;
        void showThievesGuildWindow(const CGObjectInstance * obj) override;
        void showUniversityWindow(const IMarket * market, const CGHeroInstance * visitor) override;
        void tileHidden(const std::unordered_set<int3> & pos) override;
        void tileRevealed(const std::unordered_set<int3> & pos) override;

        // Impl ???
        // part of CPlayerInterface, but we don't inherit it anywhere in the chain...
        void heroExchangeStarted(ObjectInstanceID hero1, ObjectInstanceID hero2, QueryID query) override;

        //
        // Unimplemented methods
        //

        // These are part of CAdventureAI, but VCAI does not implement them
        /*
        std::shared_ptr<CBattleGameInterface> battleAI;
        std::shared_ptr<CBattleCallback> cbc;
        virtual void activeStack(const CStack * stack) override;
        virtual void yourTacticPhase(int distance) override;
        virtual void battleNewRound(int round) override;
        virtual void battleCatapultAttacked(const CatapultAttack & ca) override;
        virtual void battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override;
        virtual void battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged) override;
        virtual void actionStarted(const BattleAction &action) override;
        virtual void battleNewRoundFirst(int round) override;
        virtual void actionFinished(const BattleAction &action) override;
        virtual void battleStacksEffectsSet(const SetStackEffect & sse) override;
        virtual void battleObstaclesChanged(const std::vector<ObstacleChanges> & obstacles) override;
        virtual void battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        virtual void battleAttack(const BattleAttack *ba) override;
        virtual void battleSpellCast(const BattleSpellCast *sc) override;
        virtual void battleEnd(const BattleResult *br, QueryID queryID) override;
        virtual void battleUnitsChanged(const std::vector<UnitChanges> & units) override;
        */

        // These are part of CGlobalAI, but VCAI does not implement them
        /*
        std::shared_ptr<Environment> env;
        */

        // These are part of CGameInterface, but VCAI does not implement them
        /*
        virtual std::optional<BattleAction> makeSurrenderRetreatDecision(const BattleStateInfoForRetreat & battleState) { return std::nullopt; }
        */

        // These are part of CBattleGameInterface, but VCAI does not implement them
        /*
        bool human;
        PlayerColor playerID;
        std::string dllName;
        virtual void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB){};
        virtual void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences){};
        virtual void activeStack(const CStack * stack)=0; //called when it's turn of that stack
        virtual void yourTacticPhase(int distance)=0; //called when interface has opportunity to use Tactics skill -> use cb->battleMakeTacticAction from this function
        */

        // These are part of IBattleEventsReceiver, but VCAI does not implement them
        /*
        virtual void actionFinished(const BattleAction &action){};//occurs AFTER every action taken by any stack or by the hero
        virtual void actionStarted(const BattleAction &action){};//occurs BEFORE every action taken by any stack or by the hero
        virtual void battleAttack(const BattleAttack *ba){}; //called when stack is performing attack
        virtual void battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged){}; //called when stack receives damage (after battleAttack())
        virtual void battleEnd(const BattleResult *br, QueryID queryID){};
        virtual void battleNewRoundFirst(int round){}; //called at the beginning of each turn before changes are applied;
        virtual void battleNewRound(int round){}; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
        virtual void battleLogMessage(const std::vector<MetaString> & lines){};
        virtual void battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport){};
        virtual void battleSpellCast(const BattleSpellCast *sc){};
        virtual void battleStacksEffectsSet(const SetStackEffect & sse){};//called when a specific effect is set to stacks
        virtual void battleTriggerEffect(const BattleTriggerEffect & bte){}; //called for various one-shot effects
        virtual void battleStartBefore(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) {}; //called just before battle start
        virtual void battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed){}; //called by engine when battle starts; side=0 - left, side=1 - right
        virtual void battleUnitsChanged(const std::vector<UnitChanges> & units){};
        virtual void battleObstaclesChanged(const std::vector<ObstacleChanges> & obstacles){};
        virtual void battleCatapultAttacked(const CatapultAttack & ca){}; //called when catapult makes an attack
        virtual void battleGateStateChanged(const EGateState state){};
        */

        // These are part of IGameEventsReceiver, but VCAI does not implement them
        /*
        virtual void bulkArtMovementStart(size_t numOfArts) {};
        virtual void askToAssembleArtifact(const ArtifactLocation & dst) {};
        virtual void viewWorldMap(){};
        virtual void showThievesGuildWindow (const CGObjectInstance * obj){};
        virtual void showQuestLog(){};
        virtual void centerView (int3 pos, int focusTime){};
        virtual void objectRemovedAfter(){}; //eg. collected resource, picked artifact, beaten hero
        virtual void playerStartsTurn(PlayerColor player){};
        */
    };
}

