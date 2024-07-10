#pragma once

#include "Global.h"
#include "Config.h"
#include "lib/mapObjects/CArmedInstance.h"
#include "lib/mapObjects/CGHeroInstance.h"
#include "lib/mapObjects/CGTownInstance.h"
#include "server/ML/Stats.h"
#include "server/queries/BattleQueries.h"
#include "Stats.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {
    class DLL_LINKAGE ServerPlugin {
    public:
        ServerPlugin(CGameState * gs, Config & config);

        void setupBattleHook(const CGTownInstance *& town, ui32 & seed);

        void startBattleHook1(
            const CArmedInstance *&army1,
            const CArmedInstance *&army2,
            const CGHeroInstance *&hero1,
            const CGHeroInstance *&hero2
        );

        void startBattleHook2(const CGHeroInstance* heroes[2], std::shared_ptr<CBattleQuery> q);

        void endBattleHook(
            BattleResult * br,
            const CGHeroInstance * heroAttacker,
            const CGHeroInstance * heroDefender
        );

        // void preBattleHook1(
        //     const CArmedInstance *& army1,
        //     const CArmedInstance *& army2,
        //     const CGHeroInstance *& hero1,
        //     const CGHeroInstance *& hero2
        // );

    private:
        CGameState * gs;
        const Config config;
        std::vector<ConstTransitivePtr<CGHeroInstance>> allheroes;
        std::vector<ConstTransitivePtr<CGTownInstance>> alltowns;
        std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> allmachines;
        std::unique_ptr<Stats> stats;
        std::mt19937 rng;

        int herocounter = 0;
        int towncounter = 0;
        int battlecounter = 0;
        int redside = 0;
    };
}

VCMI_LIB_NAMESPACE_END
