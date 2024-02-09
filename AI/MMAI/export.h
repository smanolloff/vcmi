// =============================================================================
// Copyright 2024 Simeon Manolov <s.manolloff@gmail.com>.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// =============================================================================

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

    constexpr int NV_MIN = float(0);
    constexpr int NV_MAX = float(1);
    constexpr int NV_DIFF = NV_MAX - NV_MIN;

    // Arbitary int value normalized as -1..1 float
    extern "C" struct DLL_EXPORT NValue {
        int orig;
        float norm;

        NValue() : orig(-1), norm(NV_MIN) {};
        NValue(int v, int vmin, int vmax) {
            // if (vmin < vmax)
            //     throw std::runtime_error(std::to_string(vmin) + ">" + std::to_string(vmax));

            if (v < vmin)
                throw std::runtime_error(std::to_string(v) + " < " + std::to_string(vmin) + " (NValue error)");
            else if (v > vmax)
                throw std::runtime_error(std::to_string(v) + " > " + std::to_string(vmax));

            orig = v;

            // XXX: this is a simplified version for 0..1
            // norm = static_cast<float>(v - vmin) / (vmax - vmin);

            norm = NV_DIFF * static_cast<float>(v - vmin) / (vmax - vmin) + NV_MIN;
        }
    };

    constexpr int N_STACK_ATTRS = 16;

    /**
     * State:
     * 165 hexes, N_HEX_ATTRS each
     */
    constexpr int N_HEX_ATTRS = 2 + N_STACK_ATTRS;  // id, hexstate, attrs
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
    constexpr int N_HEX_ACTIONS = 14;
    constexpr int N_ACTIONS = N_NONHEX_ACTIONS + 165 * N_HEX_ACTIONS;

    using ActMask = std::array<bool, N_ACTIONS>;

    // Control actions(not part of the regular action space)
    constexpr Action ACTION_UNSET = INT16_MIN;
    constexpr Action ACTION_RESET = -1;
    constexpr Action ACTION_RENDER_ANSI = -2;

    enum ResultType {REGULAR, ANSI_RENDER, UNSET};

    enum class Side : int {ATTACKER, DEFENDER};

    struct Result {
        Result() {};

        // Constructor 1: rendering
        Result(std::string ansiRender_)
        : type(ResultType::ANSI_RENDER), ansiRender(ansiRender_) {};

        // Constructor 1: regular result
        Result(State state_, ActMask actmask_, Side side_, int dmgDealt_, int dmgReceived_,
            int unitsLost_, int unitsKilled_, int valueLost_, int valueKilled_,
            int side0ArmyValue_, int side1ArmyValue_
        ) : type(ResultType::REGULAR),
            state(state_),
            actmask(actmask_),
            side(side_),
            dmgDealt(dmgDealt_),
            dmgReceived(dmgReceived_),
            unitsLost(unitsLost_),
            unitsKilled(unitsKilled_),
            valueLost(valueLost_),
            valueKilled(valueKilled_),
            side0ArmyValue(side0ArmyValue_),
            side1ArmyValue(side1ArmyValue_) {};

        // Constructor 2 (move constructor): regular result (battle ended)
        Result(Result &&other, bool victory_)
        : type(ResultType::REGULAR),
          state(other.state),
          actmask(other.actmask),
          side(other.side),
          dmgDealt(other.dmgDealt),
          dmgReceived(other.dmgReceived),
          unitsLost(other.unitsLost),
          unitsKilled(other.unitsKilled),
          valueLost(other.valueLost),
          valueKilled(other.valueKilled),
          side0ArmyValue(other.side0ArmyValue),
          side1ArmyValue(other.side1ArmyValue),
          ended(true),
          victory(victory_) {};


        const ResultType type = ResultType::UNSET;
        const State state = {};
        const ActMask actmask = {};
        const Side side = Side::ATTACKER;
        const int dmgDealt = 0;
        const int dmgReceived = 0;
        const int unitsLost = 0;
        const int unitsKilled = 0;
        const int valueLost = 0;
        const int valueKilled = 0;
        const int side0ArmyValue = 0;
        const int side1ArmyValue = 0;
        const std::string ansiRender = "";
        const bool ended = false;
        const bool victory = false;

        // This is dynamically set to avoid re-creating Result{}
        // on each getAction() if when actions as mostly errors
        // (ie. when nothing changes except the errors)
        ErrMask errmask = 0;

    };

    // F_Sys is a CPP function returned by pyclient's `init`
    // GymEnv will invoke it on "close()" calls (or "reset(hard=True)" ?)
    using F_Sys = std::function<void(std::string cmd)>;

    // An F_GetAction type function is called by BAI on each "activeStack()" call.
    // Such a function is usually be:
    // - libconnector's getAction (when VCMI is started as a Gym env)
    //   (VcmiEnv->PyConnector->Connector::initBaggage())
    // - a dummy getAction (when VCMI is started as a standalone program)
    using F_GetAction = std::function<Action(const Result * result)>;

    // The CB functions above are all bundled into Baggage struct
    // whose purpose is to be seamlessly transportable through VCMI code
    // as a std::any object, then cast back to Baggage in the AI constructor
    struct DLL_EXPORT Baggage {
        Baggage() = delete;
        Baggage(F_GetAction f)
        : f_getAction(f), f_getActionAttacker(f), f_getActionDefender(f) {}
        const F_GetAction f_getAction;

        // Set during vcmi_init(...) to "MMAI", "BattleAI" or "StupidAI"
        std::string attackerBattleAIName;
        std::string defenderBattleAIName;

        // Optionally set during vcmi_init(...) to a pre-trained model func
        F_GetAction f_getActionAttacker;
        F_GetAction f_getActionDefender;
    };
}
