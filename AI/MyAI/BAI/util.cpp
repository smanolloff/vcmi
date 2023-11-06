#include "BAI.h"

MMAI_NS_BEGIN

std::string BAI::gymaction_str(const GymAction &ga) {
  return std::to_string(ga);
}

std::string BAI::gymstate_str(const GymState &gs) {
  return std::to_string(gs[0].orig) + " " + std::to_string(gs[1].orig) + " " + std::to_string(gs[2].orig);
}

MMAI_NS_END
