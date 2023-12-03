#include "main.h"
#include "pyclient.h"
#include <cstdio>

int main(int argc, char * argv[]) {
    std::function<void(int)> wactioncb;

    // NOTE: the .vmap extension may be ommitted
    std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");
    int act(argc > 2 ? std::stoi(std::string(argv[2])) : 35);

    int i = act;

    MMAI::
    Export::F_GetAction getaction = [&i](const MMAI::Export::Result * r){
        if (r->type == MMAI::Export::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
        }

        return (i % 2 == 0)
            ? MMAI::Export::ACTION_RENDER_ANSI
            : MMAI::Export::Action(i++);
    };

    auto cbprovider = MMAI::Export::CBProvider(getaction);

    // TODO: config values
    std::string resdir = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin";
    // std::string mapname = "simotest.vmap";
    // std::string mapname = "simotest-enchanters.vmap";

    LOG("Start VCMI");
    init_vcmi(resdir, "trace", "trace", &cbprovider);

    // WTF for some reason linker says this is undefined symbol WTF
    // fix: check pyclient.cpp start_vcmi arguments!
    start_vcmi(mapname);

    return 0;
}
