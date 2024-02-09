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
    std::cout << "Enter an integer (0 for random valid action): ";
    std::cin >> num;

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
        {"map", "ai/P1.vmap"},
        {"loglevel-global", "error"},
        {"loglevel-ai", "debug"},
        {"attacker-ai", AI_MMAI_USER},
        {"defender-ai", AI_STUPIDAI},
        {"attacker-model", "AI/MMAI/models/model.zip"},
        {"defender-model", "AI/MMAI/models/model.zip"},
    };

    static auto benchmark = false;
    static auto interactive = false;
    static auto prerecorded = false;

    auto usage = std::stringstream();
    usage << "Usage: " << argv[0] << " [options] <MAP>\n\n";
    usage << "Available options (* denotes default value)";

    auto opts = po::options_description(usage.str(), 0);

    opts.add_options()
        ("help,h", "Show this help")
        ("map", po::value<std::string>()->value_name("<MAP>"),
            ("Path to map (" + omap.at("map") + "*)").c_str())
        ("attacker-ai", po::value<std::string>()->value_name("<AI>"),
            values(AIS, omap.at("attacker-ai")).c_str())
        ("defender-ai", po::value<std::string>()->value_name("<AI>"),
            values(AIS, omap.at("defender-ai")).c_str())
        ("attacker-model", po::value<std::string>()->value_name("<FILE>"),
            ("Path to model.zip (" + omap.at("attacker-model") + "*)").c_str())
        ("defender-model", po::value<std::string>()->value_name("<FILE>"),
            ("Path to model.zip (" + omap.at("defender-model") + "*)").c_str())
        ("loglevel-global", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel-global")).c_str())
        ("loglevel-ai", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel-ai")).c_str())
        ("interactive", po::bool_switch(&interactive),
            ("Ask for each action"))
        ("prerecorded", po::bool_switch(&prerecorded),
            ("Replay actions from local file named actions.txt"))
        ("benchmark", po::bool_switch(&benchmark),
            ("Measure performance"));

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

        // std::cout << opt << ": " << omap.at(opt) << "\n";
    }

    std::string gymdir("/Users/simo/Projects/vcmi-gym");

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
        } else if (!benchmark && !renders.at(side)) {
            renders[side] = true;
            // store mask of this result for the next action
            lastmasks.at(side) = r->actmask;
            act = MMAI::Export::ACTION_RENDER_ANSI;
        } else {
            renders.at(side) = false;
            act = interactive
                ? promptAction(r->actmask)
                : (prerecorded ? recordedAction(recordings) : randomValidAction(r->actmask));
        }

        if (!benchmark) LOGSTR("user-callback getAction returning: ", std::to_string(act));
        return act;
    };

    if (benchmark) {
        printf("Benchmark:\n");
        printf("* Map: %s\n", omap.at("map").c_str());
        printf("* Attacker AI: %s", omap.at("attacker-ai").c_str());
        omap.at("attacker-ai") == "MMAI_MODEL"
            ? printf(" %s\n", omap.at("attacker-model").c_str())
            : printf("\n");

        printf("* Defender AI: %s", omap.at("defender-ai").c_str());
        omap.at("defender-ai") == "MMAI_MODEL"
            ? printf(" %s\n", omap.at("defender-model").c_str())
            : printf("\n");

        printf("\n");
    }

    return {
        new MMAI::Export::Baggage(getaction),
        gymdir,
        omap.at("map"),
        omap.at("loglevel-global"),
        omap.at("loglevel-ai"),
        omap.at("attacker-ai"),
        omap.at("defender-ai"),
        omap.at("attacker-model"),
        omap.at("defender-model"),
    };
}
