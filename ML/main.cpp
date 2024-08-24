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

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <boost/program_options.hpp>
#include <boost/core/demangle.hpp>
#include <filesystem>

#include "AI/MMAI/common.h"
#include "AI/MMAI/schema/base.h"
#include "ML/model_wrappers/function.h"
#include "ML/model_wrappers/scripted.h"
#include "MLClient.h"

#include "AI/MMAI/schema/schema.h"
#include "AI/MMAI/schema/v1/types.h"
#include "AI/MMAI/schema/v1/constants.h"

#include "MMAILoader/TorchModel.h"
#include "user_agents/base.h"
#include "user_agents/agent-v1.h"
#include "user_agents/agent-v3.h"
#include "user_agents/agent-v4.h"


namespace po = boost::program_options;

#define LOG(msg) printf("<%s>[CPP][%s] (%s) %s\n", boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str(), std::filesystem::path(__FILE__).filename().string().c_str(), __FUNCTION__, msg);
#define LOGSTR(msg, a1) printf("<%s>[CPP][%s] (%s) %s\n", boost::lexical_cast<std::string>(boost::this_thread::get_id()).c_str(), std::filesystem::path(__FILE__).filename().string().c_str(), __FUNCTION__, (std::string(msg) + a1).c_str());

namespace ML {
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

