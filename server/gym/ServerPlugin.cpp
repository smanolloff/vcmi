#include "ServerPlugin.h"
#include "ArtifactUtils.h"
#include "gameState/CGameState.h"
#include "mapObjects/CGHeroInstance.h"
#include "mapObjects/CGTownInstance.h"
#include "mapping/CMap.h"
#include "networkPacks/PacksForClientBattle.h"
#include "server/CGameHandler.h"
#include "server/CVCMIServer.h"

VCMI_LIB_NAMESPACE_BEGIN

namespace Gym {
    // static
    std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> InitWarMachines(CGameState * gs) {
        auto res = std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> {};
        for (const auto &h : gs->map->heroesOnMap) {
            res.insert({h, {
                ArtifactUtils::createNewArtifactInstance(ArtifactID::BALLISTA),
                ArtifactUtils::createNewArtifactInstance(ArtifactID::AMMO_CART),
                ArtifactUtils::createNewArtifactInstance(ArtifactID::FIRST_AID_TENT)
            }});
        }
        return res;
    }

    // static
    std::unique_ptr<Stats> InitStats(CGameState * gs, GymInfo gi) {
        return gi.statsMode == "disabled" ? nullptr : std::make_unique<Stats>(
            gs->map->heroesOnMap.size(),
            gi.statsStorage,
            gi.statsPersistFreq,
            gi.statsSampling,
            gi.statsScoreVar
        );
    }

    ServerPlugin::ServerPlugin(CGameState * gs, GymInfo & gi)
    : gs(gs)
    , gi(gi)
    , allheroes(gs->map->heroesOnMap)
    , alltowns(gs->map->towns)
    , allmachines(InitWarMachines(gs))
    , stats(InitStats(gs, gi))
    , pseudorng(std::mt19937(gi.seed))
    , truerng(std::random_device())
    {}

    void ServerPlugin::setupBattleHook(const CGTownInstance *& town, ui32 & seed) {
        if (gi.randomObstacles > 0 && (battlecounter % gi.randomObstacles == 0)) {
            seed = gi.trueRng // modification by reference
                ? truerng()
                : gs->getRandomGenerator().nextInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        }

        if (gi.townChance > 0) {
            auto dist = std::uniform_int_distribution<>(0, 99);
            auto roll = gi.trueRng ? dist(truerng) : dist(pseudorng);

            if (roll < gi.townChance) {
                if (towncounter % alltowns.size() == 0) {
                    towncounter = 0;
                    gi.trueRng
                        ? std::shuffle(alltowns.begin(), alltowns.end(), truerng)
                        : std::shuffle(alltowns.begin(), alltowns.end(), pseudorng);
                }

                town = alltowns.at(towncounter); // modification by reference
                ++towncounter;
            } else {
                town = nullptr; // modification by reference
            }
        }
    }

