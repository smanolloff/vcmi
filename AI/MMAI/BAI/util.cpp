#include "BAI.h"
#include "export.h"
#include "types/battlefield.h"
#include "types/stack.h"

namespace MMAI {
    std::string padLeft(const std::string& input, size_t desiredLength, char paddingChar) {
        std::ostringstream ss;
        ss << std::right << std::setfill(paddingChar) << std::setw(desiredLength) << input;
        return ss.str();
    }

    std::string padRight(const std::string& input, size_t desiredLength, char paddingChar) {
        std::ostringstream ss;
        ss << std::left << std::setfill(paddingChar) << std::setw(desiredLength) << input;
        return ss.str();
    }

    // XXX:
    // this method *could* use Battlefield and friends,
    // but is only using the result on purpose:
    // it validates that the result is correct
    std::string BAI::renderANSI() {
        const auto &r = *result;
        const auto &bf = *battlefield;

        ASSERT(r.state.size() == 165 * (1 + EI(StackAttr::count)), "Unexpected state size: " + std::to_string(r.state.size()));

        auto state = r.state;

        std::string nocol = "\033[0m";
        std::string enemycol = "\033[31m"; // red
        std::string allycol = "\033[32m"; // green
        std::string activecol = "\033[32m\033[1m"; // green bold

        std::vector<std::stringstream> rows;

        //
        // 1. Add logs table:
        //
        // #1 attacks #5 for 16 dmg (1 killed)
        // #5 attacks #1 for 4 dmg (0 killed)
        // ...
        //
        for (auto &alog : attackLogs) {
            auto row = std::stringstream();
            auto col1 = allycol;
            auto col2 = enemycol;

            if (alog.isOurStackAttacked) {
                col1 = enemycol;
                col2 = allycol;
            }

            row << col1 << "#" << alog.attslot + 1 << nocol;
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

        for (int i=0; i < BF_SIZE; i++) {
            auto sym = std::string("?");
            auto &hex = bf.hexes[i];
            auto &ham = hex.hexactmask;

            auto &row = (i % BF_XMAX == 0)
                ? (rows.emplace_back() << nummap[(i/BF_XMAX)%10] << "┨" << (i % 2 == 0 ? " " : ""))
                : rows.back();

            row << " ";

            // ibase = first state index of the i-th hex
            // eg. for hex 2, ibase=15 where:
            //  state[15] is hexstate for hex2
            //  state[16] is hex.stack->attr[Qty] for hex2
            //  state[17] is hex.stack->attr[Att] for hex2
            //  ... etc
            auto ibase = i * (1 + EI(StackAttr::count));
            auto &nvstate = state[ibase];
            expect(hex.state == HexState(nvstate.orig), "hex.state=%d != state[%d]=%d", hex.state, ibase, nvstate.orig);

            // true if any hexactionmask is true
            auto anyham = std::any_of(ham.begin(), ham.end(), [](bool val) { return val; });

            // true if any stack attribute for that hex is valid
            auto anyattr = std::any_of(
                state.begin()+ibase+1,
                state.begin()+ibase+1+EI(StackAttr::count),
                [](Export::NValue nv) { return nv.orig != ATTR_NA; }
            );

            switch(hex.state) {
            case HexState::FREE_REACHABLE: {
                ASSERT(!anyattr, "FREE_REACHABLE but anyattr is true");
                ASSERT(ham[EI(HexAction::MOVE)], "FREE_REACHABLE but mask[MOVE] is false");
                sym = "○";

                for (const auto& [_, hexaction] : EDIR_TO_MELEE) {
                    if (ham[EI(hexaction)]) {
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
                auto slot = hex.stack->attrs[EI(StackAttr::Slot)];
                auto enemy = cstack->unitSide() != bf.astack->unitSide();
                ASSERT(hex.stack->attrs[EI(StackAttr::Side)] == cstack->unitSide(), "wrong unit side");
                auto col = enemy ? enemycol : allycol;

                if (cstack->unitId() == bf.astack->unitId())
                    col = activecol;

                // stacks contains 14 elements, not 7 (don't use slot)
                stacks[slot + (enemy ? 7 : 0)] = hex.stack;
                sym = col + std::to_string(slot+1) + nocol;

                // Check attributes
                // TODO: do I need to account for dead stacks at end-of-battle?
                //      ASSERT(attr == nv.orig || r.ended, "attr: " + std::to_string(attr) + " != " + std::to_string(nv.orig));
                for (int j=0; j<EI(StackAttr::count); j++) {
                    // ASSERT(state[ibase+1+j].orig == hex.stack->attrs[j], "attr check failed");
                    expect(state[ibase+1+j].orig == hex.stack->attrs[j], "attr check failed: state[%d].orig=%d != hex.stack->attrs[%d]=%d", ibase+1+j, state[ibase+1+j].orig, j, hex.stack->attrs[j]);
                }
            }
            break;
            default:
                throw std::runtime_error("unexpected hex.state: " + std::to_string(EI(hex.state)));
            }

            row << sym;

            if (i % BF_XMAX == BF_XMAX-1) {
                row << (i % 2 == 0 ? " " : "  ") << "┠" << nummap[(i/BF_XMAX)%10];
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
                name = "Last action";
                value = action ? action->name() + " [" + std::to_string(action->action) + "]" : "";
                break;
            case 2: name = "Errors"; value = std::to_string(n_errors); break;
            case 3: name = "DMG dealt"; value = std::to_string(r.dmgDealt); break;
            case 4: name = "Units killed"; value = std::to_string(r.unitsKilled); break;
            case 5: name = "Value killed"; value = std::to_string(r.valueKilled); break;
            case 6: name = "DMG received"; value = std::to_string(r.dmgReceived); break;
            case 7: name = "Units lost"; value = std::to_string(r.unitsLost); break;
            case 8: name = "Value lost"; value = std::to_string(r.valueLost); break;
            case 9: name = "Battle result"; value = r.ended ? (r.victory ? (allycol + "VICTORY" + nocol) : (enemycol + "DEFEAT" + nocol)) : ""; break;
            // case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
            default:
                continue;
            }

            rows[tablestartrow + i] << padLeft(name, 15, ' ') << ": " << value;
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
        const auto ncols = EI(StackAttr::count) + 1 - 2; // hide IsEnemy and Slot

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

        static_assert(ncols == 12);
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

        };

        for (int row=1, i=0; row<nrows; row++) {
            auto nstack = row-1;  // stack number 0..13
            auto stackdisplay = std::to_string(nstack % 7 + 1); // stack 1..7
            auto &stack = stacks[nstack];

            table[row][0] = std::tuple{"X", colwidth, stackdisplay};  // "X" changed later

            for (int col=1; col<ncols; col++, i++) {
                auto color = nocol;
                auto attr = ATTR_NA;

                if (stack) {
                    auto cstack = stack->cstack;
                    ASSERT(cstack, "NULL cstack");
                    ASSERT(cstack->unitSide() == stack->attrs[EI(StackAttr::Side)], "stack side mismatch");
                    // at activeStack(), Stack objects are created only for alive CStacks
                    // at end-of-battle, Stack objects of dead CStacks are is still there
                    // => there should be no dead stacks unless battle ends
                    if (!cstack->alive()) {
                        ASSERT(r.ended, "dead stack at " + std::to_string(nstack));
                    } else {
                        attr = stack->attrs[col-1]; // col starts at 1
                        if (cstack->unitId() == bf.astack->unitId())
                            color = activecol;
                        else if (cstack->unitSide() == bf.astack->unitSide())
                            color = allycol;
                        else
                            color = enemycol;
                    }
                }

                std::get<0>(table[row][0]) = color;  // change header color

                // table[row][col] = std::tuple{color, colwidth, std::to_string(attr)};
                table[row][col] = std::tuple{color, colwidth, (attr == ATTR_NA) ? "" : std::to_string(attr)};
            }
        }

        auto divrow = std::stringstream();
        divrow << padLeft("", headercolwidth, '-');
        divrow << "-+";
        divrow << padLeft("", (nrows-1)*colwidth, '-');
        auto divrowstr = divrow.str();

        // XXX: table is rotated here
        for (int r=0; r<ncols; r++) {
            auto row = std::stringstream();
            for (int c=0; c<nrows; c++) {
                auto [color, width, txt] = table[c][r];
                row << color << padLeft(txt, width, ' ') << nocol;

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
