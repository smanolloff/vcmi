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

#include "ServerPlugin.h"
#include "BattleFieldHandler.h"
#include "Global.h"
#include "TerrainHandler.h"
#include "constants/EntityIdentifiers.h"
#include "gameState/CGameState.h"
#include "mapObjects/CGHeroInstance.h"
#include "mapObjects/CGTownInstance.h"
#include "mapping/CMap.h"
#include "networkPacks/PacksForClient.h"
#include "networkPacks/PacksForClientBattle.h"
#include "networkPacks/StackLocation.h"
#include "server/CGameHandler.h"
#include "server/CVCMIServer.h"
#include "vstd/RNG.h"
#include <algorithm>
#include <array>
#include <boost/range/numeric.hpp>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <stdexcept>
#include <regex>

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {

namespace {

    #define ML_VERBOSE(arg) if (IsMLVerbose()) std::cout << arg;

    inline bool IsMLVerbose()
    {
        static const bool value = []
        {
            const char * envvar = std::getenv("ML_VERBOSE");
            return envvar != nullptr && std::strcmp(envvar, "1") == 0;
        }();
        return value;
    }

    int CalculateValue(const CCreature * cr)
    {
        /*
         * Formula:
         * 10 * (A + B) * C * D1 * D2 * ... * Dn
         *
         * A = <offensive factor>
         * B = <defensive factor>
         * C = <speed factor>
         * D* = <bonus factor>
         */

        auto att = cr->getBaseAttack();
        auto def = cr->getBaseDefense();
        auto dmg = (cr->getBaseDamageMax() + cr->getBaseDamageMin()) / 2.0;
        auto hp = cr->getBaseHitPoints();
        auto spd = cr->getBaseSpeed();
        auto shooter = cr->hasBonusOfType(BonusType::SHOOTER);
        auto bonuses = cr->getAllBonuses(Selector::all);

        auto a = 3 * dmg * (1 + std::min(4.0, 0.05 * att));
        auto b = hp / (1 - std::min(0.7, 0.025 * def));
        auto c = spd ? std::log(spd * 2) : 0.5;
        auto d = shooter ? 1.5 : 1.0;

        for(const auto & bonus : *bonuses)
        {
            switch(bonus->type)
            {
                case BonusType::ADDITIONAL_ATTACK:
                    d += (shooter ? 0.5 : 0.3);
                    break;
                case BonusType::ADDITIONAL_RETALIATION:
                    d += (bonus->val * 0.1);
                    break;
                case BonusType::ATTACKS_ALL_ADJACENT:
                    d += 0.2;
                    break;
                case BonusType::BLOCKS_RETALIATION:
                    d += 0.3;
                    break;
                case BonusType::DEATH_STARE:
                    d += (bonus->val * 0.02); // 10% = 0.2
                    break;
                case BonusType::DOUBLE_DAMAGE_CHANCE:
                    d += (bonus->val * 0.005); // 20% = 0.1
                    break;
                case BonusType::ENEMY_DEFENCE_REDUCTION:
                    d += (bonus->val * 0.0025); // 40% = 0.1
                    break;
                case BonusType::FIRE_SHIELD:
                    d += (bonus->val * 0.003); // 20% = 0.1
                    break;
                case BonusType::FLYING:
                    d += 0.1;
                    break;
                case BonusType::LIFE_DRAIN:
                    d += (bonus->val * 0.003); // 100% = 0.3
                    break;
                case BonusType::NO_DISTANCE_PENALTY:
                    d += 0.5;
                    break;
                case BonusType::NO_MELEE_PENALTY:
                    d += 0.1;
                    break;
                case BonusType::THREE_HEADED_ATTACK:
                    d += 0.05;
                    break;
                case BonusType::TWO_HEX_ATTACK_BREATH:
                    d += 0.1;
                    break;
                case BonusType::UNLIMITED_RETALIATIONS:
                    d += 0.2;
                    break;
                case BonusType::SPELL_LIKE_ATTACK:
                    switch(bonus->subtype.as<SpellID>())
                    {
                        case SpellID::DEATH_CLOUD:
                            d += 0.2;
                    }
                    break;
                case BonusType::SPELL_AFTER_ATTACK:
                    switch(bonus->subtype.as<SpellID>())
                    {
                        case SpellID::BLIND:
                        case SpellID::STONE_GAZE:
                        case SpellID::PARALYZE:
                            d += (bonus->val * 0.01); // 20% = 0.2
                            break;
                        case SpellID::BIND:
                            d += (bonus->val * 0.001); // 100% = 0.1
                            break;
                        case SpellID::WEAKNESS:
                            d += (bonus->val * 0.001); // 100% = 0.1
                            break;
                        case SpellID::AGE:
                            d += (bonus->val * 0.005); // 20% = 0.1
                            break;
                        case SpellID::CURSE:
                            d += (bonus->val * 0.0025); // 20% = 0.05
                    }
            }
        }

        // Multiply by 10 to reduce the integer rounding for weak units
        // (e.g. peasant 7.48 => 7 is a lot, 74.8 => 75 is OK)
        auto res = static_cast<int>(std::round(10 * (a + b) * c * d));
        return res;
    }

    std::map<CreatureID, int> InitCreatureValues()
    {
        std::map<CreatureID, int> values;

        for(const auto & creature : LIBRARY->creh->objects)
            if(creature)
                values.try_emplace(creature->getId(), CalculateValue(creature.get()));

        return values;
    }

    /*
     * Organize heroes into pools based on hero name
     * Format: "hero_<INT>_pool_<STR>"
     * Example: "hero_5123_pool_150k"
     */
    HeroPools InitHeroPools(CGameState* gs) {
        auto pattern = std::regex(R"(^hero_\d+_pool_([0-9A-Za-z]+)$)");
        auto res = HeroPools{};
        int counter = 0;

        // the order in which pools for side0 and side1 arrive may be different
        // => use a central lookup table
        int poolid = 0;
        std::unordered_map<std::string, int> poolids;

        for (const auto &heroid : gs->getMap().getHeroesOnMap()) {
            auto * hero = gs->getHero(heroid);

            const auto poolowner = hero->tempOwner.num;

            if (poolowner != 0 && poolowner != 1)
                throw std::runtime_error("Expected hero tempOwner 0 or 1, got: " + std::to_string(poolowner));

            std::string poolname = "default";  // fallback poolname
            std::smatch matches;

            auto & ownedPools = res.at(poolowner);
            auto heroname = hero->getNameTextID();

            // Check if the entire string matches the pattern
            if (std::regex_match(heroname, matches, pattern))
                poolname = matches[1].str();

            auto it = ownedPools.find(poolname);
            if (it == ownedPools.end())
            {

                auto it_id = poolids.find(poolname);
                if (it_id == poolids.end())
                    it_id = poolids.try_emplace(poolname, poolid++).first;
                auto poolid = it_id->second;

                it = ownedPools.emplace(poolname, HeroPool(poolid, poolname)).first;

                ML_VERBOSE("Added pool " << poolid << " of owner " << poolowner << ": " << poolname << "\n");
            }

            auto& pool = it->second;
            pool.heroes.push_back(hero);
            ++counter;
        }

        for (const auto & [name1, pool1] : res.at(0))
        {
            if (res.at(1).find(name1) == res.at(1).end())
                throw std::runtime_error("Owners have different pools");
            auto x = (*res.at(1).find(name1)).second;
            if (pool1.heroes.size() != (*res.at(1).find(name1)).second.heroes.size())
                // throw std::runtime_error("Owners have differently sized pools");
                std::cout << "WARNING: Owners have differently sized pools: " << pool1.heroes.size() << " <> " << (*res.at(1).find(name1)).second.heroes.size() << "\n";
        }

        ML_VERBOSE("Grouped " << counter << " heroes into " << res.size() << "x" << res.at(0).size() << " pools\n");

        return res;
    }

    std::vector<CGTownInstance*> InitTowns(CGameState* gs) {
        auto res = std::vector<CGTownInstance*> {};
        for (const auto &townid : gs->getMap().getAllTowns()) {
            auto *town = gs->getTown(townid);
            res.push_back(town);
        }
        return res;
    }

    std::map<const BattleFieldInfo*, std::vector<const TerrainType*>> InitBattleterrains(std::string battlefieldPattern) {
        auto res = std::map<const BattleFieldInfo*, std::vector<const TerrainType*>> {};
        auto lands = std::set<const TerrainType*> {};
        auto pattern = std::regex();

        if (battlefieldPattern.empty()) {
            pattern = std::regex(".");
        } else {
            pattern = std::regex(battlefieldPattern);
        }

        LIBRARY->terrainTypeHandler->forEach([&res, &lands, &pattern] (const TerrainType * terrain, bool &_) {
            if (terrain->isLand() && terrain->isPassable()) {
                lands.insert(terrain);
            }

            for (const auto & bf : terrain->battleFields) {
                if (std::regex_search(bf.getInfo()->getJsonKey(), pattern)) {
                    res[bf.getInfo()].push_back(terrain);
                } else {
                    ML_VERBOSE("Filtering out " << bf.getInfo()->getJsonKey() << "\n");
                }
            }
        });

        LIBRARY->battlefieldsHandler->forEach([&res, &lands, &pattern](const BattleFieldInfo * bi, bool &_) {
            if (!res.contains(bi)) {
                if (std::regex_search(bi->getJsonKey(), pattern)) {
                    if (bi->getJsonKey() == "core:ship_to_ship") {
                        // XXX: ship-to-ship battles are buggy
                        //      See https://github.com/vcmi/vcmi/issues/4781
                        // res[bi] = {VLC->terrainTypeHandler->getByIndex(TerrainId::WATER)}
                    } else {
                        res[bi].insert(res[bi].end(), lands.begin(), lands.end());
                    }
                } else {
                    ML_VERBOSE("Filtering out " << bi->getJsonKey() << "\n");
                }
            }
        });

        if (!battlefieldPattern.empty()) {
            ML_VERBOSE("Filtered battlefields matching pattern: '" << battlefieldPattern << "'\n");

            if (res.size() == 0) {
                std::cout << "ALL BATTLEFIELDS WERE FILTERED OUT\n";
            }

            if (IsMLVerbose()) {
                for (auto &[bi, terrains] : res) {
                    std::cout << bi->getJsonKey() << " ->";
                    for (auto &t : terrains) {
                        std::cout << " " << t->getJsonKey();
                    }
                    std::cout << "\n";
                 }
            }
         }

        return res;
    }


    std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> InitWarMachines(CGameState * gs) {
        auto res = std::map<const CGHeroInstance*, std::array<CArtifactInstance*, 3>> {};

        for (const auto &hid : gs->getMap().getHeroesOnMap()) {
            res.insert({gs->getHero(hid), {
                gs->createArtifact(ArtifactID::BALLISTA),
                gs->createArtifact(ArtifactID::AMMO_CART),
                gs->createArtifact(ArtifactID::FIRST_AID_TENT)
            }});
        }
        return res;
    }

    std::vector<CreatureID> InitCreatures() {
        auto res = std::vector<CreatureID>{};

        LIBRARY->creatures()->forEach([&res](const Creature * cr, bool &stop) {
            // Invalid creatures (arrow towers, non-damage war machines, NOT_USED, etc. have growth=0)
            // We dont those as they cause a game crash when there is no regular creature in the army (instant battle end)
            if (cr->getGrowth() > 0) {
                res.push_back(cr->getId());
                ML_VERBOSE("ADD");
            } else {
                ML_VERBOSE("SKIP");
            }

            ML_VERBOSE(" creature: level: " << cr->getLevel() << " " << cr->getNameSingularTextID() << " growth: " << cr->getGrowth() << "\n");
        });

        return res;
    }

    std::vector<CreatureID> InitShootingCreatures() {
        auto res = std::vector<CreatureID>{};

        LIBRARY->creatures()->forEach([&res](const Creature * cr, bool &stop) {
            // Invalid creatures (arrow towers, war machines, NOT_USED, etc. have lvl=0)
            // "special" creatures (property not exposed, but e.g. ballistas have lvl=4 and growth=0)
            if (cr->getLevel() > 0 && cr->getGrowth() > 0 && cr->getBaseShots() > 0)
            {
                ML_VERBOSE("ADDING SHOOTER: " << cr->getId() << " | " << cr->getGrowth() << " | " << cr->getNameSingularTranslated() << "\n");
                res.push_back(cr->getId());
            }
        });

        return res;
    }

    // Just non-shooting low-tier creatures (levels 1..3)
    std::vector<CreatureID> InitGuardingCreatures() {
        auto res = std::vector<CreatureID>{};

        LIBRARY->creatures()->forEach([&res](const Creature * cr, bool &stop) {
            // Invalid creatures (arrow towers, war machines, NOT_USED, etc. have lvl=0)
            // "special" creatures (property not exposed, but e.g. ballistas have lvl=4 and growth=0)
            if (cr->getLevel() > 0 && cr->getGrowth() && cr->getLevel() < 4 && cr->getBaseShots() == 0) {
                ML_VERBOSE("ADDING GUARD: " << cr->getId() << " | " << cr->getGrowth() << " | " << cr->getNameSingularTranslated() << "\n");
                res.push_back(cr->getId());
            }
        });

        return res;
    }

    std::unique_ptr<Stats> InitStats(CGameState * gs, const Config& config, int npools, int poolsize) {
        if (config.statsMode == "disabled")
            return nullptr;

        if (config.swapSides > 0) {
            throw std::runtime_error("Cannot track stats for " + config.statsMode + " when swapping sides");
        }

        return std::make_unique<Stats>(
            config.statsStorage,
            config.statsMode == "blue",
            config.statsTimeout,
            config.statsPersistFreq,
            config.maxBattles,
            npools,
            poolsize
        );
    }
}

ServerPlugin::ServerPlugin(CGameHandler * gh, CGameState * gs, Config & config_)
: gh(gh)
, config(config_)
, alltowns(InitTowns(gs))
, heropools(InitHeroPools(gs))
, battleterrains(InitBattleterrains(config.battlefieldPattern))
, allmachines(InitWarMachines(gs))
, allcreatures(InitCreatures())
, allshooters(InitShootingCreatures())
, allguards(InitGuardingCreatures())
, stats(InitStats(gs, config, heropools.at(0).size(), 2*heropools.at(0).begin()->second.heroes.size()))
, rng(std::mt19937(config.rngSeed ? config.rngSeed : gh->getRandomGenerator().nextInt(0, std::numeric_limits<int>::max())))
, creatureValues(InitCreatureValues())
{
    // XXX: Take out the first two heroes from the first heropool
    auto & p1 = heropools.at(0).begin()->second.heroes;
    auto & p2 = heropools.at(1).begin()->second.heroes;

    auto vipEnabled = [this]() {
        return config.leftVip || config.rightVip;
    };

    if ((vipEnabled() || config.leftHar || config.rightHar) && !config.randomArmies) {
        std::cout << "WARNING: VIP or HAR army enabled, but random armies are not enabled -- will enable random armies\n";
        config.randomArmies = true;
    }

    if ((config.leftVip && config.leftHar) || (config.rightVip && config.rightHar))
        throw std::runtime_error("VIP and HAR armies cannot be both enabled for the same side.");

    if (p1.size() < 2 || p2.size() < 2) {
        if (vipEnabled())
            std::cout << "WARNING: leftVip or rightVip, but there are less than 2 total heroes owned by this player on this map. Will not enable VIP shooters.\n";
        config.leftVip = false;
        config.rightVip = false;
        nonvipHero1 = p1[0];
        nonvipHero2 = p2[0];
    } else {
        vipHero1 = p1[0];
        vipHero2 = p2[0];

        nonvipHero1 = p1[1];
        nonvipHero2 = p2[1];

        // Remove vip heroes from pools so they can't be chosen via randomHeroes
        p1.erase(p1.begin(), p1.begin() + 2);
        p2.erase(p2.begin(), p2.begin() + 2);

        // Mark heres with "VIP shooter" armies via grail in backpack
        auto grailId = ArtifactID::GRAIL;
        for (const auto & h : {vipHero1, vipHero2}) {
            auto artloc = ArtifactLocation(h->id, ArtifactPosition::BACKPACK_START);
            // XXX: createArtifact (via GS, not GH) must be done BEFORE map is sent to clients?
            // (does not work if done in setupBattle hook, for example: client does not see new artifact)
            const auto * art = gs->createArtifact(grailId);
            ML_VERBOSE("+++++ ADD grail (ArtifactInstanceID=" << art->getId() << ", ArtifactID=" << art->getTypeId() << ") to hero (ObjectInstanceID=" << h->id << ")\n");
            h->putArtifact(artloc.slot, art);
            // gh->putArtifact(artloc, art->getId(), false);
        }
    }

    if (config.randomHeroes > 0) {
        for (int owner : {0, 1})
        {
            for (auto &[poolname, pool] : heropools.at(owner)) {
                ML_VERBOSE("poolname: " << poolname << ", heroes: " << pool.heroes.size() << "\n");
                if (pool.heroes.size() == 0) {
                    throw std::runtime_error("randomHeroes requires at leats 1 hero in each pool.");
                }
            }
        }
    }

    /*
     * Give artifacts + leadership to all heroes
     * (ensures morale and luck will be 0)
     */
    auto artifacts = std::map<ArtifactPosition, ArtifactID> {
        {ArtifactPosition::NECK, ArtifactID(108)},  // pendantOfCourage
        {ArtifactPosition::MISC1, ArtifactID(50)},  // crestOfValor
        {ArtifactPosition::MISC2, ArtifactID(51)},  // glyphOfGallantry
        {ArtifactPosition::MISC3, ArtifactID(84)},  // spiritOfOppression
        {ArtifactPosition::MISC4, ArtifactID(85)},  // hourglassOfTheEvilHour
    };

    for (const auto &hid : gs->getMap().getHeroesOnMap()) {
        auto *h = gs->getHero(hid);

        for (auto &[pos, artid] : artifacts) {
            if (h->artifactsWorn.find(pos) != h->artifactsWorn.end())
                h->removeArtifact(pos);
            h->putArtifact(pos, gs->createArtifact(artid));
        }

        h->setSecSkillLevel(SecondarySkill::LEADERSHIP, 3, ChangeValueMode::ABSOLUTE);
    }
}

void ServerPlugin::setupBattleHook(const CGTownInstance *& town, TerrainId & terrain, BattleField & terType, ui32 & seed) {
    if (config.randomTerrainChance > 0 && battleterrains.size() > 0) {
        auto dist = std::uniform_int_distribution<>(0, 99);
        auto roll = dist(rng);
        if (roll < config.randomTerrainChance) {
            // XXX: client will render the "original" terrain texture,
            //      but that's only visual, as the newly set terrain
            //      is the one actually in effect.
            auto dist1 = std::uniform_int_distribution<>(0, battleterrains.size() - 1);
            auto it1 = battleterrains.begin();
            std::advance(it1, dist1(rng));
            auto &[bi, terrains] = *it1;

            auto dist2 = std::uniform_int_distribution<>(0, terrains.size() - 1);
            auto it2 = terrains.begin();
            std::advance(it2, dist2(rng));

            // modification by reference
            terType = bi->battlefield;
            terrain = (*it2)->getId();
        }
    }

    if (config.randomObstacles > 0 && (battlecounter % config.randomObstacles == 0)) {
        // modification by reference
        seed = gh->getRandomGenerator().nextInt(0, std::numeric_limits<int>::max());
    }

    if (config.townChance > 0) {
        auto dist = std::uniform_int_distribution<>(0, 99);
        auto roll = dist(rng);

        if (roll < config.townChance) {
            if (towncounter % alltowns.size() == 0) {
                towncounter = 0;
                std::shuffle(alltowns.begin(), alltowns.end(), rng);
            }

            town = alltowns.at(towncounter); // modification by reference
            ++towncounter;
        } else {
            town = nullptr; // modification by reference
        }
    }
}

void ServerPlugin::handleRandomHeroes(
    const CArmedInstance *&army1,
    const CArmedInstance *&army2,
    const CGHeroInstance *&hero1,
    const CGHeroInstance *&hero2
) {
    if (config.randomHeroes == 0)
        return;

    if (poolcounter % heropools.at(0).size() == 0)
        poolcounter = 0; // no shuffling for std::map

    auto it1 = heropools.at(0).begin();
    auto it2 = heropools.at(1).begin();
    std::advance(it1, poolcounter);
    std::advance(it2, poolcounter);
    auto &pool1 = it1->second;
    auto &pool2 = it2->second;

    if (pool1.counter % pool1.heroes.size() == 0) {
        pool1.counter = 0;
        std::shuffle(pool1.heroes.begin(), pool1.heroes.end(), rng);
    }

    if (pool2.counter % pool2.heroes.size() == 0) {
        pool2.counter = 0;
        std::shuffle(pool2.heroes.begin(), pool2.heroes.end(), rng);
    }

    // printf("poolcounter = %d\n", poolcounter);

    // modification by reference
    hero1 = pool1.heroes.at(pool1.counter);
    hero2 = pool2.heroes.at(pool2.counter);

    // printf("Pool: %s, hero0: %s, hero1: %s\n", pool.name.c_str(), hero1->getNameTextID().c_str(), hero2->getNameTextID().c_str());

    if (battlecounter % config.randomHeroes == 0) {
        poolcounter += 1;
        pool1.counter += 1;
        pool2.counter += 1;
    }

    // modification by reference
    army1 = hero1->getArmy();
    army2 = hero2->getArmy();

    // Store as nonvip heroes
    nonvipHero1 = hero1;
    nonvipHero2 = hero2;

}

void ServerPlugin::handleRandomArmies(
    const CArmedInstance *&army1,
    const CArmedInstance *&army2,
    const CGHeroInstance *&hero1,
    const CGHeroInstance *&hero2
) {
    if (!config.randomArmies)
        return;

    if ((config.randomArmyValueMin < 500) || (config.randomArmyValueMax < config.randomArmyValueMin) || (config.randomArmyTargetVar < 0) || (config.randomArmyTargetVar > 100))
        throw std::runtime_error("invalid randomArmy config given");

    if(allcreatures.empty())
        throw std::runtime_error("cannot generate random armies without valid creatures");

    if((config.leftVip || config.rightVip) && (allshooters.empty() || allguards.empty()))
        throw std::runtime_error("cannot generate VIP random armies without shooter and guard creatures");

    std::vector<CreatureID> meleeCreatures;
    std::vector<CreatureID> fastMeleeCreatures;
    for(const auto & creatureId : allcreatures)
    {
        const auto * creature = creatureId.toCreature();
        if(creature->hasBonusOfType(BonusType::SHOOTER))
            continue;

        meleeCreatures.push_back(creatureId);
        if(creature->getBaseSpeed() > 7)
            fastMeleeCreatures.push_back(creatureId);
    }

    if((config.leftHar || config.rightHar) && fastMeleeCreatures.empty())
        throw std::runtime_error("cannot generate HAR random armies without fast melee creatures");

    struct GeneratedStack
    {
        SlotID slot;
        const CCreature * creature;
        int quantity;
    };

    auto divCeil = [](int value, int divisor) -> int
    {
        return (value + divisor - 1) / divisor;
    };

    auto totalArmyValue = [this](const std::vector<GeneratedStack> & generated) -> int
    {
        return boost::accumulate(generated, 0, [&](int sum, const auto & stack) {
            return sum + (creatureValues.at(stack.creature->getId()) * stack.quantity);
        });
    };

    auto var = static_cast<double>(config.randomArmyTargetVar) / 100;

    auto generateArmyWithStackCount = [this, &divCeil, &var](
        int targetValue,
        const std::vector<CreatureID> & creaturePool,
        int desiredStackCount
    ) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));
        const auto cheapestCreature = std::ranges::min_element(creaturePool, [this](const CreatureID & left, const CreatureID & right)
        {
            return creatureValues.at(left) < creatureValues.at(right);
        });
        const int cheapestValue = creatureValues.at(*cheapestCreature);

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            std::array<int, 7> slots = { 0, 1, 2, 3, 4, 5, 6 };
            std::shuffle(slots.begin(), slots.end(), rng);

            int totalValue = 0;
            std::vector<GeneratedStack> generated;

            for(int stackIndex = 0; stackIndex < desiredStackCount; ++stackIndex)
            {
                const int remainingSlotsAfterThis = desiredStackCount - stackIndex - 1;
                const int remainingMaxValue = maxTotalValue - totalValue;
                const int reservedValueForLaterSlots = cheapestValue * remainingSlotsAfterThis;
                if(remainingMaxValue <= reservedValueForLaterSlots)
                    break;

                const bool isLastStack = remainingSlotsAfterThis == 0;
                std::vector<const CCreature *> candidates;
                for(const auto & creatureId : creaturePool)
                {
                    const auto value = creatureValues.at(creatureId);
                    if(value <= 0 || value + reservedValueForLaterSlots > remainingMaxValue)
                        continue;

                    if(isLastStack)
                    {
                        const int requiredQuantity = divCeil(minTotalValue - totalValue, value);
                        if(requiredQuantity * value > remainingMaxValue)
                            continue;
                    }

                    candidates.push_back(creatureId.toCreature());
                }

                if(candidates.empty())
                    break;

                const auto * creature = candidates.at(std::uniform_int_distribution<>(0, static_cast<int>(candidates.size()) - 1)(rng));
                const int creatureValue = creatureValues.at(creature->getId());
                int maxQuantity = (remainingMaxValue - reservedValueForLaterSlots) / creatureValue;
                const int minQuantity = isLastStack
                    ? std::max(1, divCeil(minTotalValue - totalValue, creatureValue))
                    : 1;

                if(!isLastStack)
                {
                    const int valueBudgetForThisStack = divCeil(targetValue - totalValue, desiredStackCount - stackIndex);
                    const int quantityBudgetForThisStack = std::max(1, divCeil(valueBudgetForThisStack * 2, creatureValue));
                    maxQuantity = std::min(maxQuantity, quantityBudgetForThisStack);
                }

                if(minQuantity > maxQuantity)
                    break;

                const int quantity = std::uniform_int_distribution<>(minQuantity, maxQuantity)(rng);
                generated.push_back({ SlotID(slots.at(stackIndex)), creature, quantity });
                totalValue += creatureValue * quantity;
            }

            if(generated.size() == desiredStackCount && totalValue >= minTotalValue && totalValue <= maxTotalValue)
                return generated;
        }

        return {};
    };

    auto generateArmy = [this, &divCeil, &generateArmyWithStackCount](int targetValue) -> std::vector<GeneratedStack>
    {
        for(int attempt = 0; attempt < 100; ++attempt)
        {
            // Bias towards fuller armies while still allowing sparse ones sometimes.
            auto distSlotCount = std::uniform_int_distribution<>(1, 7);
            const int desiredStackCount = std::max(distSlotCount(rng), distSlotCount(rng));
            auto generated = generateArmyWithStackCount(targetValue, allcreatures, desiredStackCount);
            if(!generated.empty())
                return generated;
        }

        const auto cheapestCreature = std::ranges::min_element(allcreatures, [this](const CreatureID & left, const CreatureID & right)
        {
            return creatureValues.at(left) < creatureValues.at(right);
        });
        const auto * creature = cheapestCreature->toCreature();
        const int creatureValue = creatureValues.at(creature->getId());
        const int quantity = std::max(1, divCeil(targetValue, creatureValue));
        return { { SlotID(0), creature, quantity } };
    };

    auto generateVipArmy = [this, &divCeil, &var, &totalArmyValue, &generateArmyWithStackCount](int targetValue) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));

        const auto cheapestGuard = std::ranges::min_element(allguards, [this](const CreatureID & left, const CreatureID & right)
        {
            return creatureValues.at(left) < creatureValues.at(right);
        });
        const int cheapestGuardValue = creatureValues.at(*cheapestGuard);

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            const int desiredStackCount = std::uniform_int_distribution<>(4, 7)(rng);
            const int guardStackCount = desiredStackCount - 1;
            const int minGuardValue = cheapestGuardValue * guardStackCount;
            const int maxGuardValue = (maxTotalValue * 7) / 10;

            if(minGuardValue > maxGuardValue)
                continue;

            const int guardTarget = std::uniform_int_distribution<>(minGuardValue, maxGuardValue)(rng);
            auto generated = generateArmyWithStackCount(guardTarget, allguards, guardStackCount);
            if(generated.empty())
                continue;

            const int guardValue = totalArmyValue(generated);
            const int minShooterValue = std::max({ 1, minTotalValue - guardValue, divCeil(guardValue * 3, 7) });
            const int maxShooterValue = maxTotalValue - guardValue;
            if(minShooterValue > maxShooterValue)
                continue;

            struct ShooterCandidate
            {
                const CCreature * creature;
                int minQuantity;
                int maxQuantity;
            };

            std::vector<ShooterCandidate> shooters;
            for(const auto & shooterId : allshooters)
            {
                const int shooterValue = creatureValues.at(shooterId);
                const int minQuantity = std::max(1, divCeil(minShooterValue, shooterValue));
                const int maxQuantity = maxShooterValue / shooterValue;
                if(minQuantity <= maxQuantity)
                    shooters.push_back({ shooterId.toCreature(), minQuantity, maxQuantity });
            }

            if(shooters.empty())
                continue;

            const auto & shooter = shooters.at(std::uniform_int_distribution<>(0, static_cast<int>(shooters.size()) - 1)(rng));
            const int shooterQuantity = std::uniform_int_distribution<>(shooter.minQuantity, shooter.maxQuantity)(rng);
            const int shooterValue = creatureValues.at(shooter.creature->getId()) * shooterQuantity;

            std::array<bool, 7> occupied = { false, false, false, false, false, false, false };
            for(const auto & stack : generated)
                occupied.at(static_cast<int>(stack.slot)) = true;

            std::vector<int> freeSlots;
            for(int slot = 0; slot < 7; ++slot)
                if(!occupied.at(slot))
                    freeSlots.push_back(slot);

            const int shooterSlot = freeSlots.at(std::uniform_int_distribution<>(0, static_cast<int>(freeSlots.size()) - 1)(rng));
            generated.push_back({ SlotID(shooterSlot), shooter.creature, shooterQuantity });

            const int totalValue = guardValue + shooterValue;
            if(totalValue >= minTotalValue && totalValue <= maxTotalValue && shooterValue * 10 >= totalValue * 3)
                return generated;
        }

        throw std::runtime_error("failed to generate VIP random army with the current randomArmy config");
    };

    auto generateHarArmy = [this, &divCeil, &var, &meleeCreatures, &fastMeleeCreatures](int targetValue) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            const auto * primary = fastMeleeCreatures.at(std::uniform_int_distribution<>(0, static_cast<int>(fastMeleeCreatures.size()) - 1)(rng)).toCreature();
            const int primaryUnitValue = creatureValues.at(primary->getId());
            const int otherStackCount = std::uniform_int_distribution<>(0, 3)(rng);

            const CCreature * other = nullptr;
            int otherQuantity = 0;
            int otherValue = 0;
            if(otherStackCount > 0)
            {
                std::vector<const CCreature *> otherCandidates;
                for(const auto & creatureId : meleeCreatures)
                {
                    if(creatureId == primary->getId())
                        continue;

                    const int unitValue = creatureValues.at(creatureId);
                    if(unitValue > 0 && unitValue * otherStackCount <= maxTotalValue / 20)
                        otherCandidates.push_back(creatureId.toCreature());
                }

                if(otherCandidates.empty())
                    continue;

                other = otherCandidates.at(std::uniform_int_distribution<>(0, static_cast<int>(otherCandidates.size()) - 1)(rng));
                const int otherUnitValue = creatureValues.at(other->getId());
                const int maxOtherQuantity = (maxTotalValue / 20) / otherUnitValue;
                otherQuantity = std::uniform_int_distribution<>(otherStackCount, maxOtherQuantity)(rng);
                otherValue = otherUnitValue * otherQuantity;
            }

            const int minPrimaryValue = std::max(minTotalValue - otherValue, otherValue * 19);
            const int minPrimaryQuantity = std::max(1, divCeil(minPrimaryValue, primaryUnitValue));
            const int maxPrimaryQuantity = (maxTotalValue - otherValue) / primaryUnitValue;
            if(minPrimaryQuantity > maxPrimaryQuantity)
                continue;

            const int primaryQuantity = std::uniform_int_distribution<>(minPrimaryQuantity, maxPrimaryQuantity)(rng);
            const int primaryValue = primaryUnitValue * primaryQuantity;
            const int totalValue = primaryValue + otherValue;
            if(primaryValue * 20 < totalValue * 19)
                continue;

            std::array<int, 7> slots = { 0, 1, 2, 3, 4, 5, 6 };
            std::shuffle(slots.begin(), slots.end(), rng);
            std::vector<GeneratedStack> generated = { { SlotID(slots.at(0)), primary, primaryQuantity } };

            int remainingOtherQuantity = otherQuantity;
            for(int stackIndex = 0; stackIndex < otherStackCount; ++stackIndex)
            {
                const int remainingStacks = otherStackCount - stackIndex;
                const int quantity = remainingStacks == 1
                    ? remainingOtherQuantity
                    : std::uniform_int_distribution<>(1, remainingOtherQuantity - remainingStacks + 1)(rng);
                generated.push_back({ SlotID(slots.at(stackIndex + 1)), other, quantity });
                remainingOtherQuantity -= quantity;
            }

            return generated;
        }

        throw std::runtime_error("failed to generate HAR random army with the current randomArmy config");
    };

    const int target = std::uniform_int_distribution<>(config.randomArmyValueMin, config.randomArmyValueMax)(rng);
    const bool leftVip = vipHero1 && config.leftVip;
    const bool rightVip = vipHero2 && config.rightVip;
    const bool leftHar = config.leftHar;
    const bool rightHar = config.rightHar;

    if(leftVip)
    {
        // std::cout << "RED VIP: TRIGGER\n";
        // XXX: heroes must be different (objects must have different tempOwner)
        // modification by reference
        hero1 = vipHero1;
        army1 = hero1->getArmy();
    }
    else
    {
        // std::cout << "RED VIP: SKIP\n";
        hero1 = nonvipHero1;
        army1 = nonvipHero1->getArmy();
    }

    if(rightVip)
    {
        // std::cout << "BLUE VIP: TRIGGER " << config.rightVipChance << "\n";
        hero2 = vipHero2;
        army2 = hero2->getArmy();
    }
    else
    {
        // std::cout << "BLUE VIP: SKIP " << config.rightVipChance << "\n";
        hero2 = nonvipHero2;
        army2 = nonvipHero2->getArmy();
    }

    auto replaceArmy = [this, &target, &generateArmy, &generateVipArmy, &generateHarArmy](const CGHeroInstance * hero, bool vip, bool har)
    {
        std::vector<GeneratedStack> generated;
        if(vip)
            generated = generateVipArmy(target);
        else if(har)
            generated = generateHarArmy(target);
        else
            generated = generateArmy(target);

        for(int slot = 0; slot < 7; ++slot)
            if(hero->hasStackAtSlot(SlotID(slot)))
                gh->eraseStack(StackLocation(hero->id, SlotID(slot)), true);

        for(const auto & stack : generated)
            gh->insertNewStack(StackLocation(hero->id, stack.slot), stack.creature, stack.quantity);
    };

    // std::cout << "=================================\n";

    replaceArmy(hero1, leftVip, leftHar);
    replaceArmy(hero2, rightVip, rightHar);
}


