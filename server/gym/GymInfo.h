#pragma once

#include "CConfigHandler.h"
#include "StdInc.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace Gym {
    class GymInfo {
    public:
        void init(SettingsStorage &settings) {
            maxBattles = settings["server"]["maxBattles"].Integer();
            seed = settings["server"]["seed"].Integer();
            randomHeroes = settings["server"]["randomHeroes"].Integer();
            randomObstacles = settings["server"]["randomObstacles"].Integer();
            townChance = settings["server"]["townChance"].Integer();
            warmachineChance = settings["server"]["warmachineChance"].Integer();
            swapSides = settings["server"]["swapSides"].Integer();
            trueRng = settings["server"]["trueRng"].Bool();
            minMana = settings["server"]["minMana"].Integer(); // TODO (simo)
            maxMana = settings["server"]["maxMana"].Integer(); // TODO (simo)

            statsSampling = settings["server"]["statsSampling"].Integer();
            statsPersistFreq = settings["server"]["statsPersistFreq"].Integer();
            statsStorage = settings["server"]["statsStorage"].String();
            statsMode = settings["server"]["statsMode"].String();
            statsScoreVar = settings["server"]["statsScoreVar"].Float();
        }

        int maxBattles = 0;
        int seed = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        int swapSides = 0;
        bool trueRng = false;
        int minMana = 0;
        int maxMana = 0;

        int statsSampling = 0;
        int statsPersistFreq = 0;
        std::string statsStorage = "-";
        std::string statsMode = "red";
        float statsScoreVar = 0.4;
    };
}

