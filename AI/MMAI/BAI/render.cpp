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

#include "BAI.h"
#include "battle/BattleHex.h"
#include "common.h"
#include "export.h"
#include "types/battlefield.h"
#include "types/hexaction.h"
#include "types/stack.h"
#include <stdexcept>

namespace MMAI {
    // static
    // TODO: fix for terminal observation
    //       info after off-turn update is somewhat inconsistent
    //       (have not investigated properly though)
    std::tuple<Hexes, const CStack*> BAI::Reconstruct(
        const Export::Result &r,
        const std::shared_ptr<CBattleCallback> cb
    ) {
        auto state = r.state;
        auto hexes = Hexes();

        const CStack *astack = nullptr;

        // ihex = 0, 1, 2, .. 164
        // ibase = 0, 15, 30, ... 2460
        for (int ihex=0; ihex < BF_SIZE; ihex++) {
            int ibase = ihex * Export::N_HEX_ATTRS;
            auto hex = Hex{};
            int x = ihex%15;
            int y = ihex/15;
            ASSERT(y == state.at(ibase).orig, "hex.y mismatch");
            ASSERT(x == state.at(ibase+1).orig, "hex.x mismatch");
            hex.bhex = BattleHex(x+1, y);
            hex.x = x;
            hex.y = y;
            hex.hexactmask.fill(false);
            hex.state = HexState(state.at(ibase+2).orig);

            if (hex.state == HexState::FREE_REACHABLE)
                hex.hexactmask.at(EI(HexAction::MOVE)) = true;

            const CStack *cstack = nullptr;
            auto attrs = Stack::NAAttrs();

            if (state.at(ibase+3+EI(StackAttr::Quantity)).orig > 0) {
                // there's a stack on this hex
                auto stacks = cb->battleGetStacksIf([=](const CStack * stack) {
                    return stack->unitSide() == state.at(ibase+3+EI(StackAttr::Side)).orig
                        && stack->unitSlot() == state.at(ibase+3+EI(StackAttr::Slot)).orig;
                });

                expect(stacks.size() == 1, "Expected exactly 1 stack, got: %d", stacks.size());
                cstack = stacks.at(0);

                // // XXX: does not work for terminal render
                // if (!cstack->coversPos(hex.bhex)) {
                //     auto x = cstack->getPosition();
                //     throw std::runtime_error("stack on hex, but coversPos() is false");
                // }

                for (int j=0; j<EI(StackAttr::count); j++) {
                    attrs.at(j) = state.at(ibase+3+j).orig;

                    int vreal;

                    switch(StackAttr(j)) {
                    break; case StackAttr::Quantity: vreal = cstack->getCount();
                    break; case StackAttr::Attack: vreal = cstack->getAttack(false);
                    break; case StackAttr::Defense: vreal = cstack->getDefense(false);
                    break; case StackAttr::Shots: vreal = cstack->shots.available();
                    break; case StackAttr::DmgMin: vreal = cstack->getMinDamage(false);
                    break; case StackAttr::DmgMax: vreal = cstack->getMaxDamage(false);
                    break; case StackAttr::HP: vreal = cstack->getMaxHealth();
                    break; case StackAttr::HPLeft: vreal = cstack->getFirstHPleft();
                    break; case StackAttr::Speed: vreal = cstack->speed();
                    break; case StackAttr::Waited: vreal = cstack->waitedThisTurn;
                    break; case StackAttr::QueuePos:
                        vreal = attrs[j];  // not verifying
                        if (attrs[j] == 0) {
                            if (astack) {
                                expect(astack->doubleWide() && astack == cstack, "active stack conflict");
                            } else {
                                astack = cstack;
                            }
                        }
                    break; case StackAttr::RetaliationsLeft:
                        vreal = cstack->counterAttacks.available();
                        if (vreal > 3) vreal = 3;  // prevent errors from unlimited retals
                    break; case StackAttr::Side: vreal = cstack->unitSide();
                    break; case StackAttr::Slot: vreal = cstack->unitSlot();
                    break; case StackAttr::CreatureType: vreal = cstack->creatureId();
                    break; case StackAttr::AIValue: vreal = cstack->creatureId().toCreature()->getAIValue();
                    break; case StackAttr::IsActive:
                        vreal = attrs[j];
                        if (vreal == 1) {
                            expect(attrs[EI(StackAttr::QueuePos)] == 0, "active stack on non-0 queue pos");
                            if (StackAttr::QueuePos < StackAttr::IsActive) {
                                expect(astack == cstack, "active stack should have been set to cstack");
                            }
                        } else {
                            expect(attrs[EI(StackAttr::QueuePos)] != 0, "non-active stack on 0 queue pos");
                        }
                    break; default:
                      throw std::runtime_error("Unexpected StackAttr: " + std::to_string(j));
                    }

                    expect(attrs.at(j) == vreal, "attribute mismatch for %d: %d != %d", j, attrs.at(j), vreal);
                }

                hex.stack = std::make_shared<Stack>(cstack, attrs);
            }
            hexes.at(y).at(x) = hex;
        }


        return std::tuple{hexes, astack};
    }

