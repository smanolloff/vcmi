#pragma once
#include <string>
#include <functional>
#include "callbacks.h"

extern "C" void __attribute__((visibility("default"))) start_vcmi(
  std::string resdir,
  std::string mapname,
  CBProvider cbprovider,
);
