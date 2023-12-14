#include "types/battlefield.h"
#include "CCallback.h"
#include "battle/AccessibilityInfo.h"
#include "battle/Unit.h"
#include "types/hexaction.h"
#include "types/stack.h"
#include <stdexcept>

namespace MMAI {
    using NValue = Export::NValue;

    // static
    void Battlefield::initHex(
        Hex &hex,
        BattleHex bh,
        const CStack* astack,
        const AccessibilityInfo &ainfo,
        const ReachabilityInfo &rinfo
    ) {
        hex.bhex = bh;
        hex.id = Hex::calcId(bh);
        hex.hexactmask.fill(false);

        switch(ainfo.at(bh.hex)) {
        case EAccessibility::ACCESSIBLE:
            if (rinfo.distances[bh] <= astack->speed()) {
                hex.state = HexState::FREE_REACHABLE;
                hex.hexactmask.at(EI(HexAction::MOVE)) = true;
            } else {
                hex.state = HexState::FREE_UNREACHABLE;
            }

            break;
        case EAccessibility::OBSTACLE:
            hex.state = HexState::OBSTACLE;
            break;
        case EAccessibility::ALIVE_STACK: {
            hex.state = HexState::OCCUPIED;
            break;
        }
        // XXX: unhandled hex states
        // case EAccessibility::DESTRUCTIBLE_WALL:
        // case EAccessibility::GATE:
        // case EAccessibility::UNAVAILABLE:
        // case EAccessibility::SIDE_COLUMN:
        default:
          throw std::runtime_error(
            "Unexpected hex accessibility for hex "+ std::to_string(bh.hex) + ": "
              + std::to_string(static_cast<int>(ainfo.at(bh.hex)))
          );
        }
    }

    // static
    // result is a vector<UnitID>
    Queue Battlefield::getQueue(CBattleCallback* cb) {
        auto res = Queue{};

        auto tmp = std::vector<battle::Units>{};
        cb->battleGetTurnOrder(tmp, QSIZE, 0);
        for (auto &units : tmp)
            for (auto &unit : units)
                res.insert(res.end(), unit->unitId());

        return res;
    }

    /**
     * Handle attacking a hex from "surrounding" hexes
     *
     * Tricky part is if attacking with a 2-hex stack
     * (comments in HexAction enum for more info)
     */
    // static
    void Battlefield::updateMaskForAttackCases(
        const CStack * astack,
        const Hexes &hexes,
        Hex &hex
    ) {
        auto maybeupdate = [&astack, &hexes, &hex](const HexAction &hexaction, const BattleHex &nbh) {
            if (!nbh.isAvailable())
                return;

            auto &nhex = hexes.at(Hex::calcId(nbh));

            // We can melee attack from nbh (neughbouring hex) if:
            // A. we can *move* to it
            // B. we already stand there
            if (nhex.hexactmask.at(EI(HexAction::MOVE)) || astack->getPosition().hex == nbh.hex) {
                // TODO: remove (expensive check)
                ASSERT(hex.stack->cstack->isMeleeAttackPossible(astack, hex.stack->cstack, nbh), "expected possible melee attack");
                hex.hexactmask.at(EI(hexaction)) = true;
            }
        };

        auto side = BattleSide::Type(astack->unitSide());
        auto &[special, tspecial, bspecial] = EDIR_SPECIALS.at(side);

        for (const auto& [edir, hexaction] : EDIR_TO_MELEE) {
            auto nbh = hex.bhex.cloneInDirection(edir, false); // neighbouring bhex
            if (astack->doubleWide()) {
                if (edir == special) {
                    // L/R => transpose
                    maybeupdate(hexaction, nbh.cloneInDirection(special, false));
                } else if (edir == tspecial) {
                    // TL/TR => transpose + add T
                    maybeupdate(hexaction, nbh.cloneInDirection(special, false));
                    maybeupdate(HexAction::MELEE_T, nbh);
                } else if (edir == bspecial) {
                    // BL/BR => transpose + add B
                    maybeupdate(hexaction, nbh.cloneInDirection(special, false));
                    maybeupdate(HexAction::MELEE_B, nbh);
                } else {
                    maybeupdate(hexaction, nbh);
                }
            } else {
                maybeupdate(hexaction, nbh);
            }
        }
    }

