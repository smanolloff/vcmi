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

#include "gymclient/gymclient.h"
#include "main.h"

int main(int argc, char * argv[]) {
    auto [
        baggage_,
        maxBattles,
        seed,
        randomHeroes,
        randomObstacles,
        townChance,
        warmachineChance,
        swapSides,
        loglevelGlobal,
        loglevelAI,
        loglevelStats,
        redAI,
        blueAI,
        redModel,
        blueModel,
        statsMode,
        statsStorage,
        statsPersistFreq,
        statsSampling,
        statsScoreVar,
        printModelPredictions,
        trueRng
    ] = parse_args(argc, argv);

    init_vcmi(
        baggage_,
        maxBattles,
        seed,
        randomHeroes,
        randomObstacles,
        townChance,
        warmachineChance,
        swapSides,
        loglevelGlobal,
        loglevelAI,
        loglevelStats,
        redAI,
        blueAI,
        redModel,
        blueModel,
        statsMode,
        statsStorage,
        statsPersistFreq,
        statsSampling,
        statsScoreVar,
        printModelPredictions,
        trueRng,
        true
    );

    start_vcmi();
    return 0;
}

