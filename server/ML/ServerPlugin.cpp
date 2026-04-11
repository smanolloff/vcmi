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
#include <cstdlib>
#include <limits>
#include <random>
#include <stdexcept>
#include <regex>

VCMI_LIB_NAMESPACE_BEGIN

namespace ML {

namespace {
    int calculateValue(const CCreature * cr)
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
                values.try_emplace(creature->getId(), calculateValue(creature.get()));

        return values;
    }

    /*
     * Organize heroes into pools based on hero name
     * Format: "hero_<INT>_pool_<STR>"
     * Example: "hero_5123_pool_150k"
     */
    std::map<std::string, HeroPool> InitHeroPools(CGameState* gs) {
        auto pattern = std::regex(R"(^hero_\d+_pool_([0-9A-Za-z]+)$)");
        auto res = std::map<std::string, HeroPool> {};
        int poolid = 0;

        for (const auto &heroid : gs->getMap().getHeroesOnMap()) {
            auto *hero = gs->getHero(heroid);
            std::string poolname = "default";  // fallback poolname
            std::smatch matches;

            // Check if the entire string matches the pattern
            if (std::regex_match(hero->nameCustomTextId, matches, pattern))
                poolname = matches[1].str();

            if (!res.contains(poolname))
                res.insert({poolname, HeroPool(poolid++, poolname)});

            auto it = res.find(poolname);
            if (it == res.end()) {
                res.insert({poolname, HeroPool(poolid++, poolname)});
                it = res.find(poolname);
            }

            auto &pool = it->second;
            pool.heroes.push_back(hero);
        }

        std::cout << "Grouped heroes into " << res.size() << " pools\n";
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
                    // std::cout << "Filtering out " << bf.getInfo()->getJsonKey() << "\n";
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
                    // std::cout << "Filtering out " << bi->getJsonKey() << "\n";
                }
            }
        });

        if (!battlefieldPattern.empty()) {
            std::cout << "Filtered battlefields matching pattern: '" << battlefieldPattern << "'\n";

            if (res.size() == 0) {
                std::cout << "ALL BATTLEFIELDS WERE FILTERED OUT\n";
            }

            for (auto &[bi, terrains] : res) {
                std::cout << bi->getJsonKey() << " ->";
                for (auto &t : terrains) {
                    std::cout << " " << t->getJsonKey();
                }
                std::cout << "\n";
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
            // Invalid creatures (arrow towers, war machines, NOT_USED, etc. have lvl=0)
            if (cr->getLevel() > 0)
                res.push_back(cr->getId());
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
                // std::cout << "ADDING SHOOTER: " << cr->getId() << " | " << cr->getGrowth() << " | " << cr->getNameSingularTranslated() << "\n";
                res.push_back(cr->getId());
            }
        });

        return res;
    }

    // Just low-tier creatures (levels 1..3)
    std::vector<CreatureID> InitGuardingCreatures() {
        auto res = std::vector<CreatureID>{};

        LIBRARY->creatures()->forEach([&res](const Creature * cr, bool &stop) {
            // Invalid creatures (arrow towers, war machines, NOT_USED, etc. have lvl=0)
            // "special" creatures (property not exposed, but e.g. ballistas have lvl=4 and growth=0)
            if (cr->getLevel() > 0 && cr->getGrowth() && cr->getLevel() < 4) {
                // std::cout << "ADDING GUARD: " << cr->getId() << " | " << cr->getGrowth() << " | " << cr->getNameSingularTranslated() << "\n";
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
, stats(InitStats(gs, config, heropools.size(), heropools.begin()->second.heroes.size()))
, rng(std::mt19937(config.rngSeed ? config.rngSeed : gh->getRandomGenerator().nextInt(0, std::numeric_limits<int>::max())))
, creatureValues(InitCreatureValues())
{
    // XXX: Take out the first two heroes from the first heropool
    if (config.leftVipChance > 0 || config.rightVipChance > 0) {
        auto & p = heropools.begin()->second.heroes;

        if (p.size() < 4) {
            std::cout << "WARNING: VipChance > 0, but there are less than 4 total on this map. Will not enable VIP shooters.\n";
            config.leftVipChance = 0;
            config.rightVipChance = 0;
        } else {
            vipHero1 = p[0];
            vipHero2 = p[1];

            p.erase(p.begin(), p.begin() + 2);

            // Mark heres with "VIP shooter" armies via grail in backpack
            auto grailId = ArtifactID::GRAIL;
            for (const auto & h : {vipHero1, vipHero2}) {
                auto artloc = ArtifactLocation(h->id, ArtifactPosition::BACKPACK_START);
                // XXX: createArtifact (via GS, not GH) must be done BEFORE map is sent to clients?
                // (does not work if done in setupBattle hook, for example: client does not see new artifact)
                const auto * art = gs->createArtifact(grailId);
                // std::cout << "+++++ ADD grail (ArtifactInstanceID=" << art->getId() << ", ArtifactID=" << art->getTypeId() << ") to hero (ObjectInstanceID=" << h->id << ")\n";
                h->putArtifact(artloc.slot, art);
                // gh->putArtifact(artloc, art->getId(), false);
            }
        }
    }


    if (config.randomHeroes > 0) {
        for (auto &[poolname, pool] : heropools) {
            // std::cout << "poolname: " << poolname << ", heroes: " << pool.heroes.size() << "\n";
            if (pool.heroes.size() % 2 != 0) {
                throw std::runtime_error("An even number of heroes is required in each hero pool.");
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

    if (poolcounter % heropools.size() == 0)
        poolcounter = 0; // no shuffling for std::map

    auto it = heropools.begin();
    std::advance(it, poolcounter);
    auto &pool = it->second;

    if (pool.counter % pool.heroes.size() == 0) {
        pool.counter = 0;
        std::shuffle(pool.heroes.begin(), pool.heroes.end(), rng);

        // for (int i=0; i<allheroes.size(); i++)
        //  printf("allheroes[%d] = %s\n", i, allheroes.at(i)->getNameTextID().c_str());
    }
    // printf("poolcounter = %d\n", poolcounter);

    // XXX: heroes must be different (objects must have different tempOwner)
    // modification by reference
    hero1 = pool.heroes.at(pool.counter);
    hero2 = pool.heroes.at(pool.counter+1);

    // printf("Pool: %s, hero0: %s, hero1: %s\n", pool.name.c_str(), hero1->nameCustomTextId.c_str(), hero2->nameCustomTextId.c_str());

    if (battlecounter % config.randomHeroes == 0) {
        poolcounter += 1;
        pool.counter += 2;
    }

    // modification by reference
    army1 = hero1->getArmy();
    army2 = hero2->getArmy();
    // std::cout << "Pool: " << it->first << ", " << hero1->nameCustomTextId << " vs. " << hero2->nameCustomTextId << "\n";
}

void ServerPlugin::_setVipArmy(
    CGHeroInstance * heroA,
    CGHeroInstance * heroB
) {
    auto dist100 = std::uniform_int_distribution<>(0, 99);
    auto distShooterCreatures = std::uniform_int_distribution<>(0, allshooters.size() - 1);
    auto distGuardingCreatures = std::uniform_int_distribution<>(0, allguards.size() - 1);
    auto distGuardStacks = std::uniform_int_distribution<>(3, 6);
    auto distStrength = std::uniform_real_distribution<float>(0.5, 1.5);
    auto distSlot = std::uniform_int_distribution<>(0, 6);

    const auto *guard = allguards.at(distGuardingCreatures(rng)).toCreature();
    const auto *shooter = allshooters.at(distShooterCreatures(rng)).toCreature();
    auto shooterslot = SlotID(distSlot(rng));

    auto totalValue = 0;
    auto guardsToAdd = distGuardStacks(rng);
    auto guardsAdded = 0;

    for (int i = 0; i < 7; ++i) {
        auto islot = SlotID(i);

        // (A) Erase all stacks
        if (heroA->hasStackAtSlot(islot))
            gh->eraseStack(StackLocation(heroA->id, islot), true);

        // (A) Insert guards at non-shooter slots
        if (islot != shooterslot && guardsAdded < guardsToAdd){
            gh->insertNewStack(StackLocation(heroA->id, islot), guard, 1);
            ++guardsAdded;
        }

        // (B) Calculate value of all stacks
        if (heroB->hasStackAtSlot(islot)) {
            const auto & cstack = heroB->getStack(islot);
            totalValue += creatureValues.at(cstack.getCreatureID()) * cstack.getCount();
        }
    }

    int shooterQty = distStrength(rng) * (totalValue / creatureValues.at(shooter->getId()));
    if (shooterQty == 0)
        shooterQty = 1;

    // Insert shooter stack
    gh->insertNewStack(StackLocation(heroA->id, SlotID(shooterslot)), shooter, shooterQty);
}

void ServerPlugin::handleVips(
    const CArmedInstance *&army1,
    const CArmedInstance *&army2,
    const CGHeroInstance *&hero1,
    const CGHeroInstance *&hero2
) {
    auto dist100 = std::uniform_int_distribution<>(0, 99);

    if (dist100(rng) < config.leftVipChance) {
        // XXX: heroes must be different (objects must have different tempOwner)
        // modification by reference
        hero1 = vipHero1;
        army1 = hero1->getArmy();
        _setVipArmy(const_cast<CGHeroInstance*>(hero1), const_cast<CGHeroInstance*>(hero2));
    }

    if (dist100(rng) < config.rightVipChance) {
        hero2 = vipHero2;
        army2 = hero2->getArmy();
        _setVipArmy(const_cast<CGHeroInstance*>(hero2), const_cast<CGHeroInstance*>(hero1));
    }

}

void ServerPlugin::handleRandomStacks(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    auto dist100 = std::uniform_int_distribution<>(0, 99);
    auto distCreatures = std::uniform_int_distribution<>(0, allcreatures.size() - 1);
    auto distQuantity = std::uniform_int_distribution<>(0, 1000);

    auto maybeAddStack = [this, &dist100, &distQuantity, &distCreatures](const CGHeroInstance *hero, int slot) {
        if (hero->hasStackAtSlot(SlotID(slot))) {
            if (hero->stacksCount() == 1)
                return;

            // Roll whether to remove existing stack
            if (dist100(rng) >= config.randomStackChance)
                return;

            // gh->giveHeroNewArtifact(hero, )
            gh->eraseStack(StackLocation(hero->id, SlotID(slot)));
        }

        // Roll whether to add a new stack
        if (dist100(rng) >= config.randomStackChance)
            return;

        const auto * cr = allcreatures.at(distCreatures(rng)).toCreature();
        auto qty = distQuantity(rng);
        // Crude guard against absurd stacks such as 1K azure dragons
        qty = std::max(1, qty / cr->getLevel());
        // printf("Adding %d %s to slot %d for hero %s\n", qty, cr->getNamePluralTranslated().c_str(), slot, hero->nameCustomTextId.c_str());
        gh->insertNewStack(StackLocation(hero->id, SlotID(slot)), cr, qty);
    };

    // Don't add stacks to VIP armies
    if (hero1 != vipHero1)
        for (int i=0; i<7; ++i)
            maybeAddStack(hero1, i);

    if (hero2 != vipHero2)
        for (int i=0; i<7; ++i)
            maybeAddStack(hero2, i);
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
    auto dist = std::uniform_int_distribution<>(0, 20);
    gh->changePrimSkill(hero1, PrimarySkill::ATTACK, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changePrimSkill(hero1, PrimarySkill::DEFENSE, dist(rng), ChangeValueMode::ABSOLUTE);
}

void ServerPlugin::handleRandomSecondarySkills(const CGHeroInstance * hero1, const CGHeroInstance * hero2) {
    auto dist = std::uniform_int_distribution<>(MasteryLevel::NONE, MasteryLevel::EXPERT);
    gh->changeSecSkill(hero1, SecondarySkill::BALLISTICS, dist(rng), ChangeValueMode::ABSOLUTE);
    gh->changeSecSkill(hero2, SecondarySkill::BALLISTICS, dist(rng), ChangeValueMode::ABSOLUTE);
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
    handleVips(army1, army2, hero1, hero2);
    handleRandomStacks(hero1, hero2);
    handleWarmachines(hero1, hero2);
    handleTightFormation(hero1, hero2);
    handleMinMaxMana(hero1, hero2);
    handleSwapSides(hero1, hero2);
    handleRandomPrimarySkills(hero1, hero2);
    handleRandomSecondarySkills(hero1, hero2);
}

void ServerPlugin::endBattleHook(
    BattleResult * br,
    const CGHeroInstance * heroAttacker,
    const CGHeroInstance * heroDefender
) {
    // don't record stats for retreats (i.e. env resets)
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

        auto [attackerID, poolname] = extractHeroID(heroAttacker->nameCustomTextId);
        auto [defenderID, poolname2] = extractHeroID(heroDefender->nameCustomTextId);

        if (poolname != poolname2)
            throw std::runtime_error("Pools do not match: " + poolname + " <> " + poolname2);

        auto it = heropools.find(poolname);
        if (it == heropools.end())
            throw std::runtime_error("Could not pool with name: " + poolname);

        auto poolid = it->second.id;
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
