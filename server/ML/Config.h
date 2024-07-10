#pragma once

#include "CConfigHandler.h"
#include "StdInc.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    class Config {
    public:
        void init(SettingsStorage &settings) {
            maxBattles = settings["server"]["ML"]["maxBattles"].Integer();
            randomHeroes = settings["server"]["ML"]["randomHeroes"].Integer();
            randomObstacles = settings["server"]["ML"]["randomObstacles"].Integer();
            townChance = settings["server"]["ML"]["townChance"].Integer();
            warmachineChance = settings["server"]["ML"]["warmachineChance"].Integer();
            swapSides = settings["server"]["ML"]["swapSides"].Integer();
            manaMin = settings["server"]["ML"]["manaMin"].Integer();
            manaMax = settings["server"]["ML"]["manaMax"].Integer();

            statsPersistFreq = settings["server"]["ML"]["statsPersistFreq"].Integer();
            statsTimeout = settings["server"]["ML"]["statsTimeout"].Integer();
            statsStorage = settings["server"]["ML"]["statsStorage"].String();
            statsMode = settings["server"]["ML"]["statsMode"].String();
        }

        int maxBattles = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        int swapSides = 0;
        int manaMin = 0;
        int manaMax = 0;

        int statsPersistFreq = 0;
        int statsTimeout = 0;
        std::string statsStorage = "-";
        std::string statsMode = "red";
    };
}

