#pragma once
#include <string>
#include <functional>
#include "aitypes.h"

extern "C" void __attribute__((visibility("default"))) preinit_vcmi(
  std::string resdir,
  std::string loglevel
);

extern "C" void __attribute__((visibility("default"))) start_vcmi(
  MMAI::CBProvider cbprovider,
  std::string mapname
);