    void BAI::Verify(const Battlefield &bf, Hexes &hexes, const CStack* astack) {
        for (int y=0; y<BF_YMAX; y++) {
            for (int x=0; x<BF_XMAX; x++) {
                auto hex0 = bf.hexes.at(y).at(x);
                auto hex = hexes.at(y).at(x);

                expect(hex0.bhex == hex.bhex, "mismatch: hex.bhex");
                expect(hex0.y == hex.y, "mismatch: hex.y");
                expect(hex0.x == hex.x, "mismatch: hex.x");
                expect(hex0.state == hex.state, "mismatch: hex.state");

                for (int j=0; j<EI(StackAttr::count); j++) {
                    expect(hex0.stack->attrs.at(j) == hex.stack->attrs.at(j), "mismatch: hex.stack->attrs[%d]", j);
                }
            }
        }

        expect(bf.astack == astack, "mismatch: astack");
    }

    // static
    std::string BAI::Render(
        const Export::Result &r,
        const std::shared_ptr<CBattleCallback> cb,
        const Battlefield &bf,  // verification only
        const Action *action,  // for displaying "last action"
        const std::vector<AttackLog> attackLogs // for displaying log
    ) {
        static_assert(std::tuple_size<Export::State>::value == 165 * (3 + Export::N_STACK_ATTRS + Export::N_CONTEXT_ATTRS));

        auto reconstructed = Reconstruct(r, cb);
        auto hexes = std::get<0>(reconstructed);
        auto astack = std::get<1>(reconstructed);

        Verify(bf, hexes, astack);

        auto state = r.state;

        std::string nocol = "\033[0m";
        std::string redcol = "\033[31m"; // red
        std::string bluecol = "\033[34m"; // blue
        std::string activemod = "\033[107m\033[7m"; // bold+reversed

        std::vector<std::stringstream> rows;

        auto ourcol = redcol;
        auto enemycol = bluecol;
        if (astack->unitSide() == LIB_CLIENT::BattleSide::DEFENDER) {
            ourcol = bluecol;
            enemycol = redcol;
        }


        //
        // 1. Add logs table:
        //
        // #1 attacks #5 for 16 dmg (1 killed)
        // #5 attacks #1 for 4 dmg (0 killed)
        // ...
        //
        for (auto &alog : attackLogs) {
            auto row = std::stringstream();
            auto col1 = ourcol;
            auto col2 = enemycol;

            if (alog.isOurStackAttacked) {
                col1 = enemycol;
                col2 = ourcol;
            }

            if (alog.attslot >= 0)
                row << col1 << "#" << alog.attslot + 1 << nocol;
            else
                row << "\033[7m" << "FX" << nocol;

            row << " attacks ";
            row << col2 << "#" << alog.defslot + 1 << nocol;
            row << " for " << alog.dmg << " dmg";
            row << " (kills: " << alog.units << ", value: " << alog.value << ")";

            rows.push_back(std::move(row));
        }

        //
        // 2. Add errors
        //
        // Error: target hex is unreachable
        // ...

        for (const auto& pair : Export::ERRORS) {
            auto errtuple = pair.second;
            auto errflag = std::get<0>(errtuple);
            auto errname = std::get<1>(errtuple);
            auto errmsg = std::get<2>(errtuple);
            if (r.errmask & errflag)
                rows.emplace_back("[" + errname + "] Error: " + errmsg);
        }

        //
        // 3. Build ASCII table
        //    (+populate aliveStacks var)
        //    NOTE: the contents below look mis-aligned in some editors.
        //          In (my) terminal, it all looks correct though.
        //
        //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
        //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃
        // ¹┨  1 ◌ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 1 ┠¹
        // ²┨ ◌ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠²
        // ³┨  ◌ ○ ○ ○ ○ ○ ○ ◌ ▦ ▦ ◌ ◌ ◌ ◌ ◌ ┠³
        // ⁴┨ ◌ ○ ○ ○ ○ ○ ○ ○ ▦ ▦ ▦ ◌ ◌ ◌ ◌  ┠⁴
        // ⁵┨  2 ◌ ○ ○ ▦ ▦ ◌ ○ ◌ ◌ ◌ ◌ ◌ ◌ 2 ┠⁵
        // ⁶┨ ◌ ○ ○ ○ ▦ ▦ ◌ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁶
        // ⁷┨  3 3 ○ ○ ○ ▦ ◌ ○ ○ ◌ ◌ ▦ ◌ ◌ 3 ┠⁷
        // ⁸┨ ◌ ○ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌  ┠⁸
        // ⁹┨  ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠⁹
        // ⁰┨ ◌ ○ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠⁰
        // ¹┨  4 ◌ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ 4 ┠¹
        //  ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃
        //   ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕
        //

        auto stacks = std::array<std::shared_ptr<Stack>, 14>{};

        auto tablestartrow = rows.size();

        rows.emplace_back() << "  ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕";
        rows.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉", "₀"};

        for (int y=0; y < BF_YMAX; y++) {
            for (int x=0; x < BF_XMAX; x++) {
                auto sym = std::string("?");
                auto &hex = hexes.at(y).at(x);
                auto &ham = hex.hexactmask;

                auto &row = (x == 0)
                    ? (rows.emplace_back() << nummap.at(y%10) << "┨" << (y % 2 == 0 ? " " : ""))
                    : rows.back();

                row << " ";

                // ibase = first state index of the i-th hex
                // eg. for hex 2, ibase=16 where:
                //  state[16] is hexid for hex2
                //  state[17] is hexstate for hex2
                //  state[18] is hex.stack->attr[Qty] for hex2
                //  state[19] is hex.stack->attr[Att] for hex2
                //  ... etc
                auto ibase = (y * BF_XMAX + x) * (3 + EI(StackAttr::count) + Export::N_CONTEXT_ATTRS);
                auto &nvy = state.at(ibase);
                auto &nvx = state.at(ibase+1);
                expect(hex.y == nvy.orig, "hex.y=%d != state[%d]=%d", hex.y, ibase, nvy.orig);
                expect(hex.x == nvx.orig, "hex.x=%d != state[%d]=%d", hex.x, ibase+1, nvx.orig);
                auto &nvstate = state.at(ibase+2);
                expect(hex.state == HexState(nvstate.orig), "hex.state=%d != state[%d]=%d", hex.state, ibase+2, nvstate.orig);

                // true if any hexactionmask is true
                auto anyham = std::any_of(ham.begin(), ham.end(), [](bool val) { return val; });

                // true if any stack attribute for that hex is valid
                auto anyattr = std::any_of(
                    state.begin()+ibase+3,
                    // XXX: asserting only reachablyByFriendly
                    state.begin()+ibase+3+EI(StackAttr::count),
                    [](Export::NValue nv) { return nv.orig != ATTR_NA; }
                );

                // true if any any of the following context attributes is available:
                // * (7) reachableByFriendlyStacks
                // * (7) reachableByEnemyStacks
                // Expect to be available for all hexes:
                auto anycontext_all = std::any_of(
                    state.begin()+ibase+3+EI(StackAttr::count),
                    state.begin()+ibase+3+EI(StackAttr::count) + 14,
                    [](Export::NValue nv) { return nv.orig != 0; }
                );

                // not checkable
                // // true if any any of the following context attributes is available:
                // // * (7) neighbouringFriendlyStacks
                // // * (7) neighbouringEnemyStacks
                // // * (7) potentialEnemyAttackers
                // // Expect to be available for FREE_REACHABLE hexes only:
                // auto anycontext_free_reachable = std::any_of(
                //     state.begin()+ibase+3+EI(StackAttr::count)+14,
                //     state.begin()+ibase+3+EI(StackAttr::count)+14 + 21,
                //     [](Export::NValue nv) { return nv.orig != 0; }
                // );

                switch(hex.state) {
                case HexState::FREE_REACHABLE: {
                    ASSERT(!anyattr, "FREE_REACHABLE but anyattr is true");
                    // this must be always true as at least 1 stack (astack) can reach this hex
                    ASSERT(anycontext_all, "FREE_REACHABLE but anycontext_all is false");
                    // can't check this as there may be no stacks
                    // ASSERT(anycontext_free_reachable, "FREE_REACHABLE but anycontext_free_reachable is false");

                    ASSERT(ham.at(EI(HexAction::MOVE)), "FREE_REACHABLE but mask[MOVE] is false");
                    sym = "○";

                    for (int n=0; n<EI(HexAction::count); n++) {
                        if (n == EI(HexAction::MOVE)) continue;
                        if (ham.at(n)) {
                            sym = "◎";
                            break;
                        }
                    }
                }
                break;
                case HexState::FREE_UNREACHABLE:
                    ASSERT(!anyattr || r.ended, "FREE_UNREACHABLE but anyattr is true");
                    // at end-of-battle, hexes are updated but hexactmasks are not
                    ASSERT(!anyham || r.ended, "FREE_UNREACHABLE but anyham is true");
                    sym = "\033[90m◌\033[0m";
                break;
                case HexState::OBSTACLE:
                    ASSERT(!anyattr || r.ended, "OBSTACLE but anyattr is true");
                    ASSERT(!anyham || r.ended, "OBSTACLE but anyham is true");
                    sym = "\033[90m▦\033[0m";
                break;
                case HexState::OCCUPIED: {
                    ASSERT(anyattr || r.ended, "OCCUPIED but anyattr is false");
                    auto cstack = hex.stack->cstack;
                    ASSERT(cstack, "OCCUPIED hex with nullptr cstack");
                    auto slot = hex.stack->attrs.at(EI(StackAttr::Slot));
                    auto enemy = cstack->unitSide() != astack->unitSide();
                    ASSERT(hex.stack->attrs.at(EI(StackAttr::Side)) == cstack->unitSide(), "wrong unit side");
                    auto col = enemy ? enemycol : ourcol;

                    if (cstack->unitId() == astack->unitId())
                        col += activemod;

                    // stacks contains 14 elements, not 7 (don't use slot)
                    auto myslot = (cstack->unitSide() == LIB_CLIENT::BattleSide::ATTACKER)
                        ? slot : slot + 7;

                    stacks.at(myslot) = hex.stack;
                    sym = col + std::to_string(slot+1) + nocol;

                    // Check attributes
                    // TODO: do I need to account for dead stacks at end-of-battle?
                    //      ASSERT(attr == nv.orig || r.ended, "attr: " + std::to_string(attr) + " != " + std::to_string(nv.orig));
                    for (int j=0; j<EI(StackAttr::count); j++) {
                        // ASSERT(state[ibase+3+j].orig == hex.stack->attrs[j], "attr check failed");
                        expect(state.at(ibase+3+j).orig == hex.stack->attrs.at(j), "attr check failed: state[%d].orig=%d != hex.stack->attrs[%d]=%d", ibase+3+j, state[ibase+3+j].orig, j, hex.stack->attrs[j]);
                    }
                }
                break;
                default:
                    throw std::runtime_error("unexpected hex.state: " + std::to_string(EI(hex.state)));
                }

                row << sym;

                if (x == BF_XMAX-1) {
                    row << (y % 2 == 0 ? " " : "  ") << "┠" << nummap.at(y % 10);
                }
            }
        }

        rows.emplace_back() << " ┃▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁┃";
        rows.emplace_back() << "  ▕¹▕²▕³▕⁴▕⁵▕⁶▕⁷▕⁸▕⁹▕⁰▕¹▕²▕³▕⁴▕⁵▕";

        //
        // 4. Add side table stuff
        //
        //   ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕
        //  ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃    Last action:
        // ₁┨  ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ┠₁         Errors: 0
        // ₂┨ ○ ○ ○ ○ ○ ○ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌ ◌  ┠₂      DMG dealt: 0
        // ₃┨  1 ○ ○ ○ ○ ○ ◌ ◌ ▦ ▦ ◌ ◌ ◌ ◌ 1 ┠₃   Units killed: 0
        // ...

        auto n_errors = 0;
        auto errmask = r.errmask;
        while (errmask) {
                n_errors += errmask & 1;
                errmask >>= 1;
        }

        for (int i=0; i<=rows.size(); i++) {
            std::string name;
            std::string value;

            switch(i) {
            case 1:
                name = "Player";
                value = ourcol == redcol ? redcol + "RED" + nocol : bluecol + "BLUE" + nocol;
                break;
            case 2:
                name = "Last action";
                value = action ? action->name() + " [" + std::to_string(action->action) + "]" : "";
                break;
            case 3: name = "Errors"; value = std::to_string(n_errors); break;
            case 4: name = "DMG dealt"; value = std::to_string(r.dmgDealt); break;
            case 5: name = "Units killed"; value = std::to_string(r.unitsKilled); break;
            case 6: name = "Value killed"; value = std::to_string(r.valueKilled); break;
            case 7: name = "DMG received"; value = std::to_string(r.dmgReceived); break;
            case 8: name = "Units lost"; value = std::to_string(r.unitsLost); break;
            case 9: name = "Value lost"; value = std::to_string(r.valueLost); break;
            case 10: {
                auto restext = r.victory
                    ? (ourcol == redcol ? redcol + "RED WINS" : bluecol + "BLUE WINS")
                    : (ourcol == redcol ? bluecol + "BLUE WINS" : redcol + "RED WINS");

                name = "Battle result"; value = r.ended ? (restext + nocol) : "";
                break;
            }
            // case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
            default:
                continue;
            }

            rows.at(tablestartrow + i) << PadLeft(name, 15, ' ') << ": " << value;
        }

        //
        // 5. Add stats table:
        //
        //          Stack # |   1   2   3   4   5   6   7   1   2   3   4   5   6   7
        // -----------------+--------------------------------------------------------
        //              Qty |   0  34   0   0   0   0   0   0  17   0   0   0   0   0
        //           Attack |   0   8   0   0   0   0   0   0   6   0   0   0   0   0
        //    ...10 more... | ...
        // -----------------+--------------------------------------------------------
        //

        //
        // XXX: Table is initially constructed in rotated form
        // => read "rows" as "cols" and vice versa...
        //    (until the comment line "XXX: table is rotated here")
        //

        // table with 14+1 rows, ATTRS_PER_STACK+1 cells each (+1 for headers)
        const auto nrows = 14+1;
        const auto ncols = EI(StackAttr::count) + 1 - 5; // hide IsEnemy, Slot, CreatureType, AIValue IsActive

        // Table with nrows and ncells, each cell a 3-element tuple
        auto table = std::array<
            std::array<  // row
                std::tuple<std::string, int, std::string>, // cell: color, width, txt
                ncols
            >,
            nrows
        >{};

        auto colwidth = 4;
        auto headercolwidth = 16;

        static_assert(ncols == 13);
        static_assert(EI(StackAttr::Quantity) == 0);
        static_assert(EI(StackAttr::Attack) == 1);
        static_assert(EI(StackAttr::Defense) == 2);
        static_assert(EI(StackAttr::Shots) == 3);
        static_assert(EI(StackAttr::DmgMin) == 4);
        static_assert(EI(StackAttr::DmgMax) == 5);
        static_assert(EI(StackAttr::HP) == 6);
        static_assert(EI(StackAttr::HPLeft) == 7);
        static_assert(EI(StackAttr::Speed) == 8);
        static_assert(EI(StackAttr::Waited) == 9);
        static_assert(EI(StackAttr::QueuePos) == 10); // 0=active stack
        static_assert(EI(StackAttr::RetaliationsLeft) == 11);

        table[0] = {
            std::tuple{nocol, headercolwidth, "Stack #"},
            std::tuple{nocol, headercolwidth, "Qty"},
            std::tuple{nocol, headercolwidth, "Attack"},
            std::tuple{nocol, headercolwidth, "Defense"},
            std::tuple{nocol, headercolwidth, "Shots"},
            std::tuple{nocol, headercolwidth, "Dmg (min)"},
            std::tuple{nocol, headercolwidth, "Dmg (max)"},
            std::tuple{nocol, headercolwidth, "HP"},
            std::tuple{nocol, headercolwidth, "HP left"},
            std::tuple{nocol, headercolwidth, "Speed"},
            std::tuple{nocol, headercolwidth, "Waited"},
            std::tuple{nocol, headercolwidth, "Queue"},
            std::tuple{nocol, headercolwidth, "Ret. left"},

        };

        for (int row=1, i=0; row<nrows; row++) {
            auto nstack = row-1;  // stack number 0..13
            auto stackdisplay = std::to_string(nstack % 7 + 1); // stack 1..7
            auto &stack = stacks.at(nstack);

            table[row][0] = std::tuple{"X", colwidth, stackdisplay};  // "X" changed later

            for (int col=1; col<ncols; col++, i++) {
                auto color = nocol;
                auto attr = ATTR_NA;
                auto headersuffix = "";

                if (stack) {
                    auto cstack = stack->cstack;
                    ASSERT(cstack, "NULL cstack");
                    ASSERT(cstack->unitSide() == stack->attrs.at(EI(StackAttr::Side)), "stack side mismatch");
                    // at activeStack(), Stack objects are created only for alive CStacks
                    // at end-of-battle, Stack objects of dead CStacks are is still there
                    // => there should be no dead stacks unless battle ends
                    if (!cstack->alive()) {
                        ASSERT(r.ended, "dead stack at " + std::to_string(nstack));
                    } else {
                        attr = stack->attrs.at(col-1); // col starts at 1
                        color = (cstack->unitSide() == LIB_CLIENT::BattleSide::ATTACKER)
                            ? redcol : bluecol;

                        if (cstack->unitId() == astack->unitId()) {
                            color += activemod;
                            headersuffix = "*";
                        }
                    }
                }

                if (col == 1) {
                    std::get<0>(table[row][0]) = color;         // change header color
                    std::get<2>(table[row][0]) += headersuffix; // mark active unit
                }

                // table[row][col] = std::tuple{color, colwidth, std::to_string(attr)};
                table[row][col] = std::tuple{color, colwidth, (attr == ATTR_NA) ? "" : std::to_string(attr)};
            }
        }

        auto divrow = std::stringstream();
        divrow << PadLeft("", headercolwidth, '-');
        divrow << "-+";
        divrow << PadLeft("", (nrows-1)*colwidth, '-');
        auto divrowstr = divrow.str();

        // XXX: table is rotated here
        for (int r=0; r<ncols; r++) {
            auto row = std::stringstream();
            for (int c=0; c<nrows; c++) {
                auto [color, width, txt] = table[c][r];
                row << color << PadLeft(txt, width, ' ') << nocol;

                // divider
                if (c == 0)
                    row << " |";
            }

            rows.push_back(std::move(row));

            // divider (after header row)
            if (r == 0)
                rows.emplace_back(divrowstr);
        }

        // divider (after last row)
        rows.emplace_back(divrowstr);

        //
        // 6. Join rows into a single string
        //
        std::string res = rows[0].str();
        for (int i=1; i<rows.size(); i++)
            res += "\n" + rows[i].str();

        return res;
    }
}
