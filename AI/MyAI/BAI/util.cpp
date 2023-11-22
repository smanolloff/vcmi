#include "BAI.h"
#include <sstream>
#include <cstdio>

MMAI_NS_BEGIN

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


std::string BAI::action_str(const Action &a) {
  return std::to_string(a);
}

std::string BAI::renderANSI() {
  const Result &r = result;
  const Action &a = action;
  const auto &aslot = r.state[r.state.size() - 1].orig;

  auto state = r.state;

  std::string nocol = "\033[0m";
  std::string enemycol = "\033[31m"; // red
  std::string allycol = "\033[32m"; // green
  std::string activecol = "\033[32m\033[1m"; // green bold


  //
  // 1. Build ASCII table
  //    (+populate aliveStacks var)
  //
  // ----------------------------------
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
  // ----------------------------------
  //

  std::vector<std::stringstream> rows;
  std::array<bool, 14> alivestacks{0};

  // std::stringstream &ss = rows.emplace_back();
  // ss << padLeft("", 34, '-');
  rows.emplace_back() << padLeft("", 34, '-');

  for (int i=0; i < BF_SIZE; i++) {
    auto &nv = state[i];
    auto &row = (i % BF_XMAX == 0)
      ? (rows.emplace_back() << (i % 2 == 0 ? "| " : "|"))
      : rows.back();

    row << " ";
    switch(nv.orig) {
    case 0: row << "○"; break; // FREE_REACHABLE
    case 1: row << "\033[90m◌\033[0m"; break; // FREE_UNREACHABLE
    case 2: row << "\033[90m▦\033[0m"; break; // OBSTACLE
    default:
      auto nstack = nv.orig - 3;
      auto nstackvis = nstack + 1;
      auto color = nocol;

      alivestacks[nstack] = true;

      if (nstack > 6) {
        nstackvis -= 7;
        color = enemycol;
      } else {
        color = (aslot == nstack)
          ? activecol : allycol;
      }

      row << color << nstackvis << nocol;
    }

    if (i % BF_XMAX == BF_XMAX-1) {
      row << (i % 2 == 0 ? " |" : "  |");
    }
  }

  rows.emplace_back() << padLeft("", 34, '-');

  //
  // 2. Add side table stuff
  //
  // ----------------------------------
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |  dmgDealt:    0
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |  dmgReceived: 0
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |  ended:       1
  // | ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○  |  victory:     1
  // |  ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ ○ |
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
    case 1: name = "Last action"; value = (a == ACTION_UNSET) ? "" : allActionNames[action]; break;
    case 2: name = "Errors"; value = std::to_string(n_errors); break;
    case 3: name = "DMG dealt"; value = std::to_string(r.dmgDealt); break;
    case 4: name = "Units killed"; value = std::to_string(r.unitsKilled); break;
    case 5: name = "Value killed"; value = std::to_string(r.valueKilled); break;
    case 6: name = "DMG received"; value = std::to_string(r.dmgReceived); break;
    case 7: name = "Units lost"; value = std::to_string(r.unitsLost); break;
    case 8: name = "Value lost"; value = std::to_string(r.valueLost); break;
    case 9: name = "Battle ended"; value = r.ended ? "yes" : "no"; break;
    case 10: name = "Victory"; value = r.ended ? (r.victory ? "yes" : "no") : ""; break;
    default:
      continue;
    }

    rows[i] << padLeft(name, 15, ' ') << ": " << value;
  }

  //
  // 3. Add stats table
  //    NOTE: below example is incorrect, table is now rotated
  //
  //   QTY ATT DEF SHT MD0 MD1 RD0 RD1  HP HPL SPD WAI
  // 1   1  18  19  24  14  14  14  14  30  30   9   0
  // 2   0   0   0   0   0   0   0   0   0   0   0   0
  // 3   0   0   0   0   0   0   0   0   0   0   0   0
  //

  // table with 14+1 rows, ATTRS_PER_STACK+1 cells each (+1 for headers)
  assert(BF_SIZE + 14*ATTRS_PER_STACK == state.size() - 1);

  const auto nrows = 14+1;
  const auto ncols = ATTRS_PER_STACK+1;

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

  table[0] = {
    std::tuple{nocol, headercolwidth, "Stack #"},
    std::tuple{nocol, headercolwidth, "Qty"},
    std::tuple{nocol, headercolwidth, "Attack"},
    std::tuple{nocol, headercolwidth, "Defense"},
    std::tuple{nocol, headercolwidth, "Shots"},
    std::tuple{nocol, headercolwidth, "Dmg min (melee)"},
    std::tuple{nocol, headercolwidth, "Dmg max (melee)"},
    std::tuple{nocol, headercolwidth, "Dmg min (ranged)"},
    std::tuple{nocol, headercolwidth, "Dmg max (ranged)"},
    std::tuple{nocol, headercolwidth, "HP"},
    std::tuple{nocol, headercolwidth, "HP left"},
    std::tuple{nocol, headercolwidth, "Speed"},
    std::tuple{nocol, headercolwidth, "Waited"}
  };

  int i = 0;
  for (int r=1; r<nrows; r++) {
    auto nstack = r-1;  // stack number 0..13
    auto stackdisplay = std::to_string(nstack % 7 + 1); // stack 1..7

    table[r][0] = std::tuple{"X", colwidth, stackdisplay};  // "X" changed later

    for (int c=1; c<ncols; c++) {
      auto nv = state[i+BF_SIZE];
      auto color = nocol;

      if (alivestacks[nstack]) {
        if (nstack > 6)
          color = enemycol;
        else
          color = (aslot == nstack) ? activecol : allycol;
      }

      std::get<0>(table[r][0]) = color;  // change header color
      table[r][c] = std::tuple{color, colwidth, std::to_string(nv.orig)};
      i++;
    }
  }

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

    // divider
    if (r == 0) {
      auto divrow = std::stringstream();
      divrow << padLeft("", headercolwidth, '-');
      divrow << "-+";
      divrow << padLeft("", (nrows-1)*colwidth, '-');
      rows.push_back(std::move(divrow));
    }
  }

  //
  // 5. Convert to a single string
  //

  std::string res = rows[0].str();
  for (int i=1; i<rows.size(); i++)
    res += "\n" + rows[i].str();

  return res;
}

MMAI_NS_END
