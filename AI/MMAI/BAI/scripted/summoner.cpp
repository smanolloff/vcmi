#include "StdInc.h"
#include "lib/AI_Base.h"
#include "summoner.h"
#include "lib/CStack.h"
#include "CCallback.h"
#include "lib/CCreatureHandler.h"
#include "lib/battle/BattleAction.h"
#include "lib/battle/BattleInfo.h"
#include "lib/spells/CSpellHandler.h"
#include "battle/ReachabilityInfo.h"
#include "vcmi/Environment.h"
#include <algorithm>
#include <random>

namespace MMAI::BAI::Scripted {
    Summoner::Summoner()
        : side(-1)
        , wasWaitingForRealize(false)
        , wasUnlockingGs(false)
    {
        print("created");
    }

    Summoner::~Summoner()
    {
        print("destroyed");
        if(cb)
        {
            //Restore previous state of CB - it may be shared with the main AI (like VCAI)
            cb->waitTillRealize = wasWaitingForRealize;
            cb->unlockGsWhenWaiting = wasUnlockingGs;
        }
    }

    void Summoner::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB)
    {
        print("init called, saving ptr to IBattleCallback");
        env = ENV;
        cb = CB;

        wasWaitingForRealize = CB->waitTillRealize;
        wasUnlockingGs = CB->unlockGsWhenWaiting;
        CB->waitTillRealize = false;
        CB->unlockGsWhenWaiting = false;
    }

    void Summoner::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences)
    {
        initBattleInterface(ENV, CB);
    }

    void Summoner::actionFinished(const BattleID & battleID, const BattleAction &action)
    {
        print("actionFinished called");
    }

    void Summoner::actionStarted(const BattleID & battleID, const BattleAction &action)
    {
        print("actionStarted called");
    }

    class EnemyInfo
    {
    public:
        const CStack * s;
        int adi;
        int adr;
        bool canshoot;
        std::vector<BattleHex> attackFrom; //for melee fight
        EnemyInfo(const CStack * _s, bool canshoot_) : s(_s), adi(0), adr(0), canshoot(canshoot_)
        {}
        void calcDmg(std::shared_ptr<CBattleCallback> cb, const BattleID & battleID, const CStack * ourStack)
        {
            // FIXME: provide distance info for Jousting bonus
            DamageEstimation retal;
            const auto bai = BattleAttackInfo(ourStack, s, 0, canshoot);
            DamageEstimation dmg = cb->getBattle(battleID)->battleEstimateDamage(bai, &retal);

            adi = static_cast<int>((dmg.damage.min + dmg.damage.max) / 2);
            adr = static_cast<int>((retal.damage.min + retal.damage.max) / 2);
        }

        bool operator==(const EnemyInfo& ei) const
        {
            return s == ei.s;
        }
    };

    bool isMoreProfitable(const EnemyInfo &ei1, const EnemyInfo& ei2)
    {
        return (ei1.adi-ei1.adr) < (ei2.adi - ei2.adr);
    }

    static bool willSecondHexBlockMoreEnemyShooters(std::shared_ptr<CBattleCallback> cb, const BattleID & battleID, const BattleHex &h1, const BattleHex &h2)
    {
        int shooters[2] = {0}; //count of shooters on hexes

        for(int i = 0; i < 2; i++)
        {
            for (auto & neighbour : (i ? h2 : h1).neighbouringTiles())
                if(const auto * s = cb->getBattle(battleID)->battleGetUnitByPos(neighbour))
                    if(s->isShooter())
                        shooters[i]++;
        }

        return shooters[0] < shooters[1];
    }

    void Summoner::yourTacticPhase(const BattleID & battleID, int distance)
    {
        cb->battleMakeTacticAction(battleID, BattleAction::makeEndOFTacticPhase(cb->getBattle(battleID)->battleGetTacticsSide()));
    }

    void Summoner::activeStack(const BattleID & battleID, const CStack * stack)
    {
        if (!castThisRound && spellToCast && hero->mana >= spellCost) {
            castThisRound = true;

            BattleAction spellcast;
            spellcast.actionType = EActionType::HERO_SPELL;
            spellcast.spell = spellToCast->id;
            // spellcast.setTarget();
            spellcast.side = side;
            spellcast.stackNumber = side ? -2 : -1;

            print("Casting spell with cost " + std::to_string(spellCost) + " / " + std::to_string(hero->mana));
            cb->battleMakeSpellAction(battleID, spellcast);
            return;
        }
        // for (auto const & s : VLC->spellh->objects)
        //   if (s->canBeCast(cb->getBattle(battleID).get(), spells::Mode::HERO, hero))
        //     possibleSpells.push_back(s.get());


        //boost::this_thread::sleep_for(boost::chrono::seconds(2));
        print("activeStack called for " + stack->nodeName());
        ReachabilityInfo dists = cb->getBattle(battleID)->getReachability(stack);
        bool canshoot = cb->getBattle(battleID)->battleCanShoot(stack);
        std::vector<BattleHex> avHexes = cb->getBattle(battleID)->battleGetAvailableHexes(dists, stack, false);
        std::map<const CStack*, EnemyInfo> enemiesShootable;
        std::map<const CStack*, EnemyInfo> enemiesReachable;
        std::map<const CStack*, EnemyInfo> enemiesUnreachable;

        if(stack->creatureId() == CreatureID::CATAPULT)
        {
            BattleAction attack;
            attack.side = stack->unitSide();
            attack.stackNumber = stack->unitId();
            attack.actionType = EActionType::CATAPULT;

            if(battle->battleGetGateState() == EGateState::CLOSED) {
                attack.aimToHex(battle->wallPartToBattleHex(EWallPart::GATE));
                cb->battleMakeUnitAction(battleID, attack);
                return;
            }

            using WP = EWallPart;
            auto wallparts = {
                WP::KEEP, WP::BOTTOM_TOWER, WP::UPPER_TOWER,
                WP::BELOW_GATE, WP::OVER_GATE, WP::BOTTOM_WALL, WP::UPPER_WALL
            };

            for (auto wp : wallparts) {
                using WS = EWallState;
                auto ws = battle->battleGetWallState(wp);
                if(ws == WS::REINFORCED || ws == WS::INTACT || ws == WS::DAMAGED) {
                    attack.aimToHex(battle->wallPartToBattleHex(wp));
                    cb->battleMakeUnitAction(battleID, attack);
                    return;
                }
            }

            cb->battleMakeUnitAction(battleID, BattleAction::makeDefend(stack));
            return;

        }
        else if(stack->hasBonusOfType(BonusType::SIEGE_WEAPON))
        {
            cb->battleMakeUnitAction(battleID, BattleAction::makeDefend(stack));
            return;
        }

        for (const CStack *s : cb->getBattle(battleID)->battleGetStacks(CBattleInfoEssentials::ONLY_ENEMY))
        {

            if(canshoot)
            {
                enemiesShootable.insert({s, EnemyInfo(s, canshoot)});
            }
            else
            {
                for (BattleHex hex : avHexes)
                {
                    if(CStack::isMeleeAttackPossible(stack, s, hex))
                    {
                        auto i = enemiesReachable.find(s);
                        if(i == enemiesReachable.end()) {
                            auto ei = EnemyInfo(s, canshoot);
                            ei.attackFrom.push_back(hex);
                            enemiesReachable.insert({s, ei});
                        } else {
                            i->second.attackFrom.push_back(hex);
                        }
                        break;
                    }
                }

                if(enemiesReachable.find(s) == enemiesReachable.end() && s->getPosition().isValid()) {
                    enemiesUnreachable.insert({s, EnemyInfo(s, canshoot)});
                }
            }
        }

        for ( auto & [_, ei] : enemiesReachable )
            ei.calcDmg(cb, battleID, stack);

        for ( auto & [_, ei] : enemiesShootable )
            ei.calcDmg(cb, battleID, stack);

        if(enemiesShootable.size())
        {
            const EnemyInfo &ei= std::max_element(enemiesShootable.begin(), enemiesShootable.end(), [](auto a, auto b) { return isMoreProfitable(a.second, b.second); })->second;
            cb->battleMakeUnitAction(battleID, BattleAction::makeShotAttack(stack, ei.s));
            return;
        }
        else if(enemiesReachable.size())
        {
            const EnemyInfo &ei= std::max_element(enemiesReachable.begin(), enemiesReachable.end(), [](auto a, auto b) { return isMoreProfitable(a.second, b.second); })->second;
            BattleHex targetHex = *std::max_element(ei.attackFrom.begin(), ei.attackFrom.end(), [&](auto a, auto b) { return willSecondHexBlockMoreEnemyShooters(cb, battleID, a, b);});

            cb->battleMakeUnitAction(battleID, BattleAction::makeMeleeAttack(stack, ei.s->getPosition(), targetHex));
            return;
        }
        else if(enemiesUnreachable.size()) //due to #955 - a buggy battle may occur when there are no enemies
        {
            const EnemyInfo &closestEnemy = std::min_element(enemiesUnreachable.begin(), enemiesUnreachable.end(), [&](auto a, auto b) { return dists.distToNearestNeighbour(stack, a.second.s) < dists.distToNearestNeighbour(stack, b.second.s); })->second;

            if(dists.distToNearestNeighbour(stack, closestEnemy.s) < GameConstants::BFIELD_SIZE)
            {
                cb->battleMakeUnitAction(battleID, goTowards(battleID, stack, closestEnemy.s->getAttackableHexes(stack), dists, avHexes));
                return;
            }
        }

        cb->battleMakeUnitAction(battleID, BattleAction::makeDefend(stack));
        return;
    }

    void Summoner::battleAttack(const BattleID & battleID, const BattleAttack *ba)
    {
        print("battleAttack called");
    }

    void Summoner::battleStacksAttacked(const BattleID & battleID, const std::vector<BattleStackAttacked> & bsa, bool ranged)
    {
        print("battleStacksAttacked called");
    }

    void Summoner::battleEnd(const BattleID & battleID, const BattleResult *br, QueryID queryID)
    {
        print("battleEnd called");
    }

    // void Summoner::battleResultsApplied()
    // {
    //  print("battleResultsApplied called");
    // }

    void Summoner::battleNewRoundFirst(const BattleID & battleID)
    {
        print("battleNewRoundFirst called");
    }

    void Summoner::battleNewRound(const BattleID & battleID)
    {
        print("battleNewRound called");
        castThisRound = false;
    }

    void Summoner::battleStackMoved(const BattleID & battleID, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport)
    {
        print("battleStackMoved called");
    }

    void Summoner::battleSpellCast(const BattleID & battleID, const BattleSpellCast *sc)
    {
        print("battleSpellCast called");
    }

    void Summoner::battleStacksEffectsSet(const BattleID & battleID, const SetStackEffect & sse)
    {
        print("battleStacksEffectsSet called");
    }

    void Summoner::battleStart(const BattleID & battleID, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
    {
        print("battleStart called bid=" + std::to_string(static_cast<int>(battleID)));

        side = Side;
        battle = cb->getBattle(battleID);
        hero = battle->battleGetMyHero();

        auto spells = std::array<SpellID, 4> {
            SpellID::SUMMON_AIR_ELEMENTAL,
            SpellID::SUMMON_WATER_ELEMENTAL,
            SpellID::SUMMON_EARTH_ELEMENTAL,
            SpellID::SUMMON_FIRE_ELEMENTAL
        };

        auto dist = std::uniform_int_distribution<>(0, 3);
        auto rng = std::mt19937(hero1->exp);
        auto roll = dist(rng);
        if (roll < spells.size()) {
            spellToCast = spells.at(roll).toSpell();

            if (spellToCast->canBeCast(battle.get(), spells::Mode::HERO, hero)) {
                // can get lower if enemy magic dampers die, which could (very rarely)
                // result in 1 more cast, but better to avoid checking it on each turn
                spellCost = battle->battleGetSpellCost(spellToCast, battle->battleGetMyHero());
                print("Can cast " + spellToCast->identifier);
            } else {
                print("Can NOT cast " + spellToCast->identifier);
                spellToCast = nullptr;
            }
        }

        print("Starting mana: " + std::to_string(hero->mana) + ", will cast " + (spellToCast ? spellToCast->identifier : "no spells") + " this combat.");
    }

    void Summoner::battleCatapultAttacked(const BattleID & battleID, const CatapultAttack & ca)
    {
        print("battleCatapultAttacked called");
    }

    void Summoner::print(const std::string &text) const
    {
        logAi->debug("Summoner  [%p]: %s", this, text);
    }

    BattleAction Summoner::goTowards(const BattleID & battleID, const CStack * stack, std::vector<BattleHex> hexes, const ReachabilityInfo &reachability, std::vector<BattleHex> &avHexes) const
    {
        if(!avHexes.size() || !hexes.size()) //we are blocked or dest is blocked
        {
            return BattleAction::makeDefend(stack);
        }

        std::sort(hexes.begin(), hexes.end(), [&](BattleHex h1, BattleHex h2) -> bool
        {
            return reachability.distances[h1] < reachability.distances[h2];
        });

        for(auto hex : hexes)
        {
            if(vstd::contains(avHexes, hex))
                return BattleAction::makeMove(stack, hex);

            if(stack->coversPos(hex))
            {
                logAi->warn("Warning: already standing on neighbouring tile!");
                //We shouldn't even be here...
                return BattleAction::makeDefend(stack);
            }
        }

        BattleHex bestNeighbor = hexes.front();

        if(reachability.distances[bestNeighbor] > GameConstants::BFIELD_SIZE)
        {
            return BattleAction::makeDefend(stack);
        }

        if(stack->hasBonusOfType(BonusType::FLYING))
        {
            // Flying stack doesn't go hex by hex, so we can't backtrack using predecessors.
            // We just check all available hexes and pick the one closest to the target.
            auto nearestAvailableHex = vstd::minElementByFun(avHexes, [&](BattleHex hex) -> int
            {
                return BattleHex::getDistance(bestNeighbor, hex);
            });

            return BattleAction::makeMove(stack, *nearestAvailableHex);
        }
        else
        {
            BattleHex currentDest = bestNeighbor;
            while(1)
            {
                if(!currentDest.isValid())
                {
                    logAi->error("CBattleAI::goTowards: internal error");
                    return BattleAction::makeDefend(stack);
                }

                if(vstd::contains(avHexes, currentDest))
                    return BattleAction::makeMove(stack, currentDest);

                currentDest = reachability.predecessors[currentDest];
            }
        }
    }
}
