#include "main.h"
#include "pyclient.h"
#include <cstdio>

int main(int argc, char * argv[]) {
    std::function<void(int)> wactioncb;

    // int act(argc > 2 ? std::stoi(std::string(argv[2])) : 35);

    // NOTE: the .vmap extension may be ommitted
    std::string mapname = "ai/M8.vmap";
    auto actions = std::array{
        1,
        1,
        1,
        602,
        2,
        1502,
        1082,
        1,
        1,
        2,
        602,
        1502,
        936,
    };

    if (argc > 1) mapname = argv[1];

    int i = 0;
    bool rendered = false;

    MMAI::Export::F_GetAction getaction = [&i, &actions, &rendered](const MMAI::Export::Result * r){
        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
        }

        if (i == actions.size())
            throw std::runtime_error("No more actions");

        if (i % 2 == 0 && !rendered) {
            rendered = true;
            return MMAI::Export::ACTION_RENDER_ANSI;
        }

        rendered = false;
        return MMAI::Export::Action(actions[i++]);
    };

    auto cbprovider = MMAI::Export::CBProvider(getaction);

    // TODO: config values
    std::string resdir = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin";
    // std::string mapname = "simotest.vmap";
    // std::string mapname = "simotest-enchanters.vmap";

    LOG("Start VCMI");
    init_vcmi(resdir, "trace", "trace", "", "", &cbprovider);

    // WTF for some reason linker says this is undefined symbol WTF
    // fix: check pyclient.cpp start_vcmi arguments!
    start_vcmi(mapname);

    return 0;
}
