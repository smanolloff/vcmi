#include "BAI/v15/graph/nodes/unit.h"
#include "CStack.h"
#include "battle/AccessibilityInfo.h"
#include "battle/BattleHex.h"
#include "battle/CObstacleInstance.h"
#include "battle/CPlayerBattleCallback.h"

namespace MMAI::BAI::V15
{

struct FastBFS
{
    static constexpr uint16_t INFINITE_DIST = std::numeric_limits<uint16_t>::max();
    using UnitPtr = std::shared_ptr<const Graph::Nodes::Unit>;
    using TDistances = std::array<uint16_t, GameConstants::BFIELD_SIZE>;

    explicit FastBFS(
        const CPlayerBattleCallback &battle,
        const AccessibilityInfo & accessibility)
    : accessL1(BuildAccessibilityMask(accessibility, BattleSide::LEFT_SIDE, false))
    , accessL2(BuildAccessibilityMask(accessibility, BattleSide::LEFT_SIDE, true))
    , accessR1(BuildAccessibilityMask(accessibility, BattleSide::RIGHT_SIDE, false))
    , accessR2(BuildAccessibilityMask(accessibility, BattleSide::RIGHT_SIDE, true))
    , stopL1(BuildStopMask(battle, BattleSide::LEFT_SIDE, false))
    , stopL2(BuildStopMask(battle, BattleSide::LEFT_SIDE, true))
    , stopR1(BuildStopMask(battle, BattleSide::RIGHT_SIDE, false))
    , stopR2(BuildStopMask(battle, BattleSide::RIGHT_SIDE, true))
    {}

    TDistances run(
        const BattleHex oldpos,
        const BattleHex newpos,
        const BattleSide side,
        bool isFlying,
        bool isWide,
        int speed
    ) const
    {
        assert(oldpos.isValid());
        assert(newpos.isValid());
        return isFlying
            ? calcAirReachability(oldpos, newpos, side, isWide)
            : calcLandReachability(oldpos, newpos, side, isWide, speed);
    }

private:
    using Mask = std::array<bool, GameConstants::BFIELD_SIZE>;
    using TPredecessors = std::array<BattleHex, GameConstants::BFIELD_SIZE>;

    static Mask BuildAccessibilityMask(
        const AccessibilityInfo & accessibility,
        BattleSide side,
        bool wide)
    {
        auto mask = Mask{};
        // PROBLEM:
        // .accessible can't really be cached:
        //   . . . . .
        //  1 - . . .
        //   . . . . .
        // hex in front of the "1" stack will be marked unavailable
        // for all wide left units.
        // Calculating reachability for another hypothetical position of 1
        // would still leave old "-" hex inaccessible for wide creatures...
        for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
            mask[i] = accessibility.accessible(BattleHex(i), wide, side);
        return mask;
    }

    static Mask BuildStopMask(
        const CPlayerBattleCallback & battle,
        BattleSide side,
        bool wide)
    {
        auto mask = Mask{};
        mask.fill(false);

        const auto gateState = battle.battleGetGateState();
        auto stoppers = BattleHexArray();

        // We can only "see" the obstacles visible from our own perspective
        // (regardless which side we are evaluating stoppers for)
        // XXX: stolen from CBattleInfoCallback::getStoppers
        // but modified to fix a bug with gate hex (see notes/moats.txt)
        for(const auto & o : battle.battleGetAllObstacles(battle.battleGetMySide()))
        {
            if(!battle.battleIsObstacleVisibleForSide(*o, side))
                continue;

            for(const auto & hex : o->getStoppingTile())
            {
                if(hex == BattleHex::GATE_BRIDGE
                    // we only care for wide moat where bridge can cover it (aka. fortress)
                    && o->obstacleType == CObstacleInstance::MOAT

                    // If bridge is not blocked, we only care about attackers
                    // (for defenders the bridge would lower and cover the moat)
                    // However, if blocked, then defenders are also affected
                    // This is the same as:
                    && (gateState == EGateState::OPENED
                        || gateState == EGateState::DESTROYED
                        || (gateState == EGateState::CLOSED && side == BattleSide::DEFENDER)))
                {
                    // drawbridge is open (or will open), negating the "stop" nature of the hex
                    continue;
                }

                stoppers.insert(hex);
            }
        }


        for(int i = 0; i < GameConstants::BFIELD_SIZE; ++i)
        {
            BattleHex tile(i);

            if(!tile.isValid())
                continue;

            const BattleHex first = tile;

            if(stoppers.contains(first))
            {
                mask[i] = true;
                continue;
            }

            if(!wide)
                continue;

            const BattleHex second = CStack::occupiedHex(tile, true, side);

            if(second.isValid() && stoppers.contains(second))
            {
                mask[i] = true;
                continue;
            }
        }

        return mask;
    }

    TDistances calcAirReachability(
        const BattleHex & oldpos, // actual stack position now
        const BattleHex & newpos, // hypothetical stack position to calculate reachability from
        BattleSide side,
        bool wide) const
    {
        auto distances = TDistances{};
        distances.fill(INFINITE_DIST);

        const auto & accessible = accessMask(side, wide, oldpos, newpos);

        for(int i = 0; i < GameConstants::BFIELD_SIZE; i++)
        {
            if(!accessible[i])
                continue;

            distances[i] = BattleHex::getDistance(newpos, BattleHex(i));
        }

        return distances;
    }

