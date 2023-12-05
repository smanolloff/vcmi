#include "BAI.h"
#include "types/battlefield.h"

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
        const auto &aslot = r.state[r.state.size() - 1].orig;
        const auto &bf = *battlefield;
        const auto &stacks = bf.stacks;

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
            auto errmsg = std::get<2>(errtuple);
            if (r.errmask & errflag)
                rows.emplace_back("Error: " + errmsg);
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

        std::array<bool, 14> alivestacks{0};

        auto tablestartrow = rows.size();

        rows.emplace_back() << "  ▕₁▕₂▕₃▕₄▕₅▕₆▕₇▕₈▕₉▕₀▕₁▕₂▕₃▕₄▕₅▕";
        rows.emplace_back() << " ┃▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔┃ ";

        static std::array<std::string, 10> nummap{"₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉", "₀"};

        for (int i=0; i < BF_SIZE; i++) {
            auto sym = std::string("?");
            auto &hex = bf.hexes[i];
            auto &ham = hex.hexactmask;
            auto anyham = std::any_of(std::begin(ham), std::end(ham), [](bool val) { return val; });

            auto &nv = state[i];
            auto &row = (i % BF_XMAX == 0)
                ? (rows.emplace_back() << nummap[(i/BF_XMAX)%10] << "┨" << (i % 2 == 0 ? " " : ""))
                : rows.back();

            row << " ";

            ASSERT(hex.state == HexState(nv.orig), "hex.state: " + std::to_string(EI(hex.state)) + " != " + std::to_string(nv.orig));

            switch(hex.state) {
            break; case HexState::FREE_REACHABLE:
                ASSERT(ham[EI(HexAction::MOVE)], "FREE_REACHABLE but mask[MOVE] is false");
                sym = "○";

                // check if hex also allows attack
                for (int j=0; j<=EI(HexAction::MOVE_AND_ATTACK_6); j++) {
                    if(ham[j]) {
                        sym = "◎";
                        break;
                    }
                }
            break; case HexState::FREE_UNREACHABLE:
                // at end-of-battle, hexes are updated bug hexactmasks are not
                ASSERT(!anyham || r.ended, "FREE_UNREACHABLE but anyham is true");
                sym = "\033[90m◌\033[0m";
            break; case HexState::OBSTACLE:
                ASSERT(!anyham, "OBSTACLE but anyham is true");
                sym = "\033[90m▦\033[0m";
            break; case HexState::FRIENDLY_STACK_0:
            case HexState::FRIENDLY_STACK_1:
            case HexState::FRIENDLY_STACK_2:
            case HexState::FRIENDLY_STACK_3:
            case HexState::FRIENDLY_STACK_4:
            case HexState::FRIENDLY_STACK_5:
            case HexState::FRIENDLY_STACK_6:
            case HexState::ENEMY_STACK_0:
            case HexState::ENEMY_STACK_1:
            case HexState::ENEMY_STACK_2:
            case HexState::ENEMY_STACK_3:
            case HexState::ENEMY_STACK_4:
            case HexState::ENEMY_STACK_5:
            case HexState::ENEMY_STACK_6: {
                static_assert(EI(HexState::ENEMY_STACK_0) == 10);
                static_assert(EI(HexState::FRIENDLY_STACK_0) == 3);

                auto slot = EI(hex.state) - 10;
                auto col = enemycol;

                if (EI(hex.state) < EI(HexState::ENEMY_STACK_0)) {
                    slot = EI(hex.state) - 3;
                    col = (aslot == slot) ? activecol : allycol;
                }

                // alivestacks contains 14 elements, not 7 (don't use slot)
                alivestacks[EI(hex.state) - 3] = true;
                sym = col + std::to_string(slot+1) + nocol;
            }
            break; default:
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
                value = action ? action->name() + " (" + std::to_string(action->action) + ")" : "";
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

        // table with 14+1 rows, ATTRS_PER_STACK+1 cells each (+1 for headers)
        static_assert(BF_SIZE + 14*EI(StackAttr::count) == std::tuple_size<Export::State>::value - 1);

        const auto nrows = 14+1;
        const auto ncols = EI(StackAttr::count) + 1;

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

        static_assert(ncols == 11);
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
            std::tuple{nocol, headercolwidth, "Waited"}
        };

        for (int row=1, i=0; row<nrows; row++) {
            auto nstack = row-1;  // stack number 0..13
            auto stackdisplay = std::to_string(nstack % 7 + 1); // stack 1..7
            auto &stack = stacks[nstack];

            table[row][0] = std::tuple{"X", colwidth, stackdisplay};  // "X" changed later

            for (int col=1; col<ncols; col++, i++) {
                auto nv = state[i+BF_SIZE];
                auto color = nocol;
                auto attr = 0;

                if (alivestacks[nstack]) {
                    ASSERT(stack, "null stack at " + std::to_string(nstack));
                    attr = stack->attrs[col-1]; // col starts at 1
                    if (nstack > 6)
                        color = enemycol;
                    else
                        color = (aslot == nstack) ? activecol : allycol;
                } else {
                    // at activeStack(), Stack objects are created only for alive CStacks
                    // at end-of-battle, Stack objects of dead CStacks are is still there
                    ASSERT(!stack || (r.ended && !stack->stack->alive()), "alive stack at " + std::to_string(nstack));
                }

                // see note above
                ASSERT(attr == nv.orig || r.ended, "attr: " + std::to_string(attr) + " != " + std::to_string(nv.orig));

                std::get<0>(table[row][0]) = color;  // change header color

                // table[row][col] = std::tuple{color, colwidth, std::to_string(attr)};
                table[row][col] = std::tuple{color, colwidth, stack && stack->stack->alive() ? std::to_string(attr) : ""};
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
