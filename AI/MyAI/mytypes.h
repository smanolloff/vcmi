#pragma once

// *** THIS FILE LIVES IN:
// ***
// *** vcmi/AI/MyAI/mytypes.h

#include <cassert>
#include <array>
#include <string>

namespace MMAI {

#ifndef DLL_EXPORT
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

// Actions:
// 0, 1, 2 - retreat, defend, wait
// 3..1322 - 165 hexes * 8 actions each
static const int N_ACTIONS = 1323; // 0..1322
static const int ACTION_UNKNOWN = 65535; // eg. when unset

// State:
// 165 hex + (14 stack * 12 attrs) + current_stack
static const int STATE_SIZE = 334;

// Arbitary int value normalized as -1..1 float
extern "C" struct DLL_EXPORT NValue {
    int orig;
    float norm;
    std::string name;

    NValue() : name(""), orig(0), norm(-1) {};
    NValue(std::string n, int v, int vmin, int vmax) {
        assert(vmin < vmax);

        if (v < vmin) {
            // TODO: log warning
            v = vmin;
        } else if (v > vmax) {
            // TODO: log warning
            v = vmax;
        }

        name = n;
        orig = v;
        norm = 2.0 * static_cast<float>(v - vmin) / (vmax - vmin) - 1.0;
    }
};

// SYNC WITH VcmiEnv.observation_space
using GymState = std::array<NValue, STATE_SIZE>;
struct GymResult {
    GymState state = {};
    int n_errors = 0;
    int dmgDealt = 0;
    int dmgReceived = 0;
    int unitsLost = 0;
    int unitsKilled = 0;
    int valueLost = 0;
    int valueKilled = 0;
    bool ended = false;
    bool victory = false;

    // set to true only for last observation (at battle end)
    bool nostate = false;
};

using GymAction = uint16_t;

// CppResetCB is a CPP function given to the GymEnv via PyCBResetInit (see below)
// GymEnv will invoke it on every "reset()" call
using CppResetCB = const std::function<void()>;

// PyCBResetInit is a Python function passed to VCMI entrypoint.
// VCMI will invoke it once, with 1 argument: a CppResetCB (see above)
using PyCBResetInit = const std::function<void(CppResetCB)>;

// CppCB is a CPP function given to the GymEnv via PyCBSysInit (see below)
// GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
using CppSysCB = const std::function<void(std::string cmd)>;

// PyCBSysInit is a Python function passed to VCMI entrypoint.
// VCMI will invoke it once, with 1 argument: a CppSysCB (see above)
using PyCBSysInit = const std::function<void(CppSysCB)>;

// CppCB is a CPP function given to the GymEnv via PyCBInit (see below)
// GymEnv will invoke it on every "step()" call, with 1 argument: an GymAction
using CppCB = const std::function<void(const GymAction &action)>;

// PyCBInit is a Python function passed to the AI constructor.
// AI constructor will invoke it once, with 1 argument: a CppCB (see above)
using PyCBInit = const std::function<void(CppCB)>;

// PyCB is a Python function passed to the AI constructor.
// AI will invoke it on every "yourTurn()" call, with 1 argument: a GymState
using PyCB = const std::function<void(const GymResult &result)>;

// The PyCB functions above are all bundled into CBProvider struct
// whose purpose is to be seamlessly transportable through VCMI code
// as a std::any object, then cast back to CBProvider in the AI constructor
extern "C" struct DLL_EXPORT CBProvider {
    CBProvider(
        const PyCBResetInit pycbresetinit_,
        const PyCBSysInit pycbsysinit_,
        const PyCBInit pycbinit_,
        const PyCB pycb_
    ) : pycbresetinit(pycbresetinit_),
        pycbsysinit(pycbsysinit_),
        pycbinit(pycbinit_),
        pycb(pycb_) {}

    const PyCBResetInit pycbresetinit;
    const PyCBSysInit pycbsysinit;
    const PyCBInit pycbinit;
    const PyCB pycb;
};

} // namespace MMAI
