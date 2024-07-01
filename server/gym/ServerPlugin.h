#pragma once

#include "Global.h"
#include "GymInfo.h"
#include "lib/mapObjects/CArmedInstance.h"
#include "mapObjects/CGTownInstance.h"
#include "stats.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace Gym {
    class ServerPlugin {
    public:
        ServerPlugin(CGameHandler * gh, GymInfo & gi);

        void setupBattleHook(const CGTownInstance *& town, ui32 & seed);

        void startBattleHook1(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        // void preBattleHook1(
        //     const CArmedInstance *& army1,
        //     const CArmedInstance *& army2,
        //     const CGHeroInstance *& hero1,
        //     const CGHeroInstance *& hero2
        // );

    private:
        CGameHandler* gh;
        const GymInfo gi;
        const std::vector<ConstTransitivePtr<CGHeroInstance>> allheroes;
        const std::vector<ConstTransitivePtr<CGTownInstance>> alltowns;
        const std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> allmachines;
        const std::unique_ptr<Stats> stats;
        const std::mt19937 pseudorng;
        std::random_device truerng;

        ui32 lastSeed = 0;
        int herocounter = 0;
        int towncounter = 0;
        int battlecounter = 0;
        int redside = 0;
        std::string statsMode = "disabled";
        std::string statsStorage = "-";
        float statsScoreVar = 0.4;
    };
}

VCMI_LIB_NAMESPACE_END
