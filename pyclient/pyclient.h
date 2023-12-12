#pragma once
#include <memory>
#include <string>
#include <functional>
#include "mmai_export.h"

const MMAI::Export::F_Sys __attribute__((visibility("default"))) init_vcmi(
  std::string resdir,
  std::string loglevelGlobal,
  std::string loglevelAI,
  std::string enemyAImodel,
  std::string enemyAItype,
  MMAI::Export::Baggage * baggage
);

void __attribute__((visibility("default"))) start_vcmi(std::string mapname);