    /**
     * Handle moving to the "back" hex:
     * (xxooxx -> xooxx)
     *
     * If this is the active stack
     * AND this hex is the "occupied" (ie. the "back") hex of this (2-hex) stack
     * AND it is is accessible by a 2-hex stack
     * => can move to this hex.
     */
    // static
    void Battlefield::updateMaskForSpecialMoveCase(
        Hex &hex,
        const CStack* astack,
        const AccessibilityInfo &ainfo
    ) {
        if (astack->unitId() == hex.stack->cstack->unitId() &&
            astack->occupiedHex().hex == hex.bhex.hex &&
            astack->speed() > 0 &&
            ainfo.accessible(hex.bhex, true, astack->unitSide())
        ) {
            hex.hexactmask.at(EI(HexAction::MOVE)) = true;
        }
    }

    /**
     * Handle moving to "self" (aka. defend)
     */
    // static
    void Battlefield::updateMaskForSpecialDefendCase(Hex &hex, const CStack* astack) {
        if (astack->unitId() != hex.stack->cstack->unitId())
            return;

        if (astack->getPosition().hex == hex.bhex.hex)
            hex.hexactmask.at(EI(HexAction::MOVE)) = true;
    }

    // static
    Hexes Battlefield::initHexes(CBattleCallback* cb, const CStack* astack) {
        auto res = Hexes{};
        auto ainfo = cb->getAccesibility();
        auto rinfo = cb->getReachability(astack);

        int i = 0;

        // state: set OBSTACLE/OCCUPIED/FREE_*
        // hexactmask: set MOVE
        for(int y=0; y < GameConstants::BFIELD_HEIGHT; y++) {
            // 0 and 16 are unreachable "side" hexes => exclude
            for(int x=1; x < GameConstants::BFIELD_WIDTH - 1; x++) {
                initHex(res.at(i++), BattleHex(x, y), astack, ainfo, rinfo);
            }
        }

        ASSERT(i == res.size(), "unexpected i: " + std::to_string(i));

        auto canshoot = cb->battleCanShoot(astack);
        auto queue = getQueue(cb);

        ASSERT(queue.size() == QSIZE, "queue size: " + std::to_string(queue.size()));

        // stack: set stack
        // hexactmask:
        // * set SHOOT, MELEE_*
        // * set MOVE special case: back hex for 2-hex active stack
        for (auto &hex : res) {
            switch (hex.state) {
            break; case HexState::INVALID:
            break; case HexState::OBSTACLE:
                // TODO: action mask for obstacles ("remove obstacle" spell)
            break; case HexState::FREE_UNREACHABLE:
                   case HexState::FREE_REACHABLE:
                // TODO: action mask for free hexes (ie. spells)
            break; case HexState::OCCUPIED: {
                auto cstack = cb->battleGetStackByPos(hex.bhex, true);
                ASSERT(cstack, "OCCUPIED but no cstack");
                hex.stack = std::make_unique<Stack>(initStack(cb, queue, cstack));

                updateMaskForSpecialMoveCase(hex, astack, ainfo);
                updateMaskForSpecialDefendCase(hex, astack);

                if (hex.stack->cstack->unitSide() == cb->battleGetMySide())
                    // TODO: action mask for friendly stacks (ie. spells)
                    continue;

                if (canshoot)
                    hex.hexactmask.at(EI(HexAction::SHOOT)) = true;

                updateMaskForAttackCases(astack, res, hex); // XXX: must come last
            }
            break; default:
                throw std::runtime_error("Unexpected hex state: " + std::to_string(EI(hex.state)));
                break;
            }
        }

        return res;
    };

    // static
    // XXX: queue is a flattened battleGetTurnOrder, with *prepended* astack
    Stack Battlefield::initStack(CBattleCallback* cb, const Queue &q, const CStack* cstack) {
        auto attrs = StackAttrs{};

        auto it = std::find(q.begin(), q.end(), cstack->unitId());
        auto qpos = (it == q.end()) ? QSIZE-1 : it - q.begin();

        for (int i=0; i<EI(StackAttr::count); i++) {
            int a;

            switch(StackAttr(i)) {
            break; case StackAttr::Quantity: a = cstack->getCount();
            break; case StackAttr::Attack: a = cstack->getAttack(false);
            break; case StackAttr::Defense: a = cstack->getDefense(false);
            break; case StackAttr::Shots: a = cstack->shots.available();
            break; case StackAttr::DmgMin: a = cstack->getMinDamage(false);
            break; case StackAttr::DmgMax: a = cstack->getMaxDamage(false);
            break; case StackAttr::HP: a = cstack->getMaxHealth();
            break; case StackAttr::HPLeft: a = cstack->getFirstHPleft();
            break; case StackAttr::Speed: a = cstack->speed();
            break; case StackAttr::Waited: a = cstack->waitedThisTurn;
            break; case StackAttr::QueuePos:
                a = qpos;
                ASSERT(a >= 0 && a < QSIZE, "unexpected qpos: " + std::to_string(a));
            break; case StackAttr::Side: a = cstack->unitSide();
            break; case StackAttr::Slot:
                a = cstack->unitSlot();
                ASSERT(a >= 0 && a < 7, "unexpected slot: " + std::to_string(a));
            break; case StackAttr::CreatureType:
                a = cstack->creatureId();
            break; default:
                throw std::runtime_error("Unexpected StackAttr: " + std::to_string(i));
            }

            attrs.at(i) = a;
        }

        return Stack(cstack, attrs);
    }

