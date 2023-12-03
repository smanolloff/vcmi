#include "main.h"
#include "pyclient.h"
#include "pyclient/aitypes.h"
#include <cstdio>

int main(int argc, char * argv[]) {
    std::function<void(int)> wactioncb;

    // NOTE: the .vmap extension may be ommitted
    std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");
    int act(argc > 2 ? std::stoi(std::string(argv[2])) : 35);

    int i = act;

    MMAIExport::F_GetAction getaction = [&i](const MMAIExport::Result * r){
        if (r->type == MMAIExport::ResultType::ANSI_RENDER) {
            std::cout << r->ansiRender << "\n";
        }

        return (i % 2 == 0)
            ? MMAIExport::ACTION_RENDER_ANSI
            : MMAIExport::Action(i++);
    };

    auto cbprovider = MMAIExport::CBProvider(getaction);

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
