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

VCMI_LIB_NAMESPACE_BEGIN

class CGameHandler; // forward declaration

namespace ML {
    class HeroPool {
    public:
        HeroPool(int id, std::string name)
        : id(id), name(name) {}

        const int id;
        const std::string name;
        std::vector<CGHeroInstance*> heroes = {};
        int counter = 0;
    };

    class DLL_LINKAGE ServerPlugin {
    public:
        ServerPlugin(CGameHandler * gh, CGameState * gs, Config & config_);

        void setupBattleHook(
            const CGTownInstance *& town,
            TerrainId & terrain,
            BattleField & terType,
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

    private:
        CGameHandler * gh;
        Config config;
        std::vector<CGTownInstance*> alltowns;
        std::map<std::string, HeroPool> heropools;
        const std::map<const BattleFieldInfo*, std::vector<const TerrainType*>> battleterrains;
        const std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> allmachines;
        const std::vector<CreatureID> allcreatures;
        const std::vector<CreatureID> allshooters;
        const std::vector<CreatureID> allguards;
        std::unique_ptr<Stats> stats;  // XXX: must come after heropools
        std::mt19937 rng;

        std::map<CreatureID, int> creatureValues;

        CGHeroInstance* vipHero1 = nullptr;
        CGHeroInstance* vipHero2 = nullptr;

        int towncounter = 0;
        int battlecounter = 0;
        int poolcounter = 0;
        int redside = 0;

        void handleRandomHeroes(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void handleVipShooters(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void _setVipArmy(CGHeroInstance * heroA, CGHeroInstance * heroB);

        void handleRandomStacks(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleWarmachines(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleTightFormation(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleMinMaxMana(const CGHeroInstance * hero1, const CGHeroInstance * hero2);
        void handleSwapSides(const CGHeroInstance * hero1, const CGHeroInstance * hero2);


    };
}

VCMI_LIB_NAMESPACE_END
