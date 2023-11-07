#include "main.h"
#include "pyclient.h"

int main(int argc, char * argv[]) {
    std::function<void(int)> wcppcb;

    // NOTE: the .vmap extension may be ommitted
    std::string mapname(argc > 1 ? argv[1] : "simotest.vmap");
    int act(argc > 2 ? std::stoi(std::string(argv[2])) : 150);

    // Convert WPyCB -> PyCB
    const MMAI::PyCB pycb = [&wcppcb, &act](const MMAI::GymResult &gymresult) {
        LOG("pycb called");
        boost::thread([wcppcb, &act]() { wcppcb(act); });
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::PyCBInit pycbinit = [&wcppcb](MMAI::CppCB cppcb) {
        wcppcb = [cppcb](int act){ cppcb(act); };
        LOG("pycbinit called");
    };


    // Convert WPyCBInit -> PyCBInit
    const MMAI::PyCBSysInit pycbsysinit = [](MMAI::CppSysCB &cppsyscb) {
        LOG("pycbsysinit called");
    };

    // Convert WPyCBInit -> PyCBInit
    const MMAI::PyCBResetInit pycbresetinit = [](MMAI::CppResetCB &cppresetcb) {
        LOG("pycbresetinit called");
    };

    auto cbprovider = MMAI::CBProvider{pycbresetinit, pycbsysinit, pycbinit, pycb};

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
