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

std::string BAI::gymstate_str(const GymState &gs) {
  std::stringstream ss;

  ss << "[";
  auto stacknum = 1;

  for (int i=0; i < gs.size(); i++) {
    auto nv = gs[i];

    if (i < BF_SIZE) {
      if (i == 0) ss << "\n" << padString("", 66, '-') << "\n|  ";
      else if (i % BF_XMAX == 0)
        ss << ((i % 2 == 0) ? "    |\n|  " : "  |\n|");

      ss << padString(std::to_string(nv.orig), 3, ' ') << ",";
    } else if (i < gs.size() - 1) {
      if (i == BF_SIZE) {
        ss << "  |\n" << padString("", 66, '-') << "\n";
        ss << "              QTY   ATT   DEF  SHOT DMGM0 DMGM1 DMGR0 DMGR1    HP   HPL   SPD   WAI";
      }

      if ((i-BF_SIZE) % ATTRS_PER_STACK == 0) {
        if (stacknum > 7)
          ss << "\n[E stack #" << (stacknum - 7) << "]";
        else
          ss << "\n[F stack #" << (stacknum) << "]";

        stacknum += 1;
      }
      ss << padString(std::to_string(nv.orig), 5, ' ') << ",";
    } else {
      ss << "\n[waited] " << nv.orig;
    }
  }

  ss << "\n]";

  return ss.str();
}

MMAI_NS_END
