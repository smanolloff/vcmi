#include "CStack.h"
#include "types/battlefield.h"

namespace MMAI {
    using NValue = MMAIExport::NValue;

    // static
    void Battlefield::initHex(
        Hex &hex,
        BattleHex bh,
        CBattleCallback* cb,
        const CStack* astack,
        const AccessibilityInfo &ainfo,
        const ReachabilityInfo &rinfo
    ) {
        hex.bhex = bh;
        hex.id = Hex::calcId(bh);

        switch(ainfo[bh.hex]) {
        case LIB_CLIENT::EAccessibility::ACCESSIBLE:
            if (rinfo.distances[bh] <= astack->speed()) {
                hex.state = HexState::FREE_REACHABLE;
                hex.hexactmask[EI(HexAction::MOVE)] = true;
            } else {
                hex.state = HexState::FREE_UNREACHABLE;
            }

            break;
        case LIB_CLIENT::EAccessibility::OBSTACLE:
            hex.state = HexState::OBSTACLE;
            break;
        case LIB_CLIENT::EAccessibility::ALIVE_STACK:
            hex.stack = cb->battleGetStackByPos(bh, true);
            hex.state = (hex.stack->unitSide() == cb->battleGetMySide())
                ? HexState(EI(HexState::FRIENDLY_STACK_0) + hex.stack->unitSlot())
                : HexState(EI(HexState::ENEMY_STACK_0) + hex.stack->unitSlot());

            // Handle moving to the "back" hex of two-hex active stacks:
            // (xxooxx -> xooxx)
            //
            // If this is the active stack
            // AND this hex is the "occupied" (ie. the "back") hex of this (2-hex) stack
            // AND it is is accessible by a 2-hex stack
            // => can move to this hex.
            if (
              astack->unitId() == hex.stack->unitId() &&
              astack->occupiedHex() == bh &&
              ainfo.accessible(bh, true, astack->unitSide())
            ) {
              hex.hexactmask[EI(HexAction::MOVE)] = true;
            }
            break;
        // XXX: unhandled hex states
        // case LIB_CLIENT::EAccessibility::DESTRUCTIBLE_WALL:
        // case LIB_CLIENT::EAccessibility::GATE:
        // case LIB_CLIENT::EAccessibility::UNAVAILABLE:
        // case LIB_CLIENT::EAccessibility::SIDE_COLUMN:
        default:
          throw std::runtime_error(
            "Unexpected hex accessibility for hex "+ std::to_string(bh.hex) + ": "
              + std::to_string(static_cast<int>(ainfo[bh.hex]))
          );
        }
    }

    // static
    Hexes Battlefield::initHexes(CBattleCallback* cb, const CStack* astack) {
        // TEST
        auto ainfo = cb->getAccesibility();
        auto rinfo = cb->getReachability(astack);
        auto res = Hexes{};

        int i = 0;

        for(int y=0; y < GameConstants::BFIELD_HEIGHT; y++) {
            // 0 and 16 are unreachable "side" hexes => exclude
            for(int x=1; x < GameConstants::BFIELD_WIDTH - 1; x++) {
                initHex(res[i++], BattleHex(x, y), cb, astack, ainfo, rinfo);
            }
        }

        ASSERT(i == res.size(), "unexpected i: " + std::to_string(i));

        auto canshoot = cb->battleCanShoot(astack);
        auto &astackhex = res.at(Hex::calcId(astack->getPosition()));

        for (auto &estack : cb->battleGetStacks(CBattleCallback::ONLY_ENEMY)) {
            // If we can shoot, that enemy stack is attackable from our own pos
            // TODO: if we can shoot, maybe we can STILL melee attack
            //       reachable hexes? If so, "continue" must be removed.
            if (canshoot) {
                astackhex.hexactmask[estack->unitSlot()] = true;
                continue;
            }

            auto apos = astack->getPosition();

            // For each hex surrounding that stack, if we [can move to|stand on] it
            // => we can melee that stack
            for (auto &bh : estack->getSurroundingHexes()) {
                auto id = Hex::calcId(bh);
                auto &hex = res[id];
                auto &mask = hex.hexactmask;

                ASSERT(hex.bhex.isValid(), "uninitialized hex at " + std::to_string(id));

                // We can move+attack from that (neugbhouring) hex if:
                // A. we can *move* to it
                // B. if we already stand there.
                //
                // HexAction::MOVE covers A. and (partially) B.
                // (partially = standing with our "back" hex there)
                if (mask[EI(HexAction::MOVE)] || bh.hex == apos) {
                    mask[estack->unitSlot()] = true;
                }
            }

            if (!astack->doubleWide())
                continue;

            // If we are "A" (2-hex stack), we can attack from distant hexes:
            //
            //  o o o o   o o o o    o A A o
            // o x o o   o x A A o  o x o o o
            //  o A A o   o o o o    o o o o
            //      ^ from here we can attack
            //
            // (using a modified version of Unit::getSurroundingHexes())
            //

            auto directions = std::array<BattleHex::EDir, 3>{
                BattleHex::EDir::TOP_RIGHT,
                BattleHex::EDir::RIGHT,
                BattleHex::EDir::BOTTOM_RIGHT
            };

            // From a defender's perspective, it's vertically mirrored
            if(astack->unitSide() == BattleSide::DEFENDER) {
                directions[0] = BattleHex::EDir::TOP_LEFT;
                directions[1] = BattleHex::EDir::LEFT;
                directions[2] = BattleHex::EDir::BOTTOM_LEFT;
            }

            auto epos = estack->getPosition();

            for (auto &dir : directions) {
                auto tmpbh = epos.cloneInDirection(dir, false);
                if (!tmpbh.isAvailable()) continue;

                tmpbh = tmpbh.cloneInDirection(directions[1]);
                if (!tmpbh.isAvailable()) continue;

                auto &hex = res[Hex::calcId(tmpbh)];
                auto &mask = hex.hexactmask;

                if (mask[EI(HexAction::MOVE)] || tmpbh.hex == apos) {
                    mask[estack->unitSlot()] = true;
                }
            }
        }

        return res;
    };

