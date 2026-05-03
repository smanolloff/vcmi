#include "BAI/v15/graph/graph.h"
#include "entities/building/TownFortifications.h"
#include <stdexcept>

namespace MMAI::BAI::V15::Graph
{

using ET = S15::Graph::ElementType;

namespace
{
    Nodes::Global::TowerFlags GetSiegeTowers(const CPlayerBattleCallback & battle) {
        Nodes::Global::TowerFlags res = 0; // {upper, middle, lower}

        auto has = [&battle](EWallPart part) {
            auto ws = battle.battleGetWallState(part);
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

    Nodes::Global::CorpseFlags GetSiegeCorpses(const CPlayerBattleCallback & battle)
    {
        Nodes::Global::CorpseFlags res = 0; // {gate, bridge}

        if(battle.battleGetFortifications().wallsHealth == 0)
            return res;

        for(const auto & cstack : battle.battleGetAllStacks(false))
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

std::vector<const S15::Graph::INode*>
Graph::getNodes(Schema::V15::Graph::ElementType t) const
{
    auto convert = [](const auto & elementStore)
    {
        std::vector<const S15::Graph::INode*> res;
        const auto entries = elementStore.entries();
        res.reserve(entries.size());
        for (const auto & e : entries)
            res.push_back(&e);
        return res;
    };

    switch (t)
    {
        case ET::NODE_ACTION:
            return convert(getElementStore<Nodes::Action>());
        case ET::NODE_GLOBAL:
            return convert(getElementStore<Nodes::Global>());
        case ET::NODE_PLAYER:
            return convert(getElementStore<Nodes::Player>());
        case ET::NODE_UNIT:
            return convert(getElementStore<Nodes::Unit>());
        case ET::NODE_HEX:
            return convert(getElementStore<Nodes::Hex>());
        default:
            throw std::runtime_error(
                "Unexpected node element type: " + std::to_string(EU(t))
            );
    }
}

std::vector<const S15::Graph::IEdge*>
Graph::getEdges(Schema::V15::Graph::ElementType t) const
{
    auto convert = [](const auto& store)
    {
        std::vector<const S15::Graph::IEdge*> res;
        auto entries = store.entries();
        res.reserve(entries.size());
        for (const auto& elem : entries)
            res.push_back(&elem);
        return res;
    };

    switch (t)
    {
        case ET::EDGE_HEX_ADJACENT_HEX:
            return convert(getElementStore<Edges::Hex_Adjacent_Hex>());
        case ET::EDGE_UNIT_ACTS_BEFORE_UNIT:
            return convert(getElementStore<Edges::Unit_ActsBefore_Unit>());
        case ET::EDGE_UNIT_MELEE_DMG_UNIT:
            return convert(getElementStore<Edges::Unit_MeleeDmg_Unit>());
        case ET::EDGE_UNIT_RANGED_DMG_UNIT:
            return convert(getElementStore<Edges::Unit_RangedDmg_Unit>());
        case ET::EDGE_ACTION_EXPOSES_TO_UNIT:
            return convert(getElementStore<Edges::Action_ExposesTo_Unit>());
        case ET::EDGE_ACTION_THREATENS_UNIT:
            return convert(getElementStore<Edges::Action_Threatens_Unit>());
        case ET::EDGE_ACTION_DAMAGES_UNIT:
            return convert(getElementStore<Edges::Action_Damages_Unit>());
        case ET::EDGE_ACTION_ENDS_AT_HEX:
            return convert(getElementStore<Edges::Action_EndsAt_Hex>());
        case ET::EDGE_ACTION_BY_UNIT:
            return convert(getElementStore<Edges::Action_By_Unit>());
        case ET::EDGE_UNIT_BLOCKS_UNIT:
            return convert(getElementStore<Edges::Unit_Blocks_Unit>());
        case ET::EDGE_UNIT_CAN_MELEE_UNIT:
            return convert(getElementStore<Edges::Unit_CanMelee_Unit>());
        case ET::EDGE_UNIT_CAN_SHOOT_UNIT:
            return convert(getElementStore<Edges::Unit_CanShoot_Unit>());
        case ET::EDGE_UNIT_THREATENS_HEX:
            return convert(getElementStore<Edges::Unit_Threatens_Hex>());
        case ET::EDGE_UNIT_OCCUPIES_HEX:
            return convert(getElementStore<Edges::Unit_Occupies_Hex>());
        default:
            throw std::runtime_error("Unexpected edge element type: " + std::to_string(EU(t)));
    }
}

// XXX: the only difference between regular and per-stack accessibility
// is that the stack's own hexes are ACCESSIBLE instead of ALIVE_STACK
// For the purposes of MMAI observation we always want them as ALIVE_STACK
const AccessibilityInfo & Graph::getAccessibility() const
{
    if (!acache)
        throw std::runtime_error("getAccessibility: cache not built");

    return *acache;
}

void Graph::cacheAccessibility()
{
    if (acache)
        throw std::runtime_error("getAccessibility: cache already built");
    acache = std::make_unique<AccessibilityInfo>(battle.getAccessibility());
}


const ReachabilityInfo & Graph::getReachability(const CStack & cstack) const
{
    if(!haveReachability)
        throw std::runtime_error("getReachability: cache not built");

    const auto & it = rcache.find(cstack.unitId());
    if(it == rcache.end())
        throw std::runtime_error("getReachability: could not find entry for cstack");

    return it->second;
}

void Graph::cacheReachability()
{
    if(haveReachability)
        throw std::runtime_error("cacheReachability: cache already built");

    haveReachability = true;

    for (const auto & unit : getAll<Nodes::Unit>()) {
        const auto & cstack = unit.cstack;
        auto rinfo = battle.getReachability(&cstack);
        auto dists = rinfo.distances;  // must not mutate rinfo => copy
        auto attacker = cstack.unitSide() == BattleSide::ATTACKER;

        if (cstack.doubleWide()) {
            for (int i=0; i<dists.size(); ++i) {
                const auto rhex = BattleHex(i);
                if(!rhex.isAvailable())
                    continue;

                const auto fhex = rhex.cloneInDirection(attacker ? BattleHex::RIGHT : BattleHex::LEFT, false);
                if(!fhex.isAvailable())
                    continue;

                // RUFR logic (Rear-Unreachable-with-Front-Reachable)
                // VCMI does not allow moving onto such hexes.
                // MMAI explicitly allows it, treating it as a MOVE to the front hex.
                if(!rinfo.isReachable(rhex.toInt()) && rinfo.isReachable(fhex.toInt())) {
                    dists[rhex.toInt()] = dists[fhex.toInt()];
                    rufrHexes[cstack.unitId()][rhex.toInt()] = true;
                }
            }
        }

        rcache.try_emplace(cstack.unitId(), rinfo);
    }

    if (rcache.empty())
    {
        // This should only happen on an empty battlefield (draw?)
        // => throw only if there are units still alive
        for (const auto * cstack : battle.battleGetAllStacks(false))
            if (cstack->alive())
                throw std::runtime_error("cacheReachability: graph contains no units");
    }
}

bool Graph::isRUFR(const CStack & cstack, const BattleHex & bh) const
{
    // rufrHexes is built as part of reachability
    if(!haveReachability)
        throw std::runtime_error("isRUFR: reachability cache not built");

    if(!(cstack.doubleWide() && bh.isAvailable()))
        return false;

    const auto & it = rufrHexes.find(cstack.unitId());
    if(it == rufrHexes.end())
        return false;

    return it->second.at(bh.toInt());
}

// result is a vector<UnitID>
// XXX: there is a bug in VCMI when high morale occurs:
//      - the stack acts as if it's already the next unit's turn
//      - as a result, QueuePos for the ACTIVE stack is non-0
//        while the QueuePos for the next (non-active) stack is 0
// (this applies only to good morale; bad morale simply skips turn)
// As a workaround, a "isMorale" flag is passed whenever the astack is
// acting because of high morale and queue is "shifted" accordingly.
const Nodes::Unit::Queue & Graph::getQueue() const
{
    if (!queue)
        throw std::runtime_error("getQueue: cache not built");

    return *queue;
}

void Graph::cacheQueue(bool isMorale)
{
    if (queue)
        throw std::runtime_error("getQueue: cache already built");

    queue = std::make_unique<Nodes::Unit::Queue>();

    auto tmp = std::vector<battle::Units>{};
    battle.battleGetTurnOrder(tmp, S15::STACK_QUEUE_SIZE, 0);
    for(const auto & units : tmp)
    {
        for(const auto & unit : units)
        {
            if(queue->size() < S15::STACK_QUEUE_SIZE)
                queue->push_back(unit->unitId());
            else
                break;
        }
    }

    // XXX: TODO: FIXME: this must be set to NULLPTR on battle start/end
    const auto * astack = battle.battleActiveUnit();

    // XXX: after morale, battleGetTurnOrder() returns wrong order
    //      (where a non-active stack is first)
    //      The active stack *must* be first-in-queue
    if(isMorale && astack && queue->at(0) != astack->unitId())
    {
        // logAi->debug("Morale triggered -- will rearrange stack queue");
        std::rotate(queue->rbegin(), queue->rbegin() + 1, queue->rend());
        queue->at(0) = astack->unitId();
    }
    else
    {
        // the only scenario where the active stack is not first in queue
        // is at battle end (i.e. no active stack)
        // assert(astack == nullptr || res.at(0) == astack->unitId());
        ASSERT(astack == nullptr || queue->at(0) == astack->unitId(), "queue[0] is not the currently active stack!");
    }
}

const Nodes::Unit * Graph::findUnitByHex(const BattleHex & bh) const
{
    if (!haveUnitsByHex)
        throw std::runtime_error("findUnitByHex: cache not built");

    const auto & it = unitsByHex.find(bh);
    if(it == unitsByHex.end())
        return nullptr;

    return &it->second;
}

void Graph::cacheUnitsByHex()
{
    if(haveUnitsByHex)
        throw std::runtime_error("cacheUnitsByHex: cache already built");

    haveUnitsByHex = true;

    for (const auto & unit : getAll<Nodes::Unit>())
        for(const auto & hex : unit.cstack.getHexes())
            unitsByHex.try_emplace(hex, unit);

    if (unitsByHex.empty())
    {
        // This should only happen on an empty battlefield (draw?)
        // => throw only if there are units still alive
        for (const auto * cstack : battle.battleGetAllStacks(false))
            if (cstack->alive())
                throw std::runtime_error("cacheUnitsByHex: graph contains no units");
    }

    haveUnitsByHex = true;
}

void Graph::addGlobalNode(
    S15::CombatResult result,
    int round,
    int valueStart,
    int hpStart,
    int value,
    int hp
)
{
    const auto side = acstack ? acstack->unitSide() : battle.battleGetMySide();
    add(std::make_shared<Nodes::Global>(
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

void Graph::addPlayerNode(
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
    add(std::make_shared<Nodes::Player>(
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

} // namespace