void ServerPlugin::handleMirrorArmies(
    const CArmedInstance *&army1,
    const CArmedInstance *&army2,
    const CGHeroInstance *&hero1,
    const CGHeroInstance *&hero2
) {
    if (!config.mirrorArmies)
        return;

    struct MirroredStack
    {
        SlotID slot;
        const CCreature * creature;
        TQuantity quantity;
    };

    std::vector<MirroredStack> mirroredStacks;
    for(const auto & [slot, stack] : army1->Slots())
        mirroredStacks.push_back({ slot, stack->getCreature(), stack->getCount() });

    for(int slot = 0; slot < GameConstants::ARMY_SIZE; ++slot)
        if(army2->hasStackAtSlot(SlotID(slot)))
            gh->eraseStack(StackLocation(army2->id, SlotID(slot)), true);

    for(const auto & stack : mirroredStacks)
        gh->insertNewStack(StackLocation(army2->id, stack.slot), stack.creature, stack.quantity);
}


void ServerPlugin::handleWarmachines(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    if (config.warmachineChance == 0)
        return;

    // XXX: adding war machines by index of pre-created per-hero artifact instances
    // 0=ballista, 1=cart, 2=tent
    auto machineslots = std::map<ArtifactID, ArtifactPosition> {
        {ArtifactID::BALLISTA, ArtifactPosition::MACH1},
        {ArtifactID::AMMO_CART, ArtifactPosition::MACH2},
        {ArtifactID::FIRST_AID_TENT, ArtifactPosition::MACH3},
    };

    auto dist = std::uniform_int_distribution<>(0, 99);
    for (const auto *h : {hero1, hero2}) {
        for (auto *m : allmachines.at(h)) {
            auto it = machineslots.find(m->getTypeId());
            if (it == machineslots.end())
                throw std::runtime_error("Could not find warmachine");

            auto apos = it->second;
            auto roll = dist(rng);
            if (roll < config.warmachineChance) {
                if (!h->getArt(apos))
                    const_cast<CGHeroInstance*>(h)->putArtifact(apos, m);
            } else {
                if (h->getArt(apos))
                    const_cast<CGHeroInstance*>(h)->removeArtifact(apos);
            }
        }
    }
}

