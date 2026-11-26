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

#include "Global.h"
#include "Config.h"
#include "lib/mapObjects/army/CArmedInstance.h"
#include "lib/mapObjects/CGHeroInstance.h"
#include "lib/mapObjects/CGTownInstance.h"
// #include "server/CGameHandler.h"
#include "server/ML/Stats.h"
#include "Stats.h"

class CGameHandler; // forward declaration
class IGameInfoCallback;
struct BattleLayout;

namespace ML {
    class BattleCheckpoint;

    struct DLL_LINKAGE RolloutResult {
        EBattleResult result;
        BattleSide winner;
    };

    DLL_LINKAGE std::shared_ptr<const BattleCheckpoint> captureClientBattle(const CGameState & state, BattleID battleID);
    DLL_LINKAGE void restoreClientBattle(CGameState & state, const BattleCheckpoint & checkpoint);

    class HeroPool {
    public:
        HeroPool(int id, std::string name)
        : id(id), name(name) {}

        const int id;
        const std::string name;
        std::vector<CGHeroInstance*> heroes = {};
        int counter = 0;
    };

    // dims: per-owner pools (assume there are only 2 players on the map)
    using HeroPools = std::array<std::map<std::string, HeroPool>, 2>;

    using BattleTerrains = std::vector<std::pair<const BattleFieldInfo *, std::vector<const TerrainType *>>>;

    class DLL_LINKAGE ServerPlugin {
    public:
        ServerPlugin(CGameHandler * gh, CGameState * gs, Config & config_);

        void setupBattleHook(
            const IGameInfoCallback & gameInfo,
            const CArmedInstance * attacker,
            const CArmedInstance * defender,
            const CGTownInstance *& town,
            TerrainId & terrain,
            BattleField & terType,
            BattleLayout & layout,
            ui32 & seed
        );

        void startBattleHook(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void endBattleHook(
            BattleResult * br,
            const CGHeroInstance * heroAttacker,
            const CGHeroInstance * heroDefender
        );

        /// Capture a decision state without changing rollout mode.
        std::shared_ptr<const BattleCheckpoint> captureCheckpoint(BattleID battleID) const;
        /// Capture a decision state and enable speculative results for this battle.
        std::shared_ptr<const BattleCheckpoint> checkpoint(BattleID battleID);
        /// Begin client-driven rollouts at the current decision.
        void beginRollout(BattleID battleID, std::function<void(RolloutResult)> onTerminal);
        /// Restore a decision state and discard any speculative terminal result.
        void restore(const BattleCheckpoint & checkpoint);
        /// Restore the chosen root and resume normal battle finalization.
        void finishRollout(const BattleCheckpoint & root);
        bool isRollingOut(BattleID battleID) const;
        std::optional<RolloutResult> rolloutResult(BattleID battleID) const;
        bool captureRolloutResult(BattleID battleID, EBattleResult result, BattleSide winner);

        Config config;
    private:
        CGameHandler * gh;
        CGameState & gameState;
        std::optional<BattleID> rolloutBattle;
        std::optional<BattleID> endingBattle;
        std::optional<RolloutResult> terminalResult;
        std::function<void(RolloutResult)> notifyRolloutEnded;
        std::vector<CGTownInstance*> alltowns;
        HeroPools heropools;
        const BattleTerrains battleterrains;
        const std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> allmachines;
        const std::vector<CreatureID> allcreatures;
        const std::vector<CreatureID> leftCreatures;
        const std::vector<CreatureID> rightCreatures;
        const std::vector<CreatureID> allshooters;
        const std::vector<CreatureID> allguards;
        std::unique_ptr<Stats> stats;  // XXX: must come after heropools
        std::mt19937 rng;

        std::map<CreatureID, int> creatureValues;

        int towncounter = 0;
        int battlecounter = 0;
        int poolcounter = 0;
        int redside = 0;
        bool creatureBankBattle = false;

        void handleRandomHeroes(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void handleRandomArmies(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void handleMirrorArmies(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void handleWarmachines(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleTightFormation(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleMinMaxMana(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleSwapSides(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleRandomPrimarySkills(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleRandomSecondarySkills(const CGHeroInstance * hero1, const CGHeroInstance * hero2);


    };
}
