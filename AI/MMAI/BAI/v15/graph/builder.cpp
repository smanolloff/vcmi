#include "builder.h"
#include "battle/BattleSide.h"
#include "entities/building/TownFortifications.h"

namespace MMAI::BAI::V15::Graph
{

namespace
{
    Nodes::Global::TowerFlags GetSiegeTowers(const CPlayerBattleCallback * battle) {
        Nodes::Global::TowerFlags res = 0; // {upper, middle, lower}

        auto has = [&battle](EWallPart part) {
            auto ws = battle->battleGetWallState(part);
            return ws != EWallState::NONE && ws != EWallState::DESTROYED;
        };

        if (has(EWallPart::UPPER_TOWER))
            res.set(0);
        if (has(EWallPart::KEEP))
            res.set(1);
        if (has(EWallPart::BOTTOM_TOWER))
            res.set(2);

        return res;
    }

    Nodes::Global::CorpseFlags GetSiegeCorpses(const CPlayerBattleCallback * battle)
    {
        Nodes::Global::CorpseFlags res = 0; // {gate, bridge}

        if(battle->battleGetFortifications().wallsHealth == 0)
            return res;

        for(const auto & cstack : battle->battleGetAllStacks(false))
        {
            if(cstack->alive())
                continue;

            if(cstack->coversPos(BattleHex::GATE_INNER) || cstack->coversPos(BattleHex::GATE_OUTER))
                res.set(0);
            if (cstack->coversPos(BattleHex::GATE_BRIDGE))
                res.set(1);
        };

        return res;
    }
}

Builder::Builder(
    const CPlayerBattleCallback * battle,
    const CStack * acstack
)
    : battle(battle)
    , acstack(acstack)
    , G(std::make_shared<Graph>())
    , cache(battle, G)
{
}


void Builder::addGlobalNode(
    S15::CombatResult result,
    int round,
    int valueStart,
    int hpStart,
    int value,
    int hp
)
{
    const auto side = acstack ? acstack->unitSide() : battle->battleGetMySide();
    G->add(std::make_shared<Nodes::Global>(
        side,
        result,
        round,
        valueStart,
        value,
        hpStart,
        hp,
        GetSiegeTowers(battle),
        GetSiegeCorpses(battle)
    ));
}

void Builder::addPlayerNode(
    BattleSide side,
    int globalValueStart,
    int globalHpStart,
    int globalValuePrevRound,
    int globalHpPrevRound,
    int value,
    int hp,
    int dmgDealt,
    int dmgReceived,
    int valueKilled,
    int valueLost
)
{
    G->add(std::make_shared<Nodes::Player>(
        side,
        globalValueStart,
        globalHpStart,
        globalValuePrevRound,
        globalHpPrevRound,
        value,
        hp,
        dmgDealt,
        dmgReceived,
        valueKilled,
        valueLost
    ));
}

}
