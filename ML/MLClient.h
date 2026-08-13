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
#include "AI/MMAI/schema/base.h"

namespace ML {
    constexpr auto AI_STUPIDAI = "StupidAI";
    constexpr auto AI_BATTLEAI = "BattleAI";
    constexpr auto AI_MMAI_USER = "MMAI_USER"; // for user-provided getAction (gym)
    constexpr auto AI_MMAI_MODEL = "MMAI_MODEL"; // for pre-trained model's getAction
    constexpr auto AI_MMAI_BATTLEAI = "MMAI_BATTLEAI";
    constexpr auto AI_VIPBOT = "VIPBot"; // protects VIP shooters
    constexpr auto AI_HARBOT = "HARBot"; // does hit-and-run

    const std::vector<std::string> AIS = {
        AI_STUPIDAI,
        AI_BATTLEAI,
        AI_MMAI_USER,
        AI_MMAI_MODEL,
        AI_MMAI_BATTLEAI,
        AI_VIPBOT,
        AI_HARBOT,
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
        const std::string leftModelFile;
        const std::string rightModelFile;
        const float temperature;
        const std::string mapname;
        const int maxBattles;
        const int seed;
        const int randomHeroes;
        const int randomObstacles;
        const int townChance;
        const int warmachineChance;
        const bool mirrorArmies;
        const bool randomArmies;
        const int randomArmyValueMin;
        const int randomArmyValueMax;
        const int randomArmyTargetVar;
        const int tightFormationChance;
        const int randomTerrainChance;
        const bool leftVip;
        const bool rightVip;
        const bool leftHar;
        const bool rightHar;
        const std::string battlefieldPattern;
        const int manaMin;
        const int manaMax;
        const int randomPrimarySkills;
        const int swapSides;
        const std::string loglevelGlobal;
        const std::string loglevelAI;
        const std::string loglevelNetwork;
        const std::string loglevelStats;
        const std::string statsMode;
        const std::string statsStorage;
        const int statsTimeout;
        const int statsPersistFreq;
        const bool headless;
        const bool hotseat;
    };

    void MMAI_DLL_LINKAGE init_vcmi(
        MMAI::Schema::IModel * leftModel,
        MMAI::Schema::IModel * rightModel,
        const InitArgs & a);

    void MMAI_DLL_LINKAGE start_vcmi();
    void MMAI_DLL_LINKAGE shutdown_vcmi();
}
[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