void ServerPlugin::handleTightFormation(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    auto dist = std::uniform_int_distribution<>(0, 99);
    for (const auto *h : {hero1, hero2}) {
        auto roll = dist(rng);
        const_cast<CGHeroInstance*>(h)->formation = (roll < config.tightFormationChance)
            ? EArmyFormation::TIGHT
            : EArmyFormation::LOOSE;
    }
}

void ServerPlugin::handleMinMaxMana(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    // Randomize mana
    auto dist = std::uniform_int_distribution<>(config.manaMin, config.manaMax);
    for(const auto *h : {hero1, hero2}) {
        if(!h) continue;
        gh->setManaPoints(h->id, dist(rng));
    }
}

void ServerPlugin::handleSwapSides(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    bool swappingSides = (config.swapSides > 0 && (battlecounter % config.swapSides) == 0);
    if (swappingSides)
        redside = !redside;

    // Set temp owner of both heroes to player0 and player1
    // XXX: causes UB after battle, unless it is replayed (ok for training)
    // XXX: if redside=1 (right), hero2 should have owner=0 (red)
    //      if redside=0 (left), hero1 should have owner=0 (red)
    const_cast<CGHeroInstance*>(hero1)->tempOwner = PlayerColor(redside);
    const_cast<CGHeroInstance*>(hero2)->tempOwner = PlayerColor(!redside);
}