    const Export::State Battlefield::exportState() {
        auto res = Export::State{};
        int i = 0;

        for (auto &hex : hexes) {
            res.at(i++) = NValue(EI(hex.state), 0, EI(HexState::count) - 1);
            auto &stack = hex.stack;

            for (int j=0; j<EI(StackAttr::count); j++) {
                int max;

                switch (StackAttr(j)) {
                break; case StackAttr::Quantity: max = 5000;
                break; case StackAttr::Attack:   max = 100;
                break; case StackAttr::Defense:  max = 100;
                break; case StackAttr::Shots:    max = 32;
                break; case StackAttr::DmgMin:   max = 100;
                break; case StackAttr::DmgMax:   max = 100;
                break; case StackAttr::HP:       max = 1500;
                break; case StackAttr::HPLeft:   max = 1500;
                break; case StackAttr::Speed:    max = 30;
                break; case StackAttr::Waited:   max = 1;
                break; case StackAttr::QueuePos: max = QSIZE-1;
                break; case StackAttr::Side:     max = 1;
                break; case StackAttr::Slot:     max = 6;
                break; case StackAttr::CreatureType: max = 150;
                break; default:
                    throw std::runtime_error("Unexpected StackAttr: " + std::to_string(EI(j)));
                }

                res.at(i++) = NValue(stack->attrs.at(j), ATTR_NA, max);
            }
        }

        ASSERT(i == Export::STATE_SIZE, "unexpected i: " + std::to_string(i));

        return res;
    }

    const Export::ActMask Battlefield::exportActMask() {
        auto res = Export::ActMask{};
        int i = 0;

        for (int j=0; j<EI(NonHexAction::count); j++) {
            switch (NonHexAction(j)) {
            break; case NonHexAction::RETREAT: res.at(i++) = true;
            break; case NonHexAction::WAIT: res.at(i++) = !astack->waitedThisTurn;
            break; default:
                throw std::runtime_error("Unexpected NonHexAction: " + std::to_string(j));
            }
        }

        for (auto &hex : hexes) {
            for (auto &m : hex.hexactmask) {
                res.at(i++) = m;
            }
        }

        // static_assert in action_enums.h makes this redundant?
        ASSERT(i == Export::N_ACTIONS, "unexpected i: " + std::to_string(i));

        return res;
    }

    /**
     * Update state between turns:
     * - used if expecting a battle end (for terminal state/render)
     * - no "reachable" hexes (off-turn means no active stack)
     */
    void Battlefield::offTurnUpdate(CBattleCallback* cb) {
        auto ainfo = cb->getAccesibility();
        auto queue = getQueue(cb);

        for (auto &hex : hexes) {
            switch(ainfo.at(hex.bhex.hex)) {
            break; case EAccessibility::ACCESSIBLE:
                hex.state = HexState::FREE_UNREACHABLE;
            break; case EAccessibility::OBSTACLE:
                hex.state = HexState::OBSTACLE;
            break; case EAccessibility::ALIVE_STACK: {
                hex.state = HexState::OCCUPIED;

                auto cstack = cb->battleGetStackByPos(hex.bhex, true);
                ASSERT(cstack, "null cstack on ALIVE_STACK hex");
                hex.stack = std::make_unique<Stack>(initStack(cb, queue, cstack));

                // nothing else needed here: no active stack
                // => no special move case
                // => no action mask
            }
            break; default:
              throw std::runtime_error(
                "Unexpected hex accessibility for hex "+ std::to_string(hex.bhex.hex) + ": "
                  + std::to_string(static_cast<int>(ainfo.at(hex.bhex.hex)))
              );
            }
        }
    }
}
