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
            mirrorArmies = settings["server"]["ML"]["mirrorArmies"].Bool();
            randomArmies = settings["server"]["ML"]["randomArmies"].Bool();
            randomArmyValueMin = settings["server"]["ML"]["randomArmyValueMin"].Integer();
            randomArmyValueMax = settings["server"]["ML"]["randomArmyValueMax"].Integer();
            randomArmyTargetVar = settings["server"]["ML"]["randomArmyTargetVar"].Integer();
            leftTargetMod = settings["server"]["ML"]["leftTargetMod"].Float();
            rightTargetMod = settings["server"]["ML"]["rightTargetMod"].Float();
            leftUniformChance = settings["server"]["ML"]["leftUniformChance"].Integer();
            rightUniformChance = settings["server"]["ML"]["rightUniformChance"].Integer();
            leftWhitelist = settings["server"]["ML"]["leftWhitelist"].String();
            rightWhitelist = settings["server"]["ML"]["rightWhitelist"].String();
            tightFormationChance = settings["server"]["ML"]["tightFormationChance"].Integer();
            creatureBankChance = settings["server"]["ML"]["creatureBankChance"].Integer();
            randomTerrainChance = settings["server"]["ML"]["randomTerrainChance"].Integer();
            leftVip = settings["server"]["ML"]["leftVip"].Bool();
            rightVip = settings["server"]["ML"]["rightVip"].Bool();
            leftHar = settings["server"]["ML"]["leftHar"].Bool();
            rightHar = settings["server"]["ML"]["rightHar"].Bool();
            battlefieldPattern = settings["server"]["ML"]["battlefieldPattern"].String();
            swapSides = settings["server"]["ML"]["swapSides"].Integer();
            manaMin = settings["server"]["ML"]["manaMin"].Integer();
            manaMax = settings["server"]["ML"]["manaMax"].Integer();
            randomPrimarySkills = settings["server"]["ML"]["randomPrimarySkills"].Integer();

            statsPersistFreq = settings["server"]["ML"]["statsPersistFreq"].Integer();
            statsTimeout = settings["server"]["ML"]["statsTimeout"].Integer();
            statsStorage = settings["server"]["ML"]["statsStorage"].String();
            statsMode = settings["server"]["ML"]["statsMode"].String();
        }

        std::string battlefieldPattern;

        int maxBattles = 0;
        int rngSeed = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        bool mirrorArmies = false;
        bool randomArmies = false;
        int randomArmyValueMin = 0;
        int randomArmyValueMax = 0;
        int randomArmyTargetVar = 0;
        double leftTargetMod = 1.0;
        double rightTargetMod = 1.0;
        int leftUniformChance = 0;
        int rightUniformChance = 0;
        std::string leftWhitelist;
        std::string rightWhitelist;
        int tightFormationChance = 0;
        int creatureBankChance = 0;
        int randomTerrainChance = 0;
        bool leftVip = false;
        bool rightVip = false;
        bool leftHar = false;
        bool rightHar = false;
        int swapSides = 0;
        int manaMin = 0;
        int manaMax = 0;
        int randomPrimarySkills = 0;

        int statsPersistFreq = 0;
        int statsTimeout = 0;
        std::string statsStorage = "-";
        std::string statsMode = "red";
    };
}

