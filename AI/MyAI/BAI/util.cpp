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

  int nstack = 0;
  std::string nocolor = "\033[0m";

  auto row = std::stringstream();
  row << "  QTY ATT DEF SHT MD0 MD1 RD0 RD1  HP HPL SPD WAI";

  for (int i=BF_SIZE; i < gs.size()-1; i++) {
    auto nv = gs[i];

    if ((i-BF_SIZE) % ATTRS_PER_STACK == 0) {
      rows.push_back(std::move(row));
      row = std::stringstream();
      auto color = nocolor;
      auto nstackvis = (nstack < 7) ? nstack + 1 : nstack - 6;

      if (alivestacks[nstack]) {
        if (nstack > 6)
          color = enemycol;
        else
          color = (astack && astack->unitSlot() == nstack) ? activecol : allycol;
      }

      row << color << nstackvis;
      nstack++;
    }

    row << padLeft(std::to_string(nv.orig), 4, ' ');
  }

  row << nocolor;
  rows.push_back(std::move(row));

  //
  // 4. Add waited
  //
  // [waited]        0
  //

  rows.emplace_back() << "[waited]        " << std::to_string(gs[gs.size()-1].orig);

  //
  // 5. Convert to a single string
  //

  std::string res = rows[0].str();

  for (int i=1; i<rows.size(); i++) {
    res += "\n" + rows[i].str();
  }

  return res;
}

MMAI_NS_END
