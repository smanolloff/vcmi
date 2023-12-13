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
        {"defender-model", "AI/MMAI/models/model.zip"}
    };

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
            ("Path to model.zip (" + omap.at("defender-model") + "*)").c_str());

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
    std::string resdir(gymdir + "/vcmi_gym/envs/v0/vcmi/build/bin");

    // The user CB function is hard-coded
    // (no way to provide this from the cmd line args)
    static int i = -1;
    bool rendered = false;

    MMAI::Export::F_GetAction getaction = [&rendered](const MMAI::Export::Result * r){
        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
        }

        if (i % 2 == 0 && !rendered) {
            rendered = true;
            return MMAI::Export::ACTION_RENDER_ANSI;
        }

        rendered = false;

        if (++i == MMAI::Export::N_ACTIONS) {
            i = 2;
            // i = 0; => results in sporadic retreats

        }

        auto act = i;

        // Uncomment to disable sporadic retreats

        // MMAI::AAI will expect a RESET action here
        if (r->ended) {
            LOG("user-callback battle ended => sending ACTION_RESET");
            act = MMAI::Export::ACTION_RESET;
        }

        LOGSTR("user-callback getAction returning: ", std::to_string(act));
        return MMAI::Export::Action(act);
    };

    return {
        new MMAI::Export::Baggage(getaction),
        gymdir,
        resdir,
        omap.at("map"),
        omap.at("loglevel"),
        omap.at("loglevel"),
        omap.at("attacker-ai"),
        omap.at("defender-ai"),
        omap.at("attacker-model"),
        omap.at("defender-model"),
    };
}
