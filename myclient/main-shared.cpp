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

#include "AI/MMAI/export.h"
#include "main.h"
#include "myclient.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <boost/program_options.hpp>
#include <filesystem>


namespace po = boost::program_options;

#define LOG(msg) printf("<%s>[CPP][%s] (%s) %s\n", boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str(), std::filesystem::path(__FILE__).filename().string().c_str(), __FUNCTION__, msg);
#define LOGSTR(msg, a1) printf("<%s>[CPP][%s] (%s) %s\n", boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str(), std::filesystem::path(__FILE__).filename().string().c_str(), __FUNCTION__, (std::string(msg) + a1).c_str());

static std::array<MMAI::Export::ActMask, 2> lastmasks{};

MMAI::Export::Action firstValidAction(const MMAI::Export::ActMask &mask) {
    for (int j = 1; j < mask.size(); j++)
        if (mask[j]) return j;

    return -5;
}


MMAI::Export::Action randomValidAction(const MMAI::Export::ActMask &mask) {
    auto validActions = std::vector<MMAI::Export::Action>{};

    for (int j = 1; j < mask.size(); j++) {
        if (mask[j])
            validActions.push_back(j);
    }

    if (validActions.empty())
        throw std::runtime_error("No valid actions?!");

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, validActions.size() - 1);
    int randomIndex = dist(gen);
    return validActions[randomIndex];
}

MMAI::Export::Action promptAction(const MMAI::Export::ActMask &mask) {
    int num;

    while (true) {
        std::cout << "Enter an integer (blank or 0 for a random valid action): ";

        // Read the user input as a string
        std::string input;
        std::getline(std::cin, input);

        // If the input is empty, treat it as if 0 was entered
        if (input.empty()) {
            num = 0;
            break;
        } else {
            try {
                num = std::stoi(input);
                if (num >= 0)
                    break;
                else
                    std::cerr << "Invalid input!\n";
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid input!\n";
            } catch (const std::out_of_range& e) {
                std::cerr << "Invalid input!\n";
            }
        }
    }

    return num == 0 ? randomValidAction(mask) : MMAI::Export::Action(num);
}

static auto recorded_i = 0;

MMAI::Export::Action recordedAction(std::vector<int> &recording) {
    if (recorded_i >= recording.size()) throw std::runtime_error("\n\n*** No more recorded actions in actions.txt ***\n\n");
    return MMAI::Export::Action(recording[recorded_i++]);
};

// "default" is a reserved word => use "fallback"
std::string values(std::vector<std::string> all, std::string fallback) {
    auto found = false;
    for (int i=0; i<all.size(); i++) {
        if (all[i] == fallback) {
            all[i] = fallback + "*";
            found = true;
        }
    }

    if (!found)
        throw std::runtime_error("Default value '" + fallback + "' not found");

    return "Values: " + boost::algorithm::join(all, " | ");
}

