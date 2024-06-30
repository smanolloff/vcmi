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

#include "AI/MMAI/schema/schema.h"

using Args = std::tuple<
    MMAI::Schema::Baggage*,
    int,         // maxBattles
    int,         // seed
    int,         // randomHeroes
    int,         // randomObstacles
    int,         // townChance
    int,         // warmachineChance
    int,         // swapSides
    std::string, // loglevelGlobal
    std::string, // loglevelAI
    std::string, // loglevelStats
    std::string, // redAI
    std::string, // blueAI
    std::string, // redModel
    std::string, // blueModel
    std::string, // statsMode
    std::string, // statsStorage
    int,         // statsPersistFreq
    int,         // statsSampling
    float,       // statsScoreVar
    bool,        // printModelPredictions
    bool         // trueRng
>;

Args parse_args(int argc, char * argv[]);
