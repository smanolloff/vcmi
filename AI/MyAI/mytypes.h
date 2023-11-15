#pragma once

// *** THIS FILE LIVES IN:
// ***
// *** vcmi/AI/MyAI/mytypes.h

#include <cassert>
#include <array>
#include <string>
#include <map>
#include <cstdint>

namespace MMAI {

#ifndef DLL_EXPORT
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

// Regular actions to be passed by GymEnv:
// 3 non-move actions
// 1320 move[+attack] actions (165 hexes * 8 actions each)
static const int N_ACTIONS = 3 + 1320;

// Non-move actions:
static const int ACTION_RETREAT = 0;
static const int ACTION_DEFEND = 1;
static const int ACTION_WAIT = 2;

// Control actions
// (not part of the regular action space)
static const int ACTION_UNSET = INT16_MIN;
static const int ACTION_RESET = -1;
static const int ACTION_RENDER_ANSI = -2;


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

enum ResultType {
    REGULAR,
    ANSI_RENDER,
    UNSET
};

// SYNC WITH VcmiEnv.observation_space
using State = std::array<NValue, STATE_SIZE>;
struct Result {
    ResultType type = ResultType::UNSET;
    State state = {};
    ErrMask errmask = 0;
    int dmgDealt = 0;
    int dmgReceived = 0;
    int unitsLost = 0;
    int unitsKilled = 0;
    int valueLost = 0;
    int valueKilled = 0;
    bool ended = false;
    bool victory = false;
    std::string ansiRender = "";
};

using Action = int16_t;

// F_Sys is a CPP function returned by pyclient's `init`
// GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
using F_Sys = std::function<void(std::string cmd)>;

// F_GetAction is a Python function passed to the AI constructor.
// AI will invoke it on every "yourTurn()" call, with 1 argument: a Result
using F_GetAction = std::function<Action(const Result &result)>;

// The CB functions above are all bundled into CBProvider struct
// whose purpose is to be seamlessly transportable through VCMI code
// as a std::any object, then cast back to CBProvider in the AI constructor
struct DLL_EXPORT CBProvider {
    F_GetAction f_getAction = nullptr;
    CBProvider(F_GetAction f) : f_getAction(f) {}
};

} // namespace MMAI