void ServerPlugin::handleRandomPrimarySkills(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    if(config.randomPrimarySkills == 0)
        return;

    auto dist = std::uniform_int_distribution<>(0, config.randomPrimarySkills);
    gh->changePrimSkill(hero1, PrimarySkill::ATTACK, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changePrimSkill(hero1, PrimarySkill::DEFENSE, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changePrimSkill(hero2, PrimarySkill::ATTACK, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changePrimSkill(hero2, PrimarySkill::DEFENSE, dist(rng), ChangeValueMode::ABSOLUTE);
}

void ServerPlugin::handleRandomSecondarySkills(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    auto dist = std::uniform_int_distribution<>(MasteryLevel::NONE, MasteryLevel::EXPERT);
    gh->changeSecSkill(hero1, SecondarySkill::BALLISTICS, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changeSecSkill(hero2, SecondarySkill::BALLISTICS, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changeSecSkill(hero1, SecondarySkill::ARTILLERY, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changeSecSkill(hero2, SecondarySkill::ARTILLERY, dist(rng), ChangeValueMode::ABSOLUTE);
}

void ServerPlugin::startBattleHook(
    const CArmedInstance *&army1,
    const CArmedInstance *&army2,
    const CGHeroInstance *&hero1,
    const CGHeroInstance *&hero2
) {
    if (!(hero1 && hero2))
        throw std::runtime_error("Both hero1 and hero2 are required");

    battlecounter++;

    // printf("config.randomHeroes = %d\n", config.randomHeroes);

    handleRandomHeroes(army1, army2, hero1, hero2);
    handleRandomArmies(army1, army2, hero1, hero2);
    handleMirrorArmies(army1, army2, hero1, hero2);
    handleWarmachines(hero1, hero2);
    handleTightFormation(hero1, hero2);
    handleMinMaxMana(hero1, hero2);
    // handleSwapSides(hero1, hero2);  // DO NOT USE (causes nasty bugs)
    handleRandomPrimarySkills(hero1, hero2);
    handleRandomSecondarySkills(hero1, hero2);
}

void ServerPlugin::endBattleHook(
    BattleResult * br,
    const CGHeroInstance * heroAttacker,
    const CGHeroInstance * heroDefender
) {
    // don't record stats for retreats (i.e. env resets)
    // XXX: stats not updated with owner-based hero pools
    if (stats && br->result == EBattleResult::NORMAL) {
        auto extractHeroID = [](const std::string& name) {
            std::regex pattern(R"(^hero_(\d+)_pool_([0-9A-Za-z]+)$)");

            int hero_id = -1;
            std::string pool_name = "default";
            std::smatch match;

            if (std::regex_match(name, match, pattern)) {
                pool_name = match[2].str();
            } else {
                // assert old hero name format (no pools)
                std::regex pattern2(R"(^hero_(\d+)$)");
                if (!std::regex_match(name, match, pattern2))
                    throw std::runtime_error("invalid hero name: " + name);
            }

            hero_id = std::stoi(match[1].str());

            return std::pair<int, std::string>(hero_id, pool_name);
        };

        auto [attackerID, poolname] = extractHeroID(heroAttacker->getNameTextID());
        auto [defenderID, poolname2] = extractHeroID(heroDefender->getNameTextID());

        if (poolname != poolname2)
            throw std::runtime_error("Pools do not match: " + poolname + " <> " + poolname2);

        if(heroAttacker->tempOwner == heroDefender->tempOwner)
            throw std::runtime_error("temp owners must differ");

        auto itAttacker = heropools.at(heroAttacker->tempOwner).find(poolname);
        if (itAttacker == heropools.at(heroAttacker->tempOwner).end())
            throw std::runtime_error("Could not find attacker pool with name: " + poolname);

        auto itDefender = heropools.at(heroDefender->tempOwner).find(poolname);
        if (itDefender == heropools.at(heroDefender->tempOwner).end())
            throw std::runtime_error("Could not find defender pool with name: " + poolname);

        if (itAttacker->second.id != itDefender->second.id)
            throw std::runtime_error("Attacker and defender pools are different: " + std::to_string(itAttacker->second.id) + "/" + itAttacker->second.name + " <> " + std::to_string(itDefender->second.id) + "/" + itDefender->second.name);

        auto poolid = itAttacker->second.id;

        // XXX: stats not updated with owner-based hero pools
        auto statside = (config.statsMode == "red") ? redside : !redside;
        auto victory = br->winner == BattleSide(statside);
        stats->dataadd(victory, poolid, attackerID, defenderID);
    }

    if (config.maxBattles && battlecounter >= config.maxBattles) {
        std::cout << "Hit battle limit of " << config.maxBattles << ", will quit now...\n";
        if (stats && config.statsPersistFreq) stats->dbupdate();

        #ifdef VCMI_APPLE
            ::exit(EXIT_SUCCESS);
        #else
            std::quick_exit(EXIT_SUCCESS);
        #endif

        ::exit(EXIT_SUCCESS);
        return;
    }
}
}

VCMI_LIB_NAMESPACE_END
