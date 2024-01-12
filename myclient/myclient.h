// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

#pragma once
#include <string>
#include <functional>
#include "mmai_export.h"

constexpr auto AI_STUPIDAI = "StupidAI";
constexpr auto AI_BATTLEAI = "BattleAI";
constexpr auto AI_MMAI_USER = "MMAI_USER"; // for user-provided getAction (gym)
constexpr auto AI_MMAI_MODEL = "MMAI_MODEL"; // for pre-trained model's getAction

const std::vector<std::string> AIS = {
    AI_STUPIDAI,
    AI_BATTLEAI,
    AI_MMAI_USER,
    AI_MMAI_MODEL
};

const std::vector<std::string> LOGLEVELS = {"trace", "debug", "info", "warn", "error"};

void __attribute__((visibility("default"))) init_vcmi(
    MMAI::Export::Baggage* baggage,
    std::string gymdir,
    std::string mapname,
    std::string loglevelGlobal,
    std::string loglevelAI,
    std::string attackerAI,
    std::string defenderAI,
    std::string attackerModel,
    std::string defenderModel,
    bool headless
);

void __attribute__((visibility("default"))) start_vcmi();

[[noreturn]] void handleFatalError(const std::string & message, bool terminate);
