#include "BAI.h"
#include <sstream>

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


std::string BAI::gymaction_str(const GymAction &ga) {
  return std::to_string(ga);
}

const std::string BAI::buildReport(const GymResult &gr, const GymAction &ga, const CStack* astack) {
  auto gs = gr.state;

  std::string nocol = "\033[0m";
  std::string enemycol = "\033[31m"; // red
  std::string allycol = "\033[32m"; // green
  // std::string activecol = "\033[33m\033[44m"; // yellow on blue background
  std::string activecol = "\033[32m\033[1m"; // green bold


  //
  // 1. Build ASCII table
  //    (+populate aliveStacks)
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
    auto &nv = gs[i];
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
        color = (astack && astack->unitSlot() == nstack)
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

  for (int i=0; i<=rows.size(); i++) {
    std::string name;
    std::string value;

    switch(i) {
    case 1: name = "Action"; value = allGymActionNames[gymaction];; break;
    case 2: name = "n_errors"; value = std::to_string(gr.n_errors); break;
    case 3: name = "dmgDealt"; value = std::to_string(gr.dmgDealt); break;
    case 4: name = "dmgReceived"; value = std::to_string(gr.dmgReceived); break;
    case 5: name = "ended"; value = std::to_string(gr.ended); break;
    case 6: name = "victory"; value = std::to_string(gr.victory); break;
    default:
      continue;
    }

    rows[i] << "  " << padLeft(name, 13, ' ') << ": " << value;
  }

  //
  // 3. Add stats table
  //
  //   QTY ATT DEF SHT MD0 MD1 RD0 RD1  HP HPL SPD WAI
  // 1   1  18  19  24  14  14  14  14  30  30   9   0
  // 2   0   0   0   0   0   0   0   0   0   0   0   0
  // 3   0   0   0   0   0   0   0   0   0   0   0   0
  //

  // table with 14+1 rows, ATTRS_PER_STACK+1 cells each (+1 for headers)
  assert(BF_SIZE + 14*ATTRS_PER_STACK == gs.size() - 1);

  const auto nrows = 14+1;
  const auto ncols = ATTRS_PER_STACK+1;

  auto table = std::array<std::array<std::pair<std::string, std::string>, ncols>, nrows>{};
  table[0] = {
    std::pair{"STACK", nocol},
    std::pair{"QTY", nocol},
    std::pair{"ATT", nocol},
    std::pair{"DEF", nocol},
    std::pair{"SHT", nocol},
    std::pair{"MD0", nocol},
    std::pair{"MD1", nocol},
    std::pair{"RD0", nocol},
    std::pair{"RD1", nocol},
    std::pair{"HP", nocol},
    std::pair{"HPL", nocol},
    std::pair{"SPD", nocol},
    std::pair{"WAI", nocol}
  };

  int i = 0;
  for (int r=1; r<nrows; r++) {
    auto nstack = r-1;  // stack number 0..13
    auto stackdisplay = std::to_string(nstack % 7 + 1); // stack 1..7

    table[r][0] = {stackdisplay, "X"};  // "X" changed later

    for (int c=1; c<ncols; c++) {
      auto nv = gs[i+BF_SIZE];
      auto color = nocol;

      if (alivestacks[nstack]) {
        if (nstack > 6)
          color = enemycol;
        else
          color = (astack && astack->unitSlot() == nstack) ? activecol : allycol;
      }

      table[r][0].second = color;  // change header color
      table[r][c] = {std::to_string(nv.orig), color};
      i++;
    }
  }

  for (int r=0; r<nrows; r++) {
    auto row = std::stringstream();
    for (int c=0; c<ncols; c++) {
      auto cell = table[r][c];
      row << cell.second << padLeft(cell.first, 5, ' ') << nocol;
    }
    rows.push_back(std::move(row));
  }

  // row << nocolor;
  // rows.push_back(std::move(row));

  // //
  // // 4. Add waited
  // //
  // // [waited]        0
  // //

  // rows.emplace_back() << "[waited]        " << std::to_string(gs[gs.size()-1].orig);

  // //
  // // 5. Convert to a single string
  // //

  std::string res = rows[0].str();

  for (int i=1; i<rows.size(); i++) {
    res += "\n" + rows[i].str();
  }

  return res;
}

MMAI_NS_END
