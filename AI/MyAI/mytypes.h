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

using ErrMask = uint16_t;
enum ErrType {
    ERR_ALREADY_WAITED,
    ERR_MOVE_SELF,
    ERR_HEX_UNREACHABLE,
    ERR_HEX_BLOCKED,
    ERR_STACK_NA,
    ERR_STACK_DEAD,
    ERR_STACK_INVALID,
    ERR_MOVE_SHOOT,
    ERR_ATTACK_IMPOSSIBLE,
};


static const std::map<const ErrType, std::tuple<const ErrMask, const std::string, const std::string>> ERRORS = {
    {ERR_ALREADY_WAITED,    {ErrMask(1 << 0), "ERR_ALREADY_WAITED", "already waited this turn"}},
    {ERR_MOVE_SELF,         {ErrMask(1 << 1), "ERR_MOVE_SELF", "cannot move to self unless attacking"}},
    {ERR_HEX_UNREACHABLE,   {ErrMask(1 << 2), "ERR_HEX_UNREACHABLE", "target hex is unreachable"}},
    {ERR_HEX_BLOCKED,       {ErrMask(1 << 3), "ERR_HEX_BLOCKED", "target hex is blocked"}},
    {ERR_STACK_NA,          {ErrMask(1 << 4), "ERR_STACK_NA", "target stack does not exist"}},
    {ERR_STACK_DEAD,        {ErrMask(1 << 5), "ERR_STACK_DEAD", "target stack is dead"}},
    {ERR_STACK_INVALID,     {ErrMask(1 << 6), "ERR_STACK_INVALID", "target stack is invalid (turret?)"}},
    {ERR_MOVE_SHOOT,        {ErrMask(1 << 7), "ERR_MOVE_SHOOT", "cannot move and shoot"}},
    {ERR_ATTACK_IMPOSSIBLE, {ErrMask(1 << 8), "ERR_ATTACK_IMPOSSIBLE", "melee attack not possible"}},
};

// static const std::string ERR1_DESC = "asd";
// static const uint8_t ERR2 = 0b00000010;
// static const uint8_t ERR3 = 0b00000100;
// static const uint8_t ERR4 = 0b00001000;
// static const uint8_t ERR5 = 0b00010000;
// static const uint8_t ERR6 = 0b00100000;
// static const uint8_t ERR7 = 0b01000000;
// static const uint8_t ERR8 = 0b10000000;

// static const uint8_t ACTION_ERROR_0 = 0b0;

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
    uint8_t errmask = 0;
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
