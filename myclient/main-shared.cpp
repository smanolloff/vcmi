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

static MMAI::Export::ActMask lastmask{};

MMAI::Export::Action randomValidAction(const MMAI::Export::ActMask &mask) {
    auto validActions = std::vector<MMAI::Export::Action>{};

    for (int j = 1; j <= mask.size(); j++) {
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
        {"loglevel", "debug"},
        {"attacker-ai", AI_MMAI_USER},
        {"defender-ai", AI_STUPIDAI},
        {"attacker-model", "AI/MMAI/models/model.zip"},
        {"defender-model", "AI/MMAI/models/model.zip"},
    };

    static auto benchmark = false;

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
        ("loglevel", po::value<std::string>()->value_name("<LVL>"),
            values(LOGLEVELS, omap.at("loglevel")).c_str())
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
    static int i = 0;
    static bool rendered = false;

    MMAI::Export::F_GetAction getaction = [](const MMAI::Export::Result * r){
        printf("---------- IN BAGGAGE -------------");
        MMAI::Export::Action act;

        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
            act = randomValidAction(lastmask);
        } else if (r->ended) {
            LOG("user-callback battle ended => sending ACTION_RESET");
            act = MMAI::Export::ACTION_RESET;
        } else if (!rendered) {
            rendered = true;
            lastmask = r->actmask;
            act = MMAI::Export::ACTION_RENDER_ANSI;
        } else {
            rendered = false;
            act = randomValidAction(lastmask);
        }

        LOGSTR("user-callback getAction returning: ", std::to_string(act));
        return act;
    };

    // Reproduce issue with active stack having queuePos=1
    // ai/P2.vmap, MMAI_USER + MMAI_USER (last action is invalid, but does not matter)
    static auto recorded = std::array{592, 612, 692, 82, 232, 1282, 752, 852};

    MMAI::Export::F_GetAction getactionRec = [](const MMAI::Export::Result * r){
        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
        }

        MMAI::Export::Action act;

        if (r->ended) {
            LOG("user-callback battle ended => sending ACTION_RESET");
            if (i < recorded.size()) throw std::runtime_error("Trailing actions");
            act = MMAI::Export::ACTION_RESET;
        } else if (!rendered) {
            rendered = true;
            act = MMAI::Export::ACTION_RENDER_ANSI;
        } else {
            if (i >= recorded.size()) throw std::runtime_error("No more recorded actions");
            act = recorded[i++];
            rendered = false;
        }

        LOGSTR("user-callback getAction returning: ", std::to_string(act));
        return MMAI::Export::Action(act);
    };


    if (benchmark)
        printf("Performance statistics:\n");

    static clock_t t0;
    static unsigned long steps = 0;
    static unsigned long resets = 0;

    MMAI::Export::F_GetAction bench = [](const MMAI::Export::Result * r){
        MMAI::Export::Action act;

        if (steps == 0)
            t0 = clock();

        steps++;

        if (r->ended) {
            resets++;
            switch (resets % 4) {
            case 0: printf("\r|"); break;
            case 1: printf("\r\\"); break;
            case 2: printf("\r-"); break;
            case 3: printf("\r/"); break;
            }

            if (resets == 10) {
                auto s = double(clock() - t0) / CLOCKS_PER_SEC;
                printf("  steps/s: %-6.0f resets/s: %-6.0f\n", steps/s, resets/s);
                resets = 0;
                steps = 0;
                t0 = clock();
            }

            std::cout.flush();

            act = MMAI::Export::ACTION_RESET;
        } else {
            if (i >= MMAI::Export::N_ACTIONS)
                i = 0;
            act = i;
        }

        i++;

        return MMAI::Export::Action(act);
    };


    return {
        benchmark
            ? new MMAI::Export::Baggage(bench)
            // : new MMAI::Export::Baggage(getactionRec),
            : new MMAI::Export::Baggage(getaction),
        gymdir,
        omap.at("map"),
        omap.at("loglevel"),
        omap.at("loglevel"),
        omap.at("attacker-ai"),
        omap.at("defender-ai"),
        omap.at("attacker-model"),
        omap.at("defender-model"),
    };
}
