#pragma once
#include <string>
#include <functional>
#include "AI/MMAI/export.h"

constexpr auto AI_STUPIDAI = "StupidAI";
constexpr auto AI_BATTLEAI = "BattleAI";
// means use user-provided getAction() (used in training)
constexpr auto AI_MMAI_USER = "MMAI_USER";
// means use dynamically loaded ConnectorLoader_getAction()
constexpr auto AI_MMAI_MODEL = "MMAI_MODEL";

int mymain(
    MMAI::Export::Baggage* baggage,
    std::string resdir,
    std::string mapname,
    std::string loglevelGlobal,
    std::string loglevelAI,
    std::string attackerAI,
    std::string defenderAI,
    std::string attackerModel,
    std::string defenderModel
);

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
