#pragma once
#include <string>
#include <functional>
#include "aitypes.h"

extern "C" void __attribute__((visibility("default"))) preinit_vcmi(
  std::string resdir
);

extern "C" void __attribute__((visibility("default"))) start_vcmi(
  std::string mapname,
  CBProvider cbprovider
);
