#include "main.h"
#include "pyclient.h"

int main(int argc, char * argv[]) {
    std::function<void(int)> wactioncb;

    // NOTE: the .vmap extension may be ommitted
    std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");
    int act(argc > 2 ? std::stoi(std::string(argv[2])) : 150);

    int i = 0;

    // Convert WPyCB -> PyCB
    const MMAI::F_GetAction resultcb = [&i, &wactioncb, &act](const MMAI::Result &result) {
        LOG("resultcb called");
        // if (i < 5)
        boost::thread t([&i, wactioncb, &act]() { wactioncb(act+i); });

        i += 1;
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::ActionCBCB actioncbcb = [&wactioncb](MMAI::F_Action actioncb) {
        wactioncb = [actioncb](int act){ actioncb(act); };
        LOG("actioncbcb called");
    };


    // Convert WPyCBInit -> PyCBInit
    const MMAI::SysCBCB syscbcb = [](MMAI::F_Sys &syscb) {
        LOG("syscbcb called");
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::F_InitReset resetcbcb = [](MMAI::F_Reset &resetcb) {
        LOG("resetcbcb called");
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::F_InitRenderAnsi renderansicbcb = [](MMAI::F_RenderAnsi &renderansicb) {
        LOG("renderansicbcb called");
    };

    auto cbprovider = MMAI::CBProvider{renderansicbcb, resetcbcb, syscbcb, actioncbcb, resultcb};

    // TODO: config values
    std::string resdir = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin";
    // std::string mapname = "simotest.vmap";
    // std::string mapname = "simotest-enchanters.vmap";

    LOG("Start VCMI");
    preinit_vcmi(resdir, "debug");

    // WTF for some reason linker says this is undefined symbol WTF
    // fix: check pyclient.cpp start_vcmi arguments!
    start_vcmi(cbprovider, mapname);

    return 0;
}
