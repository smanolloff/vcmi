#pragma once

// *** THIS FILE LIVES IN:
// ***
// *** vcmi/AI/MyAI/mytypes.h

#include <array>

namespace MMAI {

#ifndef DLL_EXPORT
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

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

using GymAction = uint16_t;
using GymState = std::array<NValue, 334>;

// CppCB is a CPP function given to the GymEnv via PyCBSysInit (see below)
// GymEnv will invoke it on every "reset()" or "close()" calls
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
using PyCB = const std::function<void(const GymState &state)>;

// The PyCB functions above are all bundled into CBProvider struct
// whose purpose is to be seamlessly transportable through VCMI code
// as a std::any object, then cast back to CBProvider in the AI constructor
extern "C" struct DLL_EXPORT CBProvider {
    CBProvider(const PyCBSysInit pycbsysinit_, const PyCBInit pycbinit_, const PyCB pycb_)
    : pycbsysinit(pycbsysinit_), pycbinit(pycbinit_), pycb(pycb_) {}

    const PyCBSysInit pycbsysinit;
    const PyCBInit pycbinit;
    const PyCB pycb;
};

} // namespace MMAI

//Accessibility is property of hex in battle. It doesn't depend on stack, side's perspective and so on.
enum class HexState : int
{
    FREE_REACHABLE,     // 0
    FREE_UNREACHABLE,   // 1
    OBSTACLE,           // 2
    FRIENDLY_STACK_1,   // 3
    FRIENDLY_STACK_2,   // 4
    FRIENDLY_STACK_3,   // 5
    FRIENDLY_STACK_4,   // 6
    FRIENDLY_STACK_5,   // 7
    FRIENDLY_STACK_6,   // 8
    FRIENDLY_STACK_7,   // 9
    ENEMY_STACK_1,      // 10
    ENEMY_STACK_2,      // 11
    ENEMY_STACK_3,      // 12
    ENEMY_STACK_4,      // 13
    ENEMY_STACK_5,      // 14
    ENEMY_STACK_6,      // 15
    ENEMY_STACK_7,      // 16
    count
};
