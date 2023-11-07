#include "BAI.h"
#include <iomanip>
#include <sstream>

MMAI_NS_BEGIN

std::string padString(const std::string& input, size_t desiredLength, char paddingChar) {
    std::ostringstream ss;
    ss << std::right << std::setfill(paddingChar) << std::setw(desiredLength) << input;
    return ss.str();
}

std::string BAI::gymaction_str(const GymAction &ga) {
  return std::to_string(ga);
}

std::string BAI::gymresult_str(const GymResult &gr) {
  const auto gs = gr.state;
  std::stringstream ss;

  auto stacknum = 1;
  std::array<bool, 14> alivestacks{0};

  for (int i=0; i < gs.size(); i++) {
    auto nv = gs[i];

    if (i < BF_SIZE) {
      if (i == 0) ss << "\n" << padString("", 34, '-') << "\n| ";
      else if (i % BF_XMAX == 0) {
        std::string stat = "";
        switch(i / BF_XMAX) {
        case 0: stat = "  n_errors:    " + std::to_string(gr.n_errors); break;
        case 1: stat = "  dmgDealt:    " + std::to_string(gr.dmgDealt); break;
        case 2: stat = "  dmgReceived: " + std::to_string(gr.dmgReceived); break;
        case 3: stat = "  ended:       " + std::to_string(gr.ended); break;
        case 4: stat = "  victory:     " + std::to_string(gr.victory); break;
        }
        ss << ((i % 2 == 0) ? "  |" : " |");
        ss << stat;
        ss << ((i % 2 == 0) ? "\n| " : "\n|");
      }

      ss << " ";

      switch(nv.orig) {
      case 0: ss << "○"; break; // FREE_REACHABLE
      case 1: ss << "\033[90m◌\033[0m"; break; // FREE_UNREACHABLE
      case 2: ss << "\033[90m▦\033[0m"; break; // OBSTACLE
      default:
        alivestacks[nv.orig - 3] = true;
        if (nv.orig <= 9)
          ss << "\033[32m" << (nv.orig - 2); // green
        else
          ss << "\033[31m" << (nv.orig - 9); // red

        ss << "\033[0m"; // reset
      }
    } else if (i < gs.size() - 1) {
      if (i == BF_SIZE) {
        ss << " |\n" << padString("", 66, '-') << "\n";
        ss << "              QTY   ATT   DEF  SHOT   MD0   MD1   RD0   RD1    HP   HPL   SPD   WAI";
      }

      if ((i-BF_SIZE) % ATTRS_PER_STACK == 0) {
        if (stacknum > 7) {
          if (alivestacks[stacknum-1])
            ss << "\n\033[31m[E stack #" << (stacknum - 7) << "]";
          else
            ss << "\n\033[0m[E stack #" << (stacknum - 7) << "]";
        } else {
          if (alivestacks[stacknum-1])
            ss << "\n\033[32m[F stack #" << (stacknum) << "]";
          else
            ss << "\n\033[0m[F stack #" << (stacknum) << "]";
        }

        stacknum += 1;
      }
      ss << padString(std::to_string(nv.orig), 5, ' ') << ",";
    } else {
      ss << "\033[0m\n[waited] " << nv.orig;
    }
  }

  ss << "\n]";

  return ss.str();
}

MMAI_NS_END
