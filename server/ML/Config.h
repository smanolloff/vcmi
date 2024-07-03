#pragma once

#include "CConfigHandler.h"
#include "StdInc.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    class Config {
    public:
        void init(SettingsStorage &settings) {
            maxBattles = settings["server"]["ML"]["maxBattles"].Integer();
            seed = settings["server"]["ML"]["seed"].Integer();
            randomHeroes = settings["server"]["ML"]["randomHeroes"].Integer();
            randomObstacles = settings["server"]["ML"]["randomObstacles"].Integer();
            townChance = settings["server"]["ML"]["townChance"].Integer();
            warmachineChance = settings["server"]["ML"]["warmachineChance"].Integer();
            swapSides = settings["server"]["ML"]["swapSides"].Integer();
            trueRng = settings["server"]["ML"]["trueRng"].Bool();
            manaMin = settings["server"]["ML"]["manaMin"].Integer();
            manaMax = settings["server"]["ML"]["manaMax"].Integer();

            statsSampling = settings["server"]["ML"]["statsSampling"].Integer();
            statsPersistFreq = settings["server"]["ML"]["statsPersistFreq"].Integer();
            statsStorage = settings["server"]["ML"]["statsStorage"].String();
            statsMode = settings["server"]["ML"]["statsMode"].String();
            statsScoreVar = settings["server"]["ML"]["statsScoreVar"].Float();
        }

        int maxBattles = 0;
        int seed = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        int swapSides = 0;
        bool trueRng = false;
        int manaMin = 0;
        int manaMax = 0;

        int statsSampling = 0;
        int statsPersistFreq = 0;
        std::string statsStorage = "-";
        std::string statsMode = "red";
        float statsScoreVar = 0.4;
    };
}

