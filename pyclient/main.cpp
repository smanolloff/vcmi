#include "main.h"
#include "pyclient.h"

int main(int argc, char * argv[]) {
    std::function<void(int)> wactioncb;

    // NOTE: the .vmap extension may be ommitted
    std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");
    int act(argc > 2 ? std::stoi(std::string(argv[2])) : 150);

    int i = 0;

    // Convert WPyCB -> PyCB
    const MMAI::ResultCB resultcb = [&i, &wactioncb, &act](const MMAI::Result &result) {
        LOG("resultcb called");
        // if (i < 5)
        boost::thread t([&i, wactioncb, &act]() { wactioncb(act+i); });

        i += 1;
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::ActionCBCB actioncbcb = [&wactioncb](MMAI::ActionCB actioncb) {
        wactioncb = [actioncb](int act){ actioncb(act); };
        LOG("actioncbcb called");
    };


    // Convert WPyCBInit -> PyCBInit
    const MMAI::SysCBCB syscbcb = [](MMAI::SysCB &syscb) {
        LOG("syscbcb called");
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::ResetCBCB resetcbcb = [](MMAI::ResetCB &resetcb) {
        LOG("resetcbcb called");
    };

    auto cbprovider = MMAI::CBProvider{resetcbcb, syscbcb, actioncbcb, resultcb};

    // TODO: config values
    std::string resdir = "/Users/simo/Projects/vcmi-gym/vcmi_gym/envs/v0/vcmi/build/bin";
    // std::string mapname = "simotest.vmap";
    // std::string mapname = "simotest-enchanters.vmap";

    LOG("Start VCMI");
    preinit_vcmi(resdir);

    // WTF for some reason linker says this is undefined symbol WTF
    start_vcmi(mapname, cbprovider);

    return 0;
}