    // static
    Stacks Battlefield::initStacks(const CBattleCallback * cb) {
        auto res = Stacks{};
        auto allstacks = cb->battleGetStacks();

        // summoned units?
        ASSERT(allstacks.size() <= 14, "unexpected allstacks size: " + std::to_string(allstacks.size()));

        for (auto &stack : allstacks) {
            auto attrs = StackAttrs{};

            attrs[EI(StackAttr::Quantity)] = stack->getCount();
            attrs[EI(StackAttr::Attack)] = stack->getAttack(false);
            attrs[EI(StackAttr::Defense)] = stack->getDefense(false);
            attrs[EI(StackAttr::Shots)] = stack->shots.available();
            attrs[EI(StackAttr::DmgMin)] = stack->getMinDamage(false);
            attrs[EI(StackAttr::DmgMax)] = stack->getMaxDamage(false);
            attrs[EI(StackAttr::HP)] = stack->getMaxHealth();
            attrs[EI(StackAttr::HPLeft)] = stack->getFirstHPleft();
            attrs[EI(StackAttr::Speed)] = stack->speed();
            attrs[EI(StackAttr::Waited)] = stack->waitedThisTurn;
            static_assert(EI(StackAttr::count) == 10);

            // summoned slots?
            int slot = stack->unitSlot();
            ASSERT(slot >= 0 && slot < 7, "unexpected slot: " + std::to_string(slot));

            auto i = (stack->unitSide() == cb->battleGetMySide()) ? slot : slot + 7;
            res[i] = std::make_unique<Stack>(stack, attrs);
        }

        return res;
    }

    const CStack * Battlefield::getEnemyStackBySlot(int slot) {
        auto res = stacks[slot+7].get();
        return res ? res->stack : nullptr;
    };

    const MMAIExport::State Battlefield::exportState() {
        auto res = MMAIExport::State{};
        int i = 0;

        for (auto &hex : hexes) {
            res[i++] = NValue(EI(hex.state), 0, EI(HexState::count) - 1);
        }

        for (auto &stack : stacks) {
            if (!stack) {
                i += EI(StackAttr::count);
                continue;
            }

            for (int j=0; j<EI(StackAttr::count); j++) {
                auto &a = stack->attrs[j];

                switch(StackAttr(j)) {
                case StackAttr::Quantity:   res[i++] = NValue(a, 0, 5000); break;
                case StackAttr::Attack:     res[i++] = NValue(a, 0, 100); break;
                case StackAttr::Defense:    res[i++] = NValue(a, 0, 100); break;
                case StackAttr::Shots:      res[i++] = NValue(a, 0, 24); break;
                case StackAttr::DmgMin:     res[i++] = NValue(a, 0, 100); break;
                case StackAttr::DmgMax:     res[i++] = NValue(a, 0, 100); break;
                case StackAttr::HP:         res[i++] = NValue(a, 0, 1500); break;
                case StackAttr::HPLeft:     res[i++] = NValue(a, 0, 1500); break;
                case StackAttr::Speed:      res[i++] = NValue(a, 0, 30); break;
                case StackAttr::Waited:     res[i++] = NValue(a, 0, 1); break;
                default:
                    throw std::runtime_error("Unexpected StackAttr");
                }
            }
        }

        // active stack
        res[i++] = NValue(astack->unitSlot(), 0, 6);

        // static_assert in battlefield.h makes this redundant?
        ASSERT(i == MMAIExport::STATE_SIZE, "unexpected i: " + std::to_string(i));

        return res;
    }

    const MMAIExport::ActMask Battlefield::exportActMask() {
        auto res = MMAIExport::ActMask{};
        int i = 0;

        for (int j=0; j<EI(NonHexAction::count); j++) {
            switch (NonHexAction(j)) {
            case NonHexAction::RETREAT: res[i++] = true; break;
            case NonHexAction::DEFEND: res[i++] = true; break;
            case NonHexAction::WAIT: res[i++] = !astack->waitedThisTurn; break;
            default:
                throw std::runtime_error("Unexpected NonHexAction");
            }
        }

        for (auto &hex : hexes) {
            for (auto &m : hex.hexactmask) {
                res[i++] = m;
            }
        }

        // static_assert in action_enums.h makes this redundant?
        ASSERT(i == MMAIExport::N_ACTIONS, "unexpected i: " + std::to_string(i));

        return res;
    }
}
