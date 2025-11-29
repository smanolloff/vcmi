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
#include <string>
#include <functional>
#include <filesystem>
#include "AI/MMAI/schema/schema.h"

namespace ML {
    namespace fs = std::filesystem;

    constexpr auto AI_STUPIDAI = "StupidAI";
    constexpr auto AI_BATTLEAI = "BattleAI";
    constexpr auto AI_MMAI_USER = "MMAI_USER"; // for user-provided getAction (gym)
    constexpr auto AI_MMAI_MODEL = "MMAI_MODEL"; // for pre-trained model's getAction
    constexpr auto AI_MMAI_SCRIPT_SUMMONER = "MMAI_SCRIPT_SUMMONER";

    const std::vector<std::string> AIS = {
        AI_STUPIDAI,
        AI_BATTLEAI,
        AI_MMAI_USER,
        AI_MMAI_MODEL,
        AI_MMAI_SCRIPT_SUMMONER,
    };

    const std::vector<std::string> LOGLEVELS = {"trace", "debug", "info", "warn", "error"};
    const std::vector<std::string> ENCODINGS = {"default", "float"};

    // TODO: rename to left/right
    const std::vector<std::string> STATPERSPECTIVES = {"disabled", "red", "blue"};

    MMAI_DLL_LINKAGE MMAI::Schema::IModel* MakeScriptedModel(std::string keyword);
    MMAI_DLL_LINKAGE MMAI::Schema::IModel* MakeUserModel(
        int version,
        std::function<int(const MMAI::Schema::IState*)> getAction,
        std::function<double(const MMAI::Schema::IState*)> getState
    );

    struct MMAI_DLL_LINKAGE InitArgs {
        InitArgs() = delete;
        InitArgs(
            std::string mapname,
            MMAI::Schema::IModel * leftModel,
            MMAI::Schema::IModel * rightModel,
            int maxBattles,
            int seed,
            int randomHeroes,
            int randomObstacles,
            int townChance,
            int warmachineChance,
            int randomStackChance,
            int tightFormationChance,
            int randomTerrainChance,
            std::string battlefieldPattern,
            int manaMin,
            int manaMax,
            int swapSides,
            std::string loglevelGlobal,
            std::string loglevelAI,
            std::string loglevelStats,
            std::string statsMode,
            std::string statsStorage,
            int statsTimeout,
            int statsPersistFreq,
            bool headless
        ) : mapname(mapname)
          , leftModel(leftModel)
          , rightModel(rightModel)
          , maxBattles(maxBattles)
          , seed(seed)
          , randomHeroes(randomHeroes)
          , randomObstacles(randomObstacles)
          , townChance(townChance)
          , warmachineChance(warmachineChance)
          , randomStackChance(randomStackChance)
          , tightFormationChance(tightFormationChance)
          , randomTerrainChance(randomTerrainChance)
          , battlefieldPattern(battlefieldPattern)
          , manaMin(manaMin)
          , manaMax(manaMax)
          , swapSides(swapSides)
          , loglevelGlobal(loglevelGlobal)
          , loglevelAI(loglevelAI)
          , loglevelStats(loglevelStats)
          , statsMode(statsMode)
          , statsStorage(statsStorage == "-" ? statsStorage : fs::absolute(fs::path(statsStorage)).string())
          , statsTimeout(statsTimeout)
          , statsPersistFreq(statsPersistFreq)
          , headless(headless) {};

        MMAI::Schema::IModel * leftModel;
        MMAI::Schema::IModel * rightModel;

        const std::string leftModelFile;
        const std::string rightModelFile;
        const std::string mapname;
        const int maxBattles;
        const int seed;
        const int randomHeroes;
        const int randomObstacles;
        const int townChance;
        const int warmachineChance;
        const int randomStackChance;
        const int tightFormationChance;
        const int randomTerrainChance;
        const std::string battlefieldPattern;
        const int manaMin;
        const int manaMax;
        const int swapSides;
        const std::string loglevelGlobal;
        const std::string loglevelAI;
        const std::string loglevelStats;
        const std::string statsMode;
        const std::string statsStorage;
        const int statsTimeout;
        const int statsPersistFreq;
        const bool headless;
    };

    void MMAI_DLL_LINKAGE init_vcmi(InitArgs &a);
    void MMAI_DLL_LINKAGE start_vcmi();
    void MMAI_DLL_LINKAGE shutdown_vcmi();
}
[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