Args parse_args(int argc, char * argv[])
{
    // std::vector<std::string> ais = {"StupidAI", "BattleAI", "MMAI", "MMAI_MODEL"};
    auto omap = std::map<std::string, std::string> {
        {"state-encoding", "default"},
        {"map", "gym/A1.vmap"},
        {"loglevel-global", "error"},
        {"loglevel-ai", "debug"},
        {"loglevel-stats", "error"},
        {"red-ai", AI_MMAI_USER},
        {"blue-ai", AI_STUPIDAI},
        {"red-model", "AI/MMAI/models/model.zip"},
        {"blue-model", "AI/MMAI/models/model.zip"},
        {"stats-mode", "disabled"},
        {"stats-storage", "-"}
    };

    static int maxBattles = 0;
    static int seed = 0;
    static int randomHeroes = 0;
    static int randomObstacles = 0;
    static int swapSides = 0;
    static bool benchmark = false;
    static bool interactive = false;
    static bool prerecorded = false;
    static int statsSampling = 0;
    static int statsPersistFreq = 0;
    static bool printModelPredictions = false;
    static float statsScoreVar = 0.4;

    auto usage = std::stringstream();
    usage << "Usage: " << argv[0] << " [options] <MAP>\n\n";
    usage << "Available options (* denotes default value)";

    auto opts = po::options_description(usage.str(), 0);

    opts.add_options()
        ("help,h", "Show this help")
        ("map", po::value<std::string>()->value_name("<MAP>"),
            ("Path to map (" + omap.at("map") + "*)").c_str())
        ("state-encoding", po::value<std::string>()->value_name("<ENC>"),
            values(ENCODINGS, omap.at("state-encoding")).c_str())
        ("max-battles", po::value<int>()->value_name("<N>"),
            "Quit game after the Nth comat (disabled if 0*)")
        ("seed", po::value<int>()->value_name("<N>"),
            "Seed for the VCMI RNG (random if 0*)")
        ("random-heroes", po::value<int>()->value_name("<N>"),
            "Pick heroes at random each Nth combat (disabled if 0*)")
        ("random-obstacles", po::value<int>()->value_name("<N>"),
            "Place obstacles at random each Nth combat (disabled if 0*)")
        ("swap-sides", po::value<int>()->value_name("<N>"),
            "Swap combat sides each Nth combat (disabled if 0*)")
        ("red-ai", po::value<std::string>()->value_name("<AI>"),
            values(AIS, omap.at("red-ai")).c_str())
        ("blue-ai", po::value<std::string>()->value_name("<AI>"),
            values(AIS, omap.at("blue-ai")).c_str())
        ("red-model", po::value<std::string>()->value_name("<FILE>"),
            ("Path to model.zip (" + omap.at("red-model") + "*)").c_str())
        ("blue-model", po::value<std::string>()->value_name("<FILE>"),
            ("Path to model.zip (" + omap.at("blue-model") + "*)").c_str())
        ("loglevel-global", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel-global")).c_str())
        ("loglevel-ai", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel-ai")).c_str())
        ("loglevel-stats", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel-stats")).c_str())
        ("interactive", po::bool_switch(&interactive),
            "Ask for each action")
        ("prerecorded", po::bool_switch(&prerecorded),
            "Replay actions from local file named actions.txt")
        ("benchmark", po::bool_switch(&benchmark),
            "Measure performance")
        ("print-predictions", po::bool_switch(&printModelPredictions),
            "Print MMAI model predictions (no effect for other AIs)")
        ("stats-mode", po::value<std::string>()->value_name("<MODE>"),
            ("Stats collection mode. " + values(STATPERSPECTIVES, omap.at("stats-mode"))).c_str())
        ("stats-storage", po::value<std::string>()->value_name("<PATH>"),
            "File path to read and persist stats to (use -* for in-memory)")
        ("stats-persist-freq", po::value<int>()->value_name("<N>"),
            "Persist stats to storage file every N battles (read only if 0*)")
        ("stats-sampling", po::value<int>()->value_name("<N>"),
            "Sample random heroes from stats, redistributing every Nth combat (disabled if 0*)")
        ("stats-score-var", po::value<float>()->value_name("<F>"),
            "Limit score variance to a float between 0.1 and 0.4*");

    po::variables_map vm;

    try {
            po::store(po::command_line_parser(argc, argv).options(opts).run(), vm);
            po::notify(vm);
    } catch (const po::error& e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::cout << opts << "\n"; // Display the help message
            exit(1);
    }

    if (vm.count("help")) {
            std::cout << opts << "\n";
            exit(1);
    }

    for (auto &[opt, _] : omap) {
        if (vm.count(opt))
            omap[opt] = vm.at(opt).as<std::string>();
    }

    if (vm.count("max-battles"))
        maxBattles = vm.at("max-battles").as<int>();

    if (vm.count("seed"))
        seed = vm.at("seed").as<int>();

    if (vm.count("random-heroes"))
        randomHeroes = vm.at("random-heroes").as<int>();

    if (vm.count("random-obstacles"))
        randomObstacles = vm.at("random-obstacles").as<int>();

    if (vm.count("swap-sides"))
        swapSides = vm.at("swap-sides").as<int>();

    if (vm.count("stats-persist-freq"))
        statsPersistFreq = vm.at("stats-persist-freq").as<int>();

    if (vm.count("stats-sampling"))
        statsSampling = vm.at("stats-sampling").as<int>();

    if (vm.count("stats-score-var"))
        statsScoreVar = vm.at("stats-score-var").as<float>();

    // The user CB function is hard-coded
    // (no way to provide this from the cmd line args)
    static std::array<bool, 2> renders = {false, false};

    static clock_t t0;
    static unsigned long steps = 0;
    static unsigned long resets = 0;
    static int benchside = -1;

    static std::vector<int> recordings;
    if (prerecorded) {
        std::ifstream inputFile("actions.txt"); // Assuming the integers are stored in a file named "input.txt"
        if (!inputFile.is_open()) throw std::runtime_error("Failed to open actions.txt");
        int num;
        while (inputFile >> num) {
            std::cout << "Loaded action: " << num << "\n";
            recordings.push_back(num);
        }
    }

    MMAI::Export::F_GetAction getaction = [](const MMAI::Export::Result * r){
        MMAI::Export::Action act;
        auto side = static_cast<int>(r->side);

        if (steps == 0 && benchmark) {
            t0 = clock();
            benchside = side;
        }

        steps++;

        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
            // use stored mask from pre-render result
            act = interactive
                ? promptAction(lastmasks.at(side))
                : (prerecorded ? recordedAction(recordings) : randomValidAction(lastmasks.at(side)));

            renders.at(side) = false;
        } else if (r->ended) {
            if (side == benchside) {
                resets++;

                switch (resets % 4) {
                case 0: printf("\r|"); break;
                case 1: printf("\r\\"); break;
                case 2: printf("\r-"); break;
                case 3: printf("\r/"); break;
                }

                if (resets == 10) {
                    auto s = double(clock() - t0) / CLOCKS_PER_SEC;
                    printf("  steps/s: %-6.0f resets/s: %-6.2f\n", steps/s, resets/s);
                    resets = 0;
                    steps = 0;
                    t0 = clock();
                }

                std::cout.flush();
            }

            if (!benchmark) LOG("user-callback battle ended => sending ACTION_RESET");
            act = MMAI::Export::ACTION_RESET;
        // } else if (false)
        } else if (!benchmark && !renders.at(side)) {
            renders.at(side) = true;
            // store mask of this result for the next action
            lastmasks.at(side) = r->actmask;
            act = MMAI::Export::ACTION_RENDER_ANSI;
        } else {
            renders.at(side) = false;
            act = interactive
                ? promptAction(r->actmask)
                : (prerecorded ? recordedAction(recordings) : randomValidAction(r->actmask));
        }

        if (printModelPredictions && !benchmark) LOGSTR("user-callback getAction returning: ", std::to_string(act));
        return act;
    };

    if (benchmark) {
        if (
            (omap.at("red-ai") == "StupidAI" && omap.at("blue-ai") == "StupidAI") ||
            (omap.at("red-ai") == "StupidAI" && omap.at("blue-ai") == "BattleAI") ||
            (omap.at("red-ai") == "BattleAI" && omap.at("blue-ai") == "StupidAI") ||
            (omap.at("red-ai") == "BattleAI" && omap.at("blue-ai") == "BattleAI")
        ) {
            printf("--benchmark requires at least one AI of type MMAI_USER or MMAI_MODEL.\n");
            exit(1);
        }

        printf("Benchmark:\n");
        printf("* Map: %s\n", omap.at("map").c_str());
        printf("* Attacker AI: %s", omap.at("red-ai").c_str());
        omap.at("red-ai") == "MMAI_MODEL"
            ? printf(" %s\n", omap.at("red-model").c_str())
            : printf("\n");

        printf("* Defender AI: %s", omap.at("blue-ai").c_str());
        omap.at("blue-ai") == "MMAI_MODEL"
            ? printf(" %s\n", omap.at("blue-model").c_str())
            : printf("\n");

        printf("\n");
    }

    return {
        new MMAI::Export::Baggage(getaction),
        omap.at("state-encoding") == "default"
            ? MMAI::Export::STATE_ENCODING_DEFAULT
            : MMAI::Export::STATE_ENCODING_FLOAT,
        omap.at("map"),
        maxBattles,
        seed,
        randomHeroes,
        randomObstacles,
        swapSides,
        omap.at("loglevel-global"),
        omap.at("loglevel-ai"),
        omap.at("loglevel-stats"),
        omap.at("red-ai"),
        omap.at("blue-ai"),
        omap.at("red-model"),
        omap.at("blue-model"),
        omap.at("stats-mode"),
        omap.at("stats-storage"),
        statsPersistFreq,
        statsSampling,
        statsScoreVar,
        printModelPredictions,
    };
}