    InitArgs parse_args(int argc, char * argv[]) {
        int maxBattles = 0;
        int seed = 0;
        int randomHeroes = 0;
        int randomObstacles = 0;
        int townChance = 0;
        int warmachineChance = 0;
        int manaMin = 0;
        int manaMax = 0;
        int swapSides = 0;
        bool benchmark = false;
        bool interactive = false;
        bool prerecorded = false;
        int statsTimeout = 60000;
        int statsPersistFreq = 0;
        bool printModelPredictions = false;
        bool headless = false;

        // std::vector<std::string> ais = {"StupidAI", "BattleAI", "MMAI", "MMAI_MODEL"};
        auto omap = std::map<std::string, std::string> {
            {"map", "gym/A1.vmap"},
            {"loglevel-global", "error"},
            {"loglevel-ai", "warn"},
            {"loglevel-stats", "warn"},
            {"left-ai", AI_MMAI_USER},
            {"right-ai", AI_STUPIDAI},
            {"left-model", "AI/MMAI/models/model.zip"},
            {"right-model", "AI/MMAI/models/model.zip"},
            {"stats-mode", "disabled"},
            {"stats-storage", "-"}
        };

        auto usage = std::stringstream();
        usage << "Usage: " << argv[0] << " [options]\n\n";
        usage << "Available options (* denotes default value)";

        auto opts = po::options_description(usage.str(), 0);

        opts.add_options()
            ("help,h", "Show this help")
            ("headless", po::bool_switch(&headless),
                "Disable GUI (run in headless mode)")
            ("map", po::value<std::string>()->value_name("<MAP>"),
                ("Path to map (" + omap.at("map") + "*)").c_str())
            ("max-battles", po::value<int>()->value_name("<N>"),
                "Quit game after the Nth comat (disabled if 0*)")
            ("seed", po::value<int>()->value_name("<N>"),
                "Seed for the VCMI RNG (random if 0*)")
            ("random-heroes", po::value<int>()->value_name("<N>"),
                "Pick heroes at random each Nth combat (disabled if 0*)")
            ("random-obstacles", po::value<int>()->value_name("<N>"),
                "Place obstacles at random each Nth combat (disabled if 0*)")
            ("town-chance", po::value<int>()->value_name("<N>"),
                "Percent chance to have the combat in a town (no town combat if 0*)")
            ("warmachine-chance", po::value<int>()->value_name("<N>"),
                "Percent chance to add ballista/tent/cart in combat (no war machines if 0*)")
            ("mana-min", po::value<int>()->value_name("<N>"),
                "Minimum mana to give to give each hero at the start of combat (default 0*)")
            ("mana-max", po::value<int>()->value_name("<N>"),
                "Maximum mana to give to give each hero at the start of combat (default 100*)")
            ("swap-sides", po::value<int>()->value_name("<N>"),
                "Swap combat sides each Nth combat (disabled if 0*)")
            ("left-ai", po::value<std::string>()->value_name("<AI>"),
                values(AIS, omap.at("left-ai")).c_str())
            ("right-ai", po::value<std::string>()->value_name("<AI>"),
                values(AIS, omap.at("right-ai")).c_str())
            ("left-model", po::value<std::string>()->value_name("<FILE>"),
                ("Path to model.zip (" + omap.at("left-model") + "*)").c_str())
            ("right-model", po::value<std::string>()->value_name("<FILE>"),
                ("Path to model.zip (" + omap.at("right-model") + "*)").c_str())
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
            ("stats-timeout", po::value<int>()->value_name("<N>"),
                "Timeout in ms for obtaining a DB lock in stats storage (default 60000*)")
            ("stats-persist-freq", po::value<int>()->value_name("<N>"),
                "Persist stats to storage file every N battles (read only if 0*)");

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

        if (vm.count("town-chance"))
            townChance = vm.at("town-chance").as<int>();

        if (vm.count("warmachine-chance"))
            warmachineChance = vm.at("warmachine-chance").as<int>();

        if (vm.count("mana-min"))
            manaMin = vm.at("mana-min").as<int>();

        if (vm.count("mana-max"))
            manaMax = vm.at("mana-max").as<int>();

        if (vm.count("swap-sides"))
            swapSides = vm.at("swap-sides").as<int>();

        if (vm.count("stats-timeout"))
            statsTimeout = vm.at("stats-timeout").as<int>();

        if (vm.count("stats-persist-freq"))
            statsPersistFreq = vm.at("stats-persist-freq").as<int>();


        std::vector<int> recordings = {};

        if (prerecorded) {
            std::ifstream inputFile("actions.txt"); // Assuming the integers are stored in a file named "input.txt"
            if (!inputFile.is_open()) throw std::runtime_error("Failed to open actions.txt");
            int num;
            while (inputFile >> num) {
                std::cout << "Loaded action: " << num << "\n";
                recordings.push_back(num);
            }
        }

        if (benchmark) {
            if (
                omap.at("left-ai") != AI_MMAI_USER &&
                omap.at("left-ai") != AI_MMAI_MODEL &&
                omap.at("right-ai") != AI_MMAI_USER &&
                omap.at("right-ai") != AI_MMAI_MODEL
            ) {
                printf("--benchmark requires at least one AI of type MMAI_USER or MMAI_MODEL.\n");
                exit(1);
            }

            printf("Benchmark:\n");
            printf("* Map: %s\n", omap.at("map").c_str());
            printf("* Attacker AI: %s", omap.at("left-ai").c_str());
            omap.at("left-ai") == AI_MMAI_MODEL
                ? printf(" %s\n", omap.at("left-model").c_str())
                : printf("\n");

            printf("* Defender AI: %s", omap.at("right-ai").c_str());
            omap.at("right-ai") == AI_MMAI_MODEL
                ? printf(" %s\n", omap.at("right-model").c_str())
                : printf("\n");

            printf("\n");
        }

        auto leftAi = omap.at("left-ai");
        auto rightAi = omap.at("right-ai");

        MMAI::Schema::IModel * leftModel;
        MMAI::Schema::IModel * rightModel;

        auto autorender = true;

        if (leftAi == AI_MMAI_USER) {
            leftModel = new UserAgents::AgentV4(benchmark, interactive, autorender, printModelPredictions, recordings);
            // prevent double render if both models are MMAI_USER
            autorender = false;
        } else if (leftAi == AI_MMAI_MODEL) {
            leftModel = new MMAI::TorchModel(omap.at("left-model"), false);
        } else {
            leftModel = new ModelWrappers::Scripted(leftAi);
        }

        if (rightAi == AI_MMAI_USER) {
            rightModel = new UserAgents::AgentV4(benchmark, interactive, autorender, printModelPredictions, recordings);
        } else if (rightAi == AI_MMAI_MODEL) {
            rightModel = new MMAI::TorchModel(omap.at("right-model"), false);
        } else {
            rightModel = new ModelWrappers::Scripted(rightAi);
        }

        return InitArgs(
            omap.at("map"),
            leftModel,
            rightModel,
            maxBattles,
            seed,
            randomHeroes,
            randomObstacles,
            townChance,
            warmachineChance,
            manaMin,
            manaMax,
            swapSides,
            omap.at("loglevel-global"),
            omap.at("loglevel-ai"),
            omap.at("loglevel-stats"),
            omap.at("stats-mode"),
            omap.at("stats-storage"),
            statsTimeout,
            statsPersistFreq,
            headless
        );
    }
}

int main(int argc, char * argv[]) {
    auto initargs = ML::parse_args(argc, argv);
    ML::init_vcmi(initargs);
    ML::start_vcmi();
    return 0;
}
