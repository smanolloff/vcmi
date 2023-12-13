#pragma once

#include <map>
#include <string>
#include <cassert>

/*****
****** THIS FILE LIVES IN:
******
****** vcmi/AI/MMAI/export.h
******
*****/

namespace MMAI::Export {
    #ifndef DLL_EXPORT
    #define DLL_EXPORT __attribute__((visibility("default")))
    #endif

    using ErrMask = uint16_t;
    enum class ErrType : int {
        ALREADY_WAITED,
        MOVE_SELF,
        HEX_UNREACHABLE,
        HEX_BLOCKED,
        HEX_MELEE_NA,
        STACK_NA,
        STACK_DEAD,
        STACK_INVALID,
        CANNOT_SHOOT,
        FRIENDLY_FIRE,
        INVALID_DIR,
    };

    static const std::map<const ErrType, std::tuple<const ErrMask, const std::string, const std::string>> ERRORS = {
        {ErrType::ALREADY_WAITED,    {ErrMask(1 << 0), "ERR_ALREADY_WAITED", "already waited this turn"}},
        {ErrType::MOVE_SELF,         {ErrMask(1 << 1), "ERR_MOVE_SELF", "cannot move to self unless attacking"}},
        {ErrType::HEX_UNREACHABLE,   {ErrMask(1 << 2), "ERR_HEX_UNREACHABLE", "target hex is unreachable"}},
        {ErrType::HEX_BLOCKED,       {ErrMask(1 << 3), "ERR_HEX_BLOCKED", "target hex is blocked"}},
        {ErrType::HEX_MELEE_NA,      {ErrMask(1 << 4), "ERR_HEX_MELEE_NA", "hex to perform the melee attack from is N/A"}},
        {ErrType::STACK_NA,          {ErrMask(1 << 5), "ERR_STACK_NA", "no stack at target"}},
        {ErrType::STACK_DEAD,        {ErrMask(1 << 6), "ERR_STACK_DEAD", "target stack is dead"}},
        {ErrType::STACK_INVALID,     {ErrMask(1 << 7), "ERR_STACK_INVALID", "target stack is invalid (turret?)"}},
        {ErrType::CANNOT_SHOOT,      {ErrMask(1 << 8), "ERR_CANNOT_SHOOT", "ranged attack not possible"}},
        {ErrType::FRIENDLY_FIRE,     {ErrMask(1 << 9), "ERR_FRIENDLY_FIRE", "attempted to attack a friendly stack"}},
        {ErrType::INVALID_DIR,       {ErrMask(1 << 10), "ERR_INVALID_DIR", "attempted to attack from a T/B direction with a 1-hex stack"}},
    };

    // XXX: the normalization formula is simplified for (0,1)
    constexpr int NV_MIN = float(0);
    constexpr int NV_MAX = float(1);
    constexpr int NV_DIFF = NV_MAX - NV_MIN;

    // Arbitary int value normalized as -1..1 float
    extern "C" struct DLL_EXPORT NValue {
        int orig;
        float norm;

        NValue() : orig(0), norm(NV_MIN) {};
        NValue(int v, int vmin, int vmax) {
            // if (vmin < vmax)
            //     throw std::runtime_error(std::to_string(vmin) + ">" + std::to_string(vmax));

            if (v < vmin)
                throw std::runtime_error(std::to_string(v) + " < " + std::to_string(vmin) + " (NValue error)");
            else if (v > vmax)
                throw std::runtime_error(std::to_string(v) + " > " + std::to_string(vmax));

            orig = v;
            norm = static_cast<float>(v - vmin) / (vmax - vmin);
            // norm = NV_DIFF * static_cast<float>(v - vmin) / (vmax - vmin) + NV_MIN;
        }
    };

    constexpr int N_STACK_ATTRS = 14;

    /**
     * State:
     * 165 hexes, N_HEX_ATTRS each
     */
    constexpr int N_HEX_ATTRS = 1 + N_STACK_ATTRS;  // hexstate + attrs
    constexpr int STATE_SIZE = 165 * N_HEX_ATTRS;
    using State = std::array<NValue, STATE_SIZE>;
    using Action = int16_t;

    /**
     * 2 non-hex actions:
     * retreat, wait
     */
    constexpr int N_NONHEX_ACTIONS = 2;
    constexpr Action ACTION_RETREAT = 0;
    constexpr Action ACTION_WAIT = 1;

    /**
     * 8 hex actions:
     * move, move+att1, ... move+att7
     */
    constexpr int N_HEX_ACTIONS = 10;
    constexpr int N_ACTIONS = N_NONHEX_ACTIONS + 165 * N_HEX_ACTIONS;

    using ActMask = std::array<bool, N_ACTIONS>;

    // Control actions(not part of the regular action space)
    constexpr Action ACTION_UNSET = INT16_MIN;
    constexpr Action ACTION_RESET = -1;
    constexpr Action ACTION_RENDER_ANSI = -2;

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

        // Constructor 2 (move constructor): regular result (battle ended)
        Result(Result &&other, bool victory_)
        : type(ResultType::REGULAR),
          state(other.state),
          actmask(other.actmask),
          dmgDealt(other.dmgDealt),
          dmgReceived(other.dmgReceived),
          unitsLost(other.unitsLost),
          unitsKilled(other.unitsKilled),
          valueLost(other.valueLost),
          valueKilled(other.valueKilled),
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

    enum class Side : int {
        ATTACKER,
        DEFENDER
    };

    // F_Sys is a CPP function returned by pyclient's `init`
    // GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
    using F_Sys = std::function<void(std::string cmd)>;

    // F_GetAction is optionally called via F_IDGetAction only (see below).
    // - can be libconnector's getAction (VCMI started as a Gym env)
    //   (VcmiEnv->PyConnector->Connector::initBaggage())
    // - can be a dummy getAction (VCMI started as a standalone program)
    //   (myclient->main())
    using F_GetAction = std::function<Action(const Result * result)>;

    // F_IDGetAction is called by BAI on each "activeStack()" call.
    // It is set only manually during init_vcmi(...) depending on the args:
    // - can be a proxy to F_GetAction (if attacker/defenderAI is AI_MMAI_USER)
    // - can be libloader's getAction (if attackerAI = AI_MMAI_MODEL)
    using F_IDGetAction = std::function<Action(Side side, const Result * result)>;

    // The CB functions above are all bundled into Baggage struct
    // whose purpose is to be seamlessly transportable through VCMI code
    // as a std::any object, then cast back to Baggage in the AI constructor
    struct DLL_EXPORT Baggage {
        Baggage() = delete;
        Baggage(F_GetAction f)
        : f_getAction(f), f_getActionAttacker(f), f_getActionDefender(f) {}
        const F_GetAction f_getAction;

        // Set during vcmi_init(...) to "MMAI", "BattleAI" or "StupidAI"
        std::string AttackerBattleAIName;
        std::string DefenderBattleAIName;

        // Optionally set during vcmi_init(...) to a pre-trained model func
        F_GetAction f_getActionAttacker;
        F_GetAction f_getActionDefender;
    };
}
