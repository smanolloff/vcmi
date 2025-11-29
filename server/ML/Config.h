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

#include "CConfigHandler.h"
#include "StdInc.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    class Config {
    public:
        void init(SettingsStorage &settings) {
            maxBattles = settings["server"]["ML"]["maxBattles"].Integer();
            rngSeed = settings["server"]["ML"]["seed"].Integer();
            randomHeroes = settings["server"]["ML"]["randomHeroes"].Integer();
            randomObstacles = settings["server"]["ML"]["randomObstacles"].Integer();
            townChance = settings["server"]["ML"]["townChance"].Integer();
            warmachineChance = settings["server"]["ML"]["warmachineChance"].Integer();
            randomStackChance = settings["server"]["ML"]["randomStackChance"].Integer();
            tightFormationChance = settings["server"]["ML"]["tightFormationChance"].Integer();
            randomTerrainChance = settings["server"]["ML"]["randomTerrainChance"].Integer();
            battlefieldPattern = settings["server"]["ML"]["battlefieldPattern"].String();
            swapSides = settings["server"]["ML"]["swapSides"].Integer();
            manaMin = settings["server"]["ML"]["manaMin"].Integer();
            manaMax = settings["server"]["ML"]["manaMax"].Integer();

            statsPersistFreq = settings["server"]["ML"]["statsPersistFreq"].Integer();
            statsTimeout = settings["server"]["ML"]["statsTimeout"].Integer();
            statsStorage = settings["server"]["ML"]["statsStorage"].String();
            statsMode = settings["server"]["ML"]["statsMode"].String();
        }

        std::string battlefieldPattern = "";

        int maxBattles = 0;
        int rngSeed = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        int randomStackChance = 0;
        int tightFormationChance = 0;
        int randomTerrainChance = 0;
        int swapSides = 0;
        int manaMin = 0;
        int manaMax = 0;

        int statsPersistFreq = 0;
        int statsTimeout = 0;
        std::string statsStorage = "-";
        std::string statsMode = "red";
    };
}

