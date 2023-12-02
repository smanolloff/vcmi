#pragma once
#include <memory>
#include <string>
#include <functional>
#include "aitypes.h"

const MMAIExport::F_Sys __attribute__((visibility("default"))) init_vcmi(
  std::string resdir,
  std::string loglevelGlobal,
  std::string loglevelAI,
  MMAIExport::CBProvider * cbprovider
);

void __attribute__((visibility("default"))) start_vcmi(std::string mapname);
