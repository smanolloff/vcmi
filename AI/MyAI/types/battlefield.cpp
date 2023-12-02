#include "battlefield.h"
#include "battle/AccessibilityInfo.h"
#include "battle/ReachabilityInfo.h"
#include "mytypes.h"
#include "types/action_enums.h"
#include <stdexcept>

namespace MMAI {
    using NValue = MMAIExport::NValue;

    const Hexes Battlefield::initHexes(const CBattleCallback * cb) {
        auto res = Hexes{};
        int i = 0;

        for(int y=0; y < GameConstants::BFIELD_HEIGHT; y++) {
            // 0 and 16 are unreachable "side" hexes => exclude
            for(int x=1; x < GameConstants::BFIELD_WIDTH - 1; x++) {
                res[i++] = Hex(cb, astack, ainfo, rinfo, BattleHex(x, y));
            }
        }

        ASSERT(i == res.size(), "unexpected i: " + std::to_string(i));

        // Mark own pos as valid for shooting at all enemy troops
        if (astack->canShoot() && !cb->battleIsUnitBlocked(astack)) {
            auto &hex = res.at(Hex::calcId(astack->getPosition()));

            for (auto &estack : cb->battleGetStacks(CBattleCallback::ONLY_ENEMY)) {
                hex.hexactmask[estack->unitSlot()] = true;
            }
        }

        auto canshoot = (astack->canShoot() && !cb->battleIsUnitBlocked(astack));
        auto &astackhex = res.at(Hex::calcId(astack->getPosition()));

        for (auto &estack : cb->battleGetStacks(CBattleCallback::ONLY_ENEMY)) {
            // If we can shoot, that enemy stack is attackable from our own pos
            // TODO: if we can shoot, maybe we can STILL melee attack
            //       reachable hexes? If so, "continue" must be removed.
            if (canshoot) {
                astackhex.hexactmask[estack->unitSlot()] = true;
                continue;
            }

            // For each hex surrounding that stack, if we [can move to|stand on] it
            // => we can attack that stack
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
                if (mask[EI(HexAction::MOVE)] || bh.hex == astack->getPosition()) {
                    mask[estack->unitSlot()] = true;
                }
            }
        }

        return res;
    };

    const Stacks Battlefield::initStacks(const CBattleCallback * cb) {
        auto res = Stacks{};
        auto allstacks = cb->battleGetStacks();

        // summoned units?
        ASSERT(allstacks.size() <= 14, "unexpected allstacks size: " + std::to_string(allstacks.size()));

        for (auto stack : allstacks) {
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

            auto i = (stack->unitSide() == astack->unitSide()) ? slot : slot + 7;
            res[i] = std::make_unique<Stack>(stack, attrs);
        }

        return res;
    }

    const CStack * Battlefield::getEnemyStackBySlot(int slot) {
        return stacks[slot+7]->stack;
    };

    const MMAIExport::State Battlefield::exportState() {
        auto res = MMAIExport::State{};
        int i = 0;

        for (auto &hex : hexes) {
            res[i++] = NValue(EI(hex.state), 0, EI(HexState::count) - 1);
        }

        for (auto &stack : stacks) {
            for (int j=0; j<EI(StackAttr::count); j++) {
                auto &a = stack->attrs[j];

                switch(StackAttr(i)) {
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
                case StackAttr::count:
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
