#pragma once

/*****
****** THIS FILE LIVES IN:
******
****** vcmi/AI/MyAI/mytypes.h
******
*****/

#include <cassert>
#include <array>
#include <string>
#include <map>
#include <cstdint>

namespace MMAIExport {
    #ifndef DLL_EXPORT
    #define DLL_EXPORT __attribute__((visibility("default")))
    #endif

    using ErrMask = uint16_t;
    enum class ErrType : int {
        // !!! SYNC with pyconnector.py !!!
        ALREADY_WAITED,
        MOVE_SELF,
        HEX_UNREACHABLE,
        HEX_BLOCKED,
        STACK_NA,
        STACK_DEAD,
        STACK_INVALID,
        MOVE_SHOOT,
        ATTACK_IMPOSSIBLE,
    };

    static const std::map<const ErrType, std::tuple<const ErrMask, const std::string, const std::string>> ERRORS = {
        {ErrType::ALREADY_WAITED,    {ErrMask(1 << 0), "ERR_ALREADY_WAITED", "already waited this turn"}},
        {ErrType::MOVE_SELF,         {ErrMask(1 << 1), "ERR_MOVE_SELF", "cannot move to self unless attacking"}},
        {ErrType::HEX_UNREACHABLE,   {ErrMask(1 << 2), "ERR_HEX_UNREACHABLE", "target hex is unreachable"}},
        {ErrType::HEX_BLOCKED,       {ErrMask(1 << 3), "ERR_HEX_BLOCKED", "target hex is blocked"}},
        {ErrType::STACK_NA,          {ErrMask(1 << 4), "ERR_STACK_NA", "target stack does not exist"}},
        {ErrType::STACK_DEAD,        {ErrMask(1 << 5), "ERR_STACK_DEAD", "target stack is dead"}},
        {ErrType::STACK_INVALID,     {ErrMask(1 << 6), "ERR_STACK_INVALID", "target stack is invalid (turret?)"}},
        {ErrType::MOVE_SHOOT,        {ErrMask(1 << 7), "ERR_MOVE_SHOOT", "cannot move and shoot"}},
        {ErrType::ATTACK_IMPOSSIBLE, {ErrMask(1 << 8), "ERR_ATTACK_IMPOSSIBLE", "melee attack not possible"}},
    };

    // Arbitary int value normalized as -1..1 float
    extern "C" struct DLL_EXPORT NValue {
        int orig;
        float norm;

        NValue() : orig(0), norm(-1) {};
        NValue(int v, int vmin, int vmax) {
            assert(vmin < vmax);

            if (v < vmin) v = vmin;
            else if (v > vmax) v = vmax;

            orig = v;
            norm = 2.0 * static_cast<float>(v - vmin) / (vmax - vmin) - 1.0;
        }
    };

    /**
     * State:
     * 165 hex + (14 stack * 10 attrs) + current_stack
     *
     * !!! SYNC with pyconnector.py !!!
     */
    constexpr int STATE_SIZE = 306;
    using State = std::array<NValue, STATE_SIZE>;

    /**
     * Regular actions to be passed by GymEnv:
     * 3 non-move actions (retreat, defend, wait)
     * 1320 move[+attack] actions (165 hexes * 8 actions each)
     *
     * !!! SYNC with pyconnector.py !!!
     */
    constexpr int N_ACTIONS = 1323;
    using ActMask = std::array<bool, N_ACTIONS>;

    constexpr int ACTION_RETREAT = 0;
    constexpr int ACTION_DEFEND = 1;
    constexpr int ACTION_WAIT = 2;

    // Control actions(not part of the regular action space)
    constexpr int ACTION_UNSET = INT16_MIN;
    constexpr int ACTION_RESET = -1;
    constexpr int ACTION_RENDER_ANSI = -2;

    enum ResultType {REGULAR, ANSI_RENDER, UNSET};

    struct Result {
        Result() {};

        // Constructor 1: rendering
        Result(std::string ansiRender_)
        : type(ResultType::ANSI_RENDER), ansiRender(ansiRender_) {};

        // Constructor 1: regular result
        Result(State state_, ActMask actmask_, int dmgDealt_, int dmgReceived_,
            int unitsLost_, int unitsKilled_, int valueLost_, int valueKilled_
        ) : type(ResultType::REGULAR),
            state(state_),
            actmask(actmask_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_) {};

        // Constructor 2: regular result (battle ended)
        Result(State state_, ActMask actmask_, int dmgDealt_, int dmgReceived_,
            int unitsLost_, int unitsKilled_, int valueLost_, int valueKilled_,
            bool victory_
        ) : type(ResultType::REGULAR),
            state(state_),
            actmask(actmask_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_),
            ended(true),
            victory(victory_) {};

        const ResultType type = ResultType::UNSET;
        const State state = {};
        const ActMask actmask = {};
        const int dmgDealt = 0;
        const int dmgReceived = 0;
        const int unitsLost = 0;
        const int unitsKilled = 0;
        const int valueLost = 0;
        const int valueKilled = 0;
        const std::string ansiRender = "";
        const bool ended = false;
        const bool victory = false;

        // This is dynamically set to avoid re-creating Result{}
        // on each getAction() if when actions as mostly errors
        // (ie. when nothing changes except the errors)
        ErrMask errmask = 0;

    };

    using Action = int16_t;

    // F_Sys is a CPP function returned by pyclient's `init`
    // GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
    using F_Sys = std::function<void(std::string cmd)>;

    // F_GetAction is a Python function passed to the AI constructor.
    // AI will invoke it on every "yourTurn()" call, with 1 argument: a Result
    using F_GetAction = std::function<Action(const Result * result)>;

    // The CB functions above are all bundled into CBProvider struct
    // whose purpose is to be seamlessly transportable through VCMI code
    // as a std::any object, then cast back to CBProvider in the AI constructor
    struct DLL_EXPORT CBProvider {
        F_GetAction f_getAction = nullptr;
        CBProvider(F_GetAction f) : f_getAction(f) {}
    };

}
