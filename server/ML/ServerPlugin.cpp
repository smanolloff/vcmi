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
#include "battle/BattleLayout.h"
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
#include "bonuses/BonusParameters.h"
#include "spells/CSpellHandler.h"

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

    // XXX: copied from AI/MMAI/BAI/v15/graph/nodes/unit.cpp
    //      (may be outdated)
    int CalculateValue(const CCreature * cr)
    {
        auto att = cr->getBaseAttack();
        auto def = cr->getBaseDefense();
        auto dmg = (cr->getBaseDamageMax() + cr->getBaseDamageMin()) / 2.0;
        auto hp = cr->getBaseHitPoints();
        auto spd = cr->getBaseSpeed();
        auto shooter = cr->hasBonusOfType(BonusType::SHOOTER);
        auto bonuses = cr->getAllBonuses(Selector::all);

        auto multihexAttackHexcount = [](const std::vector<int> & encodedPath)
        {
            // The bonus parameters vector contains encoded info about affected hexes.
            // The encoding is not known, but also not relevant:
            // we care only about the number of affected hexes and
            // whether they are adjacent or remote
            // (because remote hexes allow to hit "guarded" shooters)
            // The assumption about this hex encoding is:
            // "L"=1, "F"=2, "R"=3, "FL"=21, "FFF"=222, etc.
            // (exact values don't matter, but the number of digits does)
            int numAdjacentHexes = 0;
            int numDistantHexes = 0;
            for(int x : encodedPath)
                x < 10 ? ++numAdjacentHexes : ++numDistantHexes;

            return std::pair{numAdjacentHexes, numDistantHexes};
        };

        /*
         * Term "c":
         * increase is linear up to SPEED_KNEE, then diminishes
         * Visualize on https://www.desmos.com/calculator:
         *
         *    [1]   a=\ln\left(x\cdot2\right)
         *    [2]   b=0.5+\left(s\cdot x\right)\left\{x\le k\right\}
         *    [3]   c=0.5+\left(s\cdot X\right)\left\{\ x\ge k+1\right\}
         *    [4]   \frac{b}{a}\left\{x>1\right\}
         *    [5]   X=k+\left(t\cdot\ln\left(1+\frac{\left(x-k\right)}{t}\right)\right)
         *    [6]   s=0.2
         *    [7]   k=15
         *    [8]   t=5
         *
         *      Use Desmos's visibility toggles to hide/show relevant visualizations:
         *      - The old speed-factor is visualized by eq. [1]
         *      - The new speed-factor is visualized by eq. [2] and [3]
         *      - The new-vs-old relation is visualized by eq. [4]
         *
         */

        constexpr double SPEED_KNEE = 13.0;
        constexpr double SPEED_SLOPE = 0.2;
        constexpr double SPEED_TAIL_WIDTH = 2.0;
        const auto effectiveSpeed = spd <= SPEED_KNEE
            ? spd
            : SPEED_KNEE + (SPEED_TAIL_WIDTH * std::log1p((spd - SPEED_KNEE) / SPEED_TAIL_WIDTH));

        auto a = 3 * dmg * (1 + std::min(4.0, 0.05 * att));
        auto b = hp / (1 - std::min(0.7, 0.025 * def));
        auto c = spd ? 0.5 + (SPEED_SLOPE * effectiveSpeed) : 0.5;
        auto d = shooter ? 1.5 : 1.0;

        // Enchanters have many "enchanter" bonuses, add only once
        bool enchanter = false;

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
                    d += (bonus->val * 0.02);
                    break;
                case BonusType::DOUBLE_DAMAGE_CHANCE:
                    d += (bonus->val * 0.005);
                    break;
                case BonusType::ENCHANTER:
                    if (!enchanter)
                    {
                        d += 0.5;
                        enchanter = true;
                    }
                    break;
                case BonusType::ENEMY_ATTACK_REDUCTION:
                case BonusType::ENEMY_DEFENCE_REDUCTION:
                    d += (bonus->val * 0.0025);
                    break;
                case BonusType::FEROCITY:
                    d += (bonus->val * 0.25);
                    break;
                case BonusType::FIRE_SHIELD:
                    d += (bonus->val * 0.003);
                    break;
                case BonusType::FIRST_STRIKE:
                    d += 0.3;
                    break;
                case BonusType::FLYING:
                    d += 0.1;
                    break;
                case BonusType::LIFE_DRAIN:
                    d += (bonus->val * 0.003);
                    break;
                case BonusType::MULTIHEX_ENEMY_ATTACK:
                {
                    const auto & [adj, dist] = multihexAttackHexcount(bonus->parameters->toVector());
                    d += (adj * 0.03);
                    d += (dist * 0.8);
                }
                break;
                case BonusType::MULTIHEX_UNIT_ATTACK:
                {
                    const auto & [adj, dist] = multihexAttackHexcount(bonus->parameters->toVector());
                    d += (adj * 0.05);
                    d += (dist * 0.1);
                }
                break;
                case BonusType::NO_DISTANCE_PENALTY:
                    d += 0.5;
                    break;
                case BonusType::NO_MELEE_PENALTY:
                    d += 0.1;
                    break;
                case BonusType::RANGED_RETALIATION:
                    d += 0.2;
                    break;
                case BonusType::REVENGE:
                case BonusType::THREE_HEADED_ATTACK:
                    d += 0.15; // deprecated by MULTIHEX_ENEMY_ATTACK
                    break;
                case BonusType::TWO_HEX_ATTACK_BREATH:
                    d += 0.1; // deprecated by MULTIHEX_UNIT_ATTACK
                    break;
                case BonusType::UNLIMITED_RETALIATIONS:
                    d += 0.2;
                    break;
                case BonusType::SPELL_LIKE_ATTACK:
                    if(bonus->subtype.as<SpellID>() == SpellID::DEATH_CLOUD)
                        d += 0.2;
                    break;
                case BonusType::SPELL_AFTER_ATTACK:
                    switch(bonus->subtype.as<SpellID>())
                    {
                        case SpellID::BLIND:
                        case SpellID::STONE_GAZE:
                        case SpellID::PARALYZE:
                            d += (bonus->val * 0.01);
                            break;
                        case SpellID::BIND:
                        case SpellID::WEAKNESS:
                            d += (bonus->val * 0.001);
                            break;
                        case SpellID::AGE:
                            d += (bonus->val * 0.005);
                            break;
                        case SpellID::CURSE:
                            d += (bonus->val * 0.0025);
                            break;
                        case SpellID::DISRUPTING_RAY:
                            d += (bonus->val * 0.002);
                            break;
                        case SpellID::POISON:
                            d += (bonus->val * 0.001);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }

        /*
         * Some examples:
         *
         * Peasant=8            Gremlin=20          Imp=21             Pixie=24
         * Medusa=260           OgreMage=270        Crusader=293       Monk=308
         * BoneDragon=1425      Giant=1432          Hydra=1752         Devil=2344
         * ArchDevil=4484       GoldDragon=4518     Archangel=4763     Titan=5341
         * CrystalDragon=11347  RustDragon=12166    AzureDragon=17988
         *
         */
        auto res = static_cast<int>(std::round((a + b) * c * d));

        if(IsMLVerbose())
        {
            std::cout << "ML_VERBOSE: " << res << " " << cr->getId().toEntity(LIBRARY)->getJsonKey() << " (a=" << a << ", b=" << b << ", c=" << c
                      << ", d=" << d << ")\n";
        }

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
    HeroPools InitHeroPools(CGameState* gs, const Config & config) {
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

        if (config.randomHeroes > 0)
        {
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
        }

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

    BattleTerrains InitBattleterrains(std::string battlefieldPattern)
    {
        std::map<const BattleFieldInfo *, std::vector<const TerrainType *>> grouped;
        std::vector<const TerrainType *> lands;
        auto pattern = battlefieldPattern.empty()
            ? std::regex(".")
            : std::regex(battlefieldPattern);

        LIBRARY->terrainTypeHandler->forEach(
            [&grouped, &lands, &pattern](const TerrainType * terrain, bool &)
            {
                if(terrain->isLand() && terrain->isPassable())
                    lands.push_back(terrain);

                for(const auto & battlefield : terrain->battleFields)
                {
                    const auto * info = battlefield.getInfo();

                    if(std::regex_search(info->getJsonKey(), pattern))
                        grouped[info].push_back(terrain);
                    else
                        ML_VERBOSE("Filtering out " << info->getJsonKey() << "\n");
                }
            });

        std::ranges::sort(lands, {}, [](const TerrainType * terrain)
        {
            return terrain->getJsonKey();
        });

        LIBRARY->battlefieldsHandler->forEach(
            [&grouped, &lands, &pattern](const BattleFieldInfo * info, bool &)
            {
                if(grouped.contains(info))
                    return;

                if(std::regex_search(info->getJsonKey(), pattern))
                {
                    if(info->getJsonKey() != "core:ship_to_ship")
                        grouped[info] = lands;
                }
                else
                {
                    ML_VERBOSE("Filtering out " << info->getJsonKey() << "\n");
                }
            });

        BattleTerrains result;
        result.reserve(grouped.size());

        for(auto & [battlefield, terrains] : grouped)
        {
            std::ranges::sort(terrains, {}, [](const TerrainType * terrain)
            {
                return terrain->getJsonKey();
            });

            result.emplace_back(battlefield, std::move(terrains));
        }

        std::ranges::sort(result, {}, [](const auto & entry)
        {
            return entry.first->getJsonKey();
        });

        return result;
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

    std::vector<CreatureID> InitCreaturePool(const std::vector<CreatureID> & allCreatures, std::string whitelist, const std::string & side)
    {
        const auto trim = [](std::string & value)
        {
            const auto first = value.find_first_not_of(" \t\n\r");
            if(first == std::string::npos)
            {
                value.clear();
                return;
            }

            const auto last = value.find_last_not_of(" \t\n\r");
            value = value.substr(first, last - first + 1);
        };

        trim(whitelist);
        if(whitelist.empty())
            return allCreatures;
        if(whitelist.back() == ',')
            throw std::runtime_error(side + " creature whitelist contains an empty entry");

        std::map<std::string, CreatureID> creaturesByKey;
        for(const auto & creatureId : allCreatures)
            creaturesByKey.emplace(creatureId.toEntity(LIBRARY)->getJsonKey(), creatureId);

        std::vector<CreatureID> result;
        std::stringstream input(whitelist);
        std::string key;
        while(std::getline(input, key, ','))
        {
            trim(key);
            if(key.empty())
                throw std::runtime_error(side + " creature whitelist contains an empty entry");

            const auto creature = creaturesByKey.find(key);
            if(creature == creaturesByKey.end())
                throw std::runtime_error("unknown creature in " + side + " whitelist: " + key);

            if(std::ranges::find(result, creature->second) == result.end())
                result.push_back(creature->second);
        }

        if(result.empty())
            throw std::runtime_error(side + " creature whitelist does not contain any creatures");

        return result;
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

        if (config.randomHeroes == 0)
            throw std::runtime_error("Cannot track stats when randomHeroes is disabled");

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
, heropools(InitHeroPools(gs, config))
, battleterrains(InitBattleterrains(config.battlefieldPattern))
, allmachines(InitWarMachines(gs))
, allcreatures(InitCreatures())
, leftCreatures(InitCreaturePool(allcreatures, config.leftWhitelist, "left"))
, rightCreatures(InitCreaturePool(allcreatures, config.rightWhitelist, "right"))
, allshooters(InitShootingCreatures())
, allguards(InitGuardingCreatures())
, stats(InitStats(gs, config, heropools.at(0).size(), 2*heropools.at(0).begin()->second.heroes.size()))
, rng(std::mt19937(config.rngSeed ? config.rngSeed : gh->getRandomGenerator().nextInt(0, std::numeric_limits<int>::max())))
, creatureValues(InitCreatureValues())
{
    if ((config.leftVip && config.leftHar) || (config.rightVip && config.rightHar))
        throw std::runtime_error("VIP and HAR armies cannot be enabled for the same side.");

    if (config.leftUniformChance > 0 && (config.leftVip || config.leftHar))
        throw std::runtime_error("uniform armies cannot be combined with VIP or HAR for the left side.");

    if (config.rightUniformChance > 0 && (config.rightVip || config.rightHar))
        throw std::runtime_error("uniform armies cannot be combined with VIP or HAR for the right side.");

    if (config.leftHar && config.rightHar)
        throw std::runtime_error("both sides cannot be HAR opponents.");

    // hero1 = p1[0];
    // hero2 = p2[0];

    if (config.randomHeroes > 0) {
        for (int owner : {0, 1})
        {
            for (auto &[poolname, pool] : heropools.at(owner)) {
                ML_VERBOSE("poolname: " << poolname << ", heroes: " << pool.heroes.size() << "\n");
                if (pool.heroes.size() == 0) {
                    throw std::runtime_error("randomHeroes requires at least 1 hero in each pool.");
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

void ServerPlugin::setupBattleHook(
    const IGameInfoCallback & gameInfo,
    const CArmedInstance * attacker,
    const CArmedInstance * defender,
    const CGTownInstance *& town,
    TerrainId & terrain,
    BattleField & terType,
    BattleLayout & layout,
    ui32 & seed
) {
    if (!config.randomArmies) {
        const bool vipEnabled = config.leftVip || config.rightVip;
        const bool harEnabled = config.leftHar || config.rightHar;
        const bool uniformEnabled = config.leftUniformChance > 0 || config.rightUniformChance > 0;
        if (vipEnabled || harEnabled || uniformEnabled) {
            std::cout << "WARNING: VIP, HAR or uniform army enabled, but will have no effect because random armies are not enabled.\n";
        }
    }

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

    if (creatureBankBattle) {
        bool hasDoubleWideDefender = false;
        for (const auto & entry : defender->Slots()) {
            const auto * creature = entry.second->getCreature();
            if (creature && creature->isDoubleWide()) {
                hasDoubleWideDefender = true;
                break;
            }
        }

        const std::string layoutName = hasDoubleWideDefender ? "creatureBankWide" : "creatureBankNarrow";
        layout = BattleLayout::createLayout(gameInfo, layoutName, attacker, defender);
        town = nullptr;
        return;
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

    const auto validTargetModifier = [this](double modifier)
    {
        return std::isfinite(modifier)
            && modifier > 0
            && std::lround(config.randomArmyValueMin * modifier) >= 1
            && config.randomArmyValueMax * modifier <= std::numeric_limits<int>::max();
    };
    if(!validTargetModifier(config.leftTargetMod) || !validTargetModifier(config.rightTargetMod))
        throw std::runtime_error("random army target modifiers must be positive, finite, and produce targets within integer range");

    if(allcreatures.empty())
        throw std::runtime_error("cannot generate random armies without valid creatures");


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

    auto generateArmy = [this, &generateArmyWithStackCount](int targetValue, const std::vector<CreatureID> & creaturePool, int minStackCount, int maxStackCount) -> std::vector<GeneratedStack>
    {
        for(int attempt = 0; attempt < 100; ++attempt)
        {
            // Bias towards fuller armies while still allowing sparse ones sometimes.
            auto distSlotCount = std::uniform_int_distribution<>(minStackCount, maxStackCount);
            const int desiredStackCount = std::max(distSlotCount(rng), distSlotCount(rng));
            auto generated = generateArmyWithStackCount(targetValue, creaturePool, desiredStackCount);
            if(!generated.empty())
                return generated;
        }

        for(int stackCount = minStackCount; stackCount <= maxStackCount; ++stackCount)
        {
            auto generated = generateArmyWithStackCount(targetValue, creaturePool, stackCount);
            if(!generated.empty())
                return generated;
        }

        throw std::runtime_error("failed to generate random army with the requested stack count");
    };

    auto generateUniformArmy = [this, &divCeil, &var](int targetValue, const std::vector<CreatureID> & creaturePool, int minStackCount, int maxStackCount) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            const int stackCount = std::uniform_int_distribution<>(minStackCount, maxStackCount)(rng);
            std::vector<const CCreature *> candidates;
            for(const auto & creatureId : creaturePool)
            {
                const int creatureValue = creatureValues.at(creatureId);
                if(creatureValue <= 0)
                    continue;

                const int minQuantity = std::max(stackCount, divCeil(minTotalValue, creatureValue));
                const int maxQuantity = maxTotalValue / creatureValue;
                if(minQuantity <= maxQuantity)
                    candidates.push_back(creatureId.toCreature());
            }

            if(candidates.empty())
                continue;

            const auto * creature = candidates.at(std::uniform_int_distribution<>(0, static_cast<int>(candidates.size()) - 1)(rng));
            const int creatureValue = creatureValues.at(creature->getId());
            const int minQuantity = std::max(stackCount, divCeil(minTotalValue, creatureValue));
            const int maxQuantity = maxTotalValue / creatureValue;
            const int totalQuantity = std::uniform_int_distribution<>(minQuantity, maxQuantity)(rng);

            std::array<int, 7> slots = { 0, 1, 2, 3, 4, 5, 6 };
            std::shuffle(slots.begin(), slots.end(), rng);

            std::vector<GeneratedStack> generated;
            const int quantityPerStack = totalQuantity / stackCount;
            const int remainder = totalQuantity % stackCount;
            for(int stackIndex = 0; stackIndex < stackCount; ++stackIndex)
            {
                const int quantity = quantityPerStack + (stackIndex < remainder ? 1 : 0);
                generated.push_back({ SlotID(slots.at(stackIndex)), creature, quantity });
            }
            return generated;
        }

        throw std::runtime_error("failed to generate uniform random army with the current randomArmy config");
    };

    auto generateVipArmy = [this, &divCeil, &var, &totalArmyValue, &generateArmyWithStackCount](int targetValue, const std::vector<CreatureID> & creaturePool, int minStackCount, int maxStackCount) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));
        const auto allowed = [&creaturePool](const CreatureID & creatureId)
        {
            return std::ranges::find(creaturePool, creatureId) != creaturePool.end();
        };

        std::vector<CreatureID> guardCreatures;
        std::vector<CreatureID> shootingCreatures;
        std::ranges::copy_if(allguards, std::back_inserter(guardCreatures), allowed);
        std::ranges::copy_if(allshooters, std::back_inserter(shootingCreatures), allowed);
        if(guardCreatures.empty() || shootingCreatures.empty())
            throw std::runtime_error("cannot generate VIP random army: whitelist must contain shooter and guard creatures");

        const auto cheapestGuard = std::ranges::min_element(guardCreatures, [this](const CreatureID & left, const CreatureID & right)
        {
            return creatureValues.at(left) < creatureValues.at(right);
        });
        const int cheapestGuardValue = creatureValues.at(*cheapestGuard);

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            const int desiredStackCount = std::uniform_int_distribution<>(minStackCount, maxStackCount)(rng);
            const int guardStackCount = desiredStackCount - 1;
            const int minGuardValue = cheapestGuardValue * guardStackCount;
            const int maxGuardValue = (maxTotalValue * 7) / 10;

            if(minGuardValue > maxGuardValue)
                continue;

            const int guardTarget = std::uniform_int_distribution<>(minGuardValue, maxGuardValue)(rng);
            auto generated = generateArmyWithStackCount(guardTarget, guardCreatures, guardStackCount);
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
            for(const auto & shooterId : shootingCreatures)
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

    auto generateHarArmy = [this, &divCeil, &var](int targetValue, const std::vector<CreatureID> & creaturePool, int minimumPrimarySpeed, int minStackCount, int maxStackCount) -> std::vector<GeneratedStack>
    {
        const int minTotalValue = static_cast<int>(std::ceil(targetValue * (1 - var)));
        const int maxTotalValue = static_cast<int>(std::floor(targetValue * (1 + var)));
        std::vector<CreatureID> meleeCreatures;
        std::vector<CreatureID> primaryCreatures;
        for(const auto & creatureId : creaturePool)
        {
            const auto * creature = creatureId.toCreature();
            if(creature->hasBonusOfType(BonusType::SHOOTER))
                continue;

            meleeCreatures.push_back(creatureId);
            if(creature->getBaseSpeed() >= minimumPrimarySpeed)
                primaryCreatures.push_back(creatureId);
        }

        if(primaryCreatures.empty())
            throw std::runtime_error("cannot generate HAR random army: whitelist must contain a melee creature at least 2 speed faster than an opponent creature");

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            const auto * primary = primaryCreatures.at(std::uniform_int_distribution<>(0, static_cast<int>(primaryCreatures.size()) - 1)(rng)).toCreature();
            const int primaryUnitValue = creatureValues.at(primary->getId());
            const int otherStackCount = std::uniform_int_distribution<>(minStackCount - 1, maxStackCount - 1)(rng);

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

    const int baseTarget = std::uniform_int_distribution<>(config.randomArmyValueMin, config.randomArmyValueMax)(rng);
    const int leftTarget = static_cast<int>(std::lround(baseTarget * config.leftTargetMod));
    const int rightTarget = static_cast<int>(std::lround(baseTarget * config.rightTargetMod));
    if(IsMLVerbose())
    {
        const auto targetRange = [var](int target)
        {
            return std::pair(
                static_cast<int>(std::ceil(target * (1 - var))),
                static_cast<int>(std::floor(target * (1 + var)))
            );
        };
        const auto [leftMinimum, leftMaximum] = targetRange(leftTarget);
        const auto [rightMinimum, rightMaximum] = targetRange(rightTarget);
        std::cout
            << "Random army targets: configuredRange=[" << config.randomArmyValueMin << "," << config.randomArmyValueMax << "]"
            << ", variance=" << config.randomArmyTargetVar << "%"
            << ", base=" << baseTarget
            << ", leftMod=" << config.leftTargetMod
            << ", leftTarget=" << leftTarget
            << ", leftAllowed=[" << leftMinimum << "," << leftMaximum << "]"
            << ", rightMod=" << config.rightTargetMod
            << ", rightTarget=" << rightTarget
            << ", rightAllowed=[" << rightMinimum << "," << rightMaximum << "]\n";
    }

    // modification by reference
    army1 = hero1->getArmy();
    army2 = hero2->getArmy();

    auto generateArmyForSide = [&generateArmy, &generateUniformArmy, &generateVipArmy](int targetValue, const std::vector<CreatureID> & creaturePool, bool vip, bool uniform, int minStackCount, int maxStackCount)
    {
        if(vip)
            return generateVipArmy(targetValue, creaturePool, std::max(4, minStackCount), maxStackCount);
        if(uniform)
            return generateUniformArmy(targetValue, creaturePool, minStackCount, maxStackCount);
        return generateArmy(targetValue, creaturePool, minStackCount, maxStackCount);
    };

    auto minimumHarPrimarySpeed = [this](const std::vector<CreatureID> & opponentPool, bool opponentVip)
    {
        auto minimumSpeed = [](const std::vector<CreatureID> & creatures)
        {
            const auto creature = std::ranges::min_element(creatures, [](const CreatureID & left, const CreatureID & right)
            {
                return left.toCreature()->getBaseSpeed() < right.toCreature()->getBaseSpeed();
            });
            return creature->toCreature()->getBaseSpeed();
        };

        if(!opponentVip)
            return minimumSpeed(opponentPool) + 2;

        const auto allowed = [&opponentPool](const CreatureID & creatureId)
        {
            return std::ranges::find(opponentPool, creatureId) != opponentPool.end();
        };
        std::vector<CreatureID> guards;
        std::vector<CreatureID> shooters;
        std::ranges::copy_if(allguards, std::back_inserter(guards), allowed);
        std::ranges::copy_if(allshooters, std::back_inserter(shooters), allowed);
        if(guards.empty() || shooters.empty())
            throw std::runtime_error("cannot generate VIP HAR opponent: whitelist must contain shooter and guard creatures");

        return std::max(minimumSpeed(guards), minimumSpeed(shooters)) + 2;
    };

    auto generateHarOpponentArmy = [this, &totalArmyValue, &generateArmyForSide](int targetValue, const std::vector<CreatureID> & creaturePool, bool vip, bool uniform, int harPrimarySpeed, int minStackCount, int maxStackCount)
    {
        if(uniform)
        {
            std::vector<CreatureID> slowerCreatures;
            std::ranges::copy_if(creaturePool, std::back_inserter(slowerCreatures), [harPrimarySpeed](const CreatureID & creatureId)
            {
                return creatureId.toCreature()->getBaseSpeed() <= harPrimarySpeed - 2;
            });

            if(slowerCreatures.empty())
                throw std::runtime_error("cannot generate uniform HAR opponent army: whitelist must contain a creature at least 2 speed slower than the main HAR unit");

            return generateArmyForSide(targetValue, slowerCreatures, vip, true, minStackCount, maxStackCount);
        }

        for(int attempt = 0; attempt < 1000; ++attempt)
        {
            auto generated = generateArmyForSide(targetValue, creaturePool, vip, false, minStackCount, maxStackCount);
            const int totalValue = totalArmyValue(generated);
            const bool valid = std::ranges::all_of(generated, [this, totalValue, harPrimarySpeed](const auto & stack)
            {
                const int stackValue = creatureValues.at(stack.creature->getId()) * stack.quantity;
                return stackValue * 20 < totalValue || stack.creature->getBaseSpeed() <= harPrimarySpeed - 2;
            });

            if(valid)
                return generated;
        }

        throw std::runtime_error("failed to generate HAR opponent army satisfying the speed and value constraints");
    };

    auto rollChance = [this](int chance)
    {
        return chance > 0 && std::uniform_int_distribution<>(0, 99)(rng) < chance;
    };
    const bool leftUniform = rollChance(config.leftUniformChance);
    const bool rightUniform = rollChance(config.rightUniformChance);

    if(IsMLVerbose())
    {
        std::cout
            << "Random army modes: left={har=" << config.leftHar
            << ",vip=" << config.leftVip
            << ",uniformChance=" << config.leftUniformChance
            << ",uniform=" << leftUniform
            << ",creaturePool=" << leftCreatures.size()
            << "}, right={har=" << config.rightHar
            << ",vip=" << config.rightVip
            << ",uniformChance=" << config.rightUniformChance
            << ",uniform=" << rightUniform
            << ",creaturePool=" << rightCreatures.size()
            << "}, creatureBank=" << creatureBankBattle
            << ", mirror=" << config.mirrorArmies << "\n";
    }

    const int leftMinStackCount = creatureBankBattle && config.mirrorArmies ? 4 : 1;
    const int leftMaxStackCount = creatureBankBattle && config.mirrorArmies ? 5 : 7;
    const int rightMinStackCount = creatureBankBattle ? 4 : 1;
    const int rightMaxStackCount = creatureBankBattle ? 5 : 7;

    std::vector<GeneratedStack> generated1;
    std::vector<GeneratedStack> generated2;
    if(config.leftHar)
    {
        const int minimumPrimarySpeed = minimumHarPrimarySpeed(rightCreatures, config.rightVip);
        generated1 = generateHarArmy(leftTarget, leftCreatures, minimumPrimarySpeed, leftMinStackCount, std::min(4, leftMaxStackCount));
        generated2 = generateHarOpponentArmy(rightTarget, rightCreatures, config.rightVip, rightUniform, generated1.front().creature->getBaseSpeed(), rightMinStackCount, rightMaxStackCount);
    }
    else if(config.rightHar)
    {
        const int minimumPrimarySpeed = minimumHarPrimarySpeed(leftCreatures, config.leftVip);
        generated2 = generateHarArmy(rightTarget, rightCreatures, minimumPrimarySpeed, rightMinStackCount, rightMaxStackCount);
        generated1 = generateHarOpponentArmy(leftTarget, leftCreatures, config.leftVip, leftUniform, generated2.front().creature->getBaseSpeed(), leftMinStackCount, leftMaxStackCount);
    }
    else
    {
        generated1 = generateArmyForSide(leftTarget, leftCreatures, config.leftVip, leftUniform, leftMinStackCount, leftMaxStackCount);
        generated2 = generateArmyForSide(rightTarget, rightCreatures, config.rightVip, rightUniform, rightMinStackCount, rightMaxStackCount);
    }

    if(IsMLVerbose())
    {
        const auto logGeneratedArmy = [this, &totalArmyValue](const char * side, int target, const std::vector<GeneratedStack> & generated)
        {
            std::cout << "Generated " << side << " army: target=" << target << ", value=" << totalArmyValue(generated) << ", stacks=" << generated.size() << "\n";
            for(const auto & stack : generated)
            {
                const int unitValue = creatureValues.at(stack.creature->getId());
                std::cout
                    << "  slot=" << static_cast<int>(stack.slot)
                    << ", creature=" << stack.creature->getId().toEntity(LIBRARY)->getJsonKey()
                    << ", speed=" << stack.creature->getBaseSpeed()
                    << ", quantity=" << stack.quantity
                    << ", unitValue=" << unitValue
                    << ", stackValue=" << unitValue * stack.quantity << "\n";
            }
        };
        logGeneratedArmy("left", leftTarget, generated1);
        logGeneratedArmy("right", rightTarget, generated2);
    }

    auto replaceArmy = [this](const CGHeroInstance * hero, const std::vector<GeneratedStack> & generated)
    {
        for(int slot = 0; slot < 7; ++slot)
            if(hero->hasStackAtSlot(SlotID(slot)))
                gh->eraseStack(StackLocation(hero->id, SlotID(slot)), true);

        for(const auto & stack : generated)
            gh->insertNewStack(StackLocation(hero->id, stack.slot), stack.creature, stack.quantity);
    };

    // std::cout << "=================================\n";

    replaceArmy(hero1, generated1);
    replaceArmy(hero2, generated2);
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
    battlecounter++;

    creatureBankBattle = config.creatureBankChance > 0 &&
        std::uniform_int_distribution<>(0, 99)(rng) < config.creatureBankChance;

    if (!(hero1 && hero2)) {
        std::cout << "hero1: " << hero1 << ", hero2: " << hero2 << "\n";
        std::cout << "WARNING: hero is missing => skipping all hooks\n";
        return;
    }


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

    auto totalArmyValue = [this](const CArmedInstance * army)
    {
        int64_t total = 0;
        for(const auto & entry : army->Slots())
        {
            const auto * stack = entry.second.get();
            total += static_cast<int64_t>(creatureValues.at(stack->getCreatureID())) * stack->getCount();
        }
        return total;
    };

    if(IsMLVerbose())
        std::cout << "Army values: left=" << totalArmyValue(army1) << ", right=" << totalArmyValue(army2) << "\n";
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
