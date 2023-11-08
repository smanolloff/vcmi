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
using State = std::array<NValue, STATE_SIZE>;
struct Result {
    State state = {};
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

using Action = uint16_t;

// RenderAnsiCB is a CPP function given to the GymEnv via ResetCBCB (see below)
// GymEnv will invoke it on every "render()" call
using RenderAnsiCB = const std::function<std::string()>;

// RenderAnsiCBCB is a Python function passed to VCMI entrypoint.
// VCMI will invoke it once, with 1 argument: a RenderAnsiCB (see above)
using RenderAnsiCBCB = const std::function<void(RenderAnsiCB)>;

// ResetCB is a CPP function given to the GymEnv via ResetCBCB (see below)
// GymEnv will invoke it on every "reset()" call
using ResetCB = const std::function<void()>;

// ResetCBCB is a Python function passed to VCMI entrypoint.
// VCMI will invoke it once, with 1 argument: a ResetCB (see above)
using ResetCBCB = const std::function<void(ResetCB)>;

// SysCB is a CPP function given to the GymEnv via SysCBCB (see below)
// GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
using SysCB = const std::function<void(std::string cmd)>;

// SysCBCB is a Python function passed to VCMI entrypoint.
// VCMI will invoke it once, with 1 argument: a SysCB (see above)
using SysCBCB = const std::function<void(SysCB)>;

// ActionCB is a CPP function given to the GymEnv via ActionCBCB (see below)
// GymEnv will invoke it on every "step()" call, with 1 argument: an Action
using ActionCB = const std::function<void(const Action &action)>;

// ActionCBCB is a Python function passed to the AI constructor.
// AI constructor will invoke it once, with 1 argument: a ActionCB (see above)
using ActionCBCB = const std::function<void(ActionCB)>;

// ResultCB is a Python function passed to the AI constructor.
// AI will invoke it on every "yourTurn()" call, with 1 argument: a Result
using ResultCB = const std::function<void(const Result &result)>;

// The CB functions above are all bundled into CBProvider struct
// whose purpose is to be seamlessly transportable through VCMI code
// as a std::any object, then cast back to CBProvider in the AI constructor
extern "C" struct DLL_EXPORT CBProvider {
    CBProvider(
        const RenderAnsiCBCB renderansicbcb_,
        const ResetCBCB resetcbcb_,
        const SysCBCB syscbcb_,
        const ActionCBCB actioncbcb_,
        const ResultCB resultcb_
    ) : renderansicbcb(renderansicbcb_),
        resetcbcb(resetcbcb_),
        syscbcb(syscbcb_),
        actioncbcb(actioncbcb_),
        resultcb(resultcb_) {}

    const RenderAnsiCBCB renderansicbcb;
    const ResetCBCB resetcbcb;
    const SysCBCB syscbcb;
    const ActionCBCB actioncbcb;
    const ResultCB resultcb;
};

} // namespace MMAI