    void ServerPlugin::startBattleHook1(
        const CArmedInstance *&army1,
        const CArmedInstance *&army2,
        const CGHeroInstance *&hero1,
        const CGHeroInstance *&hero2
    ) {
        bool swappingSides = (gi.swapSides > 0 && (battlecounter % gi.swapSides) == 0);
        // printf("battlecounter: %d, swapSides: %d, rem: %d\n", battlecounter, gi.swapSides, battlecounter % gi.swapSides);

        battlecounter++;

        if (swappingSides)
            redside = !redside;

        // printf("gi.randomHeroes = %d\n", gi.randomHeroes);

        if (gi.randomHeroes > 0) {
            if (stats && gi.statsSampling) {
                auto [id1, id2] = stats->sample2(gi.statsMode == "red" ? redside : !redside);
                hero1 = allheroes.at(id1);
                hero2 = allheroes.at(id2);
                // printf("sampled: hero1: %d, hero2: %d (perspective: %s)\n", id1, id2, gh->statsMode.c_str());
            } else {
                if (allheroes.size() % 2 != 0)
                    throw std::runtime_error("Heroes size must be even");

                if (herocounter % allheroes.size() == 0) {
                    herocounter = 0;

                    gi.trueRng
                        ? std::shuffle(allheroes.begin(), allheroes.end(), truerng)
                        : std::shuffle(allheroes.begin(), allheroes.end(), pseudorng);

                    // for (int i=0; i<allheroes.size(); i++)
                    //  printf("allheroes[%d] = %s\n", i, allheroes.at(i)->getNameTextID().c_str());
                }
                // printf("herocounter = %d\n", herocounter);

                // XXX: heroes must be different (objects must have different tempOwner)
                // modification by reference
                hero1 = allheroes.at(herocounter);
                hero2 = allheroes.at(herocounter+1);

                if (battlecounter % gi.randomHeroes == 0)
                    herocounter += 2;
            }
        }

        if (gi.warmachineChance > 0) {
            // XXX: adding war machines by index of pre-created per-hero artifact instances
            // 0=ballista, 1=cart, 2=tent
            auto machineslots = std::map<ArtifactID, ArtifactPosition> {
                {ArtifactID::BALLISTA, ArtifactPosition::MACH1},
                {ArtifactID::AMMO_CART, ArtifactPosition::MACH2},
                {ArtifactID::FIRST_AID_TENT, ArtifactPosition::MACH3},
            };

            auto dist = std::uniform_int_distribution<>(0, 99);
            for (auto h : {hero1, hero2}) {
                for (auto m : allmachines.at(h)) {
                    auto it = machineslots.find(m->getTypeId());
                    if (it == machineslots.end())
                        throw std::runtime_error("Could not find warmachine");

                    auto apos = it->second;
                    auto roll = gi.trueRng ? dist(truerng) : dist(pseudorng);
                    if (roll < gi.warmachineChance) {
                        if (!h->getArt(apos)) const_cast<CGHeroInstance*>(h)->putArtifact(apos, m);
                    } else {
                        if (h->getArt(apos)) const_cast<CGHeroInstance*>(h)->removeArtifact(apos);
                    }
                }
            }
        }

        // if (gh->manaMin != gh->manaMax) {
        //  auto dist = std::uniform_int_distribution<>(manaMin, manaMax);
        //  for (auto h : {hero1, hero2}) {
        //      auto mana = gh->useTrueRng ? dist(gh->truerng) : dist(gh->pseudorng);
        //  }
        // }


        // Set temp owner of both heroes to player0 and player1
        // XXX: causes UB after battle, unless it is replayed (ok for training)
        // XXX: if redside=1 (right), hero2 should have owner=0 (red)
        //      if redside=0 (left), hero1 should have owner=0 (red)
        const_cast<CGHeroInstance*>(hero1)->tempOwner = PlayerColor(redside);
        const_cast<CGHeroInstance*>(hero2)->tempOwner = PlayerColor(!redside);

        // modification by reference
        army1 = static_cast<const CArmedInstance*>(hero1);
        army2 = static_cast<const CArmedInstance*>(hero2);
    }



    void ServerPlugin::startBattleHook2(const CGHeroInstance* heroes[2], std::shared_ptr<CBattleQuery> q) {
        auto dist = std::uniform_int_distribution<>(gi.minMana, gi.maxMana);

        for(int i : {0, 1}) {
            if(heroes[i]) {
                auto roll = gi.trueRng ? dist(truerng) : dist(pseudorng);
                q->initialHeroMana[i] = roll;
            }
        }
    }

    void ServerPlugin::endBattleHook(
        BattleResult * br,
        const CGHeroInstance * heroAttacker,
        const CGHeroInstance * heroDefender
    ) {
        // don't record stats for retreats (i.e. env resets)
        if (stats && br->result == EBattleResult::NORMAL) {
            stats->dataadd(
                redside,
                br->winner == BattleSide::ATTACKER,
                heroAttacker->exp,  // training map is designed such that exp = hero ID
                heroDefender->exp
            );
        }

        if (gi.maxBattles && battlecounter >= gi.maxBattles) {
            std::cout << "Hit battle limit of " << gi.maxBattles << ", will quit now...\n";
            if (stats) stats->dbpersist();
            exit(0); // FIXME: this causes OS errors (abrupt program termination)
            return;
        }
    }

}

VCMI_LIB_NAMESPACE_END