    TDistances calcLandReachability(
        const BattleHex & oldpos, // actual stack position now
        const BattleHex & newpos, // hypothetical stack position to calculate reachability from
        BattleSide side,
        bool wide,
        int speed) const
    {
        auto distances = TDistances{};
        auto predecessors = TPredecessors{};
        distances.fill(INFINITE_DIST);
        predecessors.fill(BattleHex::INVALID);

        const auto & accessible = accessMask(side, wide, oldpos, newpos);
        const auto & stoppers = stopMask(side, wide);

        // Start may be occupied by the moving unit itself, so do not require accessible[startIndex].
        distances[newpos.toInt()] = 0;

        std::array<BattleHex, GameConstants::BFIELD_SIZE> queue;
        size_t head = 0;
        size_t tail = 0;

        assert(tail < queue.size());
        queue[tail++] = newpos;

        while(head != tail)
        {
            const BattleHex curHex = queue[head++];
            const int curIndex = curHex.toInt();
            const uint16_t curDist = distances[curIndex];

            if(curDist >= speed)
                continue;

            // Walking stack cannot step past obstacles.
            // This preserves the old behavior: the obstacle tile may be reached,
            // but BFS does not expand from it.
            if(stoppers[curIndex])
                continue;

            const uint16_t nextDist = static_cast<uint16_t>(curDist + 1);

            for(const BattleHex & neighbour : curHex.getNeighbouringTiles())
            {
                const int ni = neighbour.toInt();

                if(!accessible[ni])
                    continue;

                if(nextDist >= distances[ni])
                    continue;

                distances[ni] = nextDist;
                predecessors[ni] = curHex;
                queue[tail++] = neighbour;
            }
        }

        return distances;
    }

    // Can the unit stand on this hex?
    const Mask accessL1{};  // accessibility for single-wide left units
    const Mask accessL2{};  // accessibility for double-wide left units
    const Mask accessR1{};  // accessibility for single-wide right units
    const Mask accessR2{};  // accessibility for double-wide right units

    // Does this hex stop the movement of the unit? (e.g. moat)
    const Mask stopL1{};  // stopping hexes for single-wide left units
    const Mask stopL2{};  // stopping hexes for double-wide left units
    const Mask stopR1{};  // stopping hexes for single-wide right units
    const Mask stopR2{};  // stopping hexes for double-wide right units

    const Mask & _accessMask(BattleSide side, bool wide) const
    {
        if(wide)
            return side == BattleSide::ATTACKER ? accessL2 : accessR2;
        return side == BattleSide::ATTACKER ? accessL1 : accessR1;
    }

    Mask accessMask(
        BattleSide side,
        bool wide,
        const BattleHex & oldpos,
        const BattleHex & newpos) const
    {
        auto mask = _accessMask(side, wide);

        // reachability should be calculated from the POV of a hypothetical
        // stack position which must have been accessible in the first place
        // no need to mark newpos as accessible (it already is)
        mask[oldpos.toInt()] = true;
        assert(mask[newpos.toInt()]);

        if (wide)
        {
            const auto occupiedHex = CStack::occupiedHex(oldpos, wide, side);

            // . . . . . . .     . . . . . .
            //  . . . . . .     . . . . . .
            // . . . . . . .     . - R R ◼ .
            //  . ◼ L L - .     . . . . . .
            // . . . . . . .     . . . . . .
            //
            // In accessL2, LL as well as the hex "-" in front are all unaccessible.
            // In accessR2, RR and the "-" in front of it are unaccessible.
            //
            // However, we are now calculating reachability for a new hypothetical
            // position of LL (or RR). hence we must set as available the LL hexes,
            // but also the hex front as well.
            // Caveat1: if that hex in front was unaccessible because of something else
            //          (e.g. real obstacle), then it must remain unaccessible
            //          => set it to whatever value it has in accessL1.
            // Caveat2: if the hex "behind" the L stack was an inaccessible (e.g. obstacle),
            //          then we must *not* mark both LL hexes as available: only the primary.
            //

            // Handle caveat 1
            const auto hexInFront = side == BattleSide::LEFT_SIDE
                ? oldpos.cloneInDirection(BattleHex::EDir::RIGHT)
                : oldpos.cloneInDirection(BattleHex::EDir::LEFT);

            mask[hexInFront.toInt()] = _accessMask(side, false)[hexInFront.toInt()];

            // Handle caveat 2
            const auto hexBehind = side == BattleSide::LEFT_SIDE
                ? occupiedHex.cloneInDirection(BattleHex::EDir::LEFT)
                : occupiedHex.cloneInDirection(BattleHex::EDir::RIGHT);

            mask[occupiedHex.toInt()] = _accessMask(side, false)[hexBehind.toInt()];
        }

        return mask;
    }

    const Mask & stopMask(BattleSide side, bool wide) const
    {
        if(wide)
            return side == BattleSide::ATTACKER ? stopL2 : stopR2;
        return side == BattleSide::ATTACKER ? stopL1 : stopR1;
    }
};

}
