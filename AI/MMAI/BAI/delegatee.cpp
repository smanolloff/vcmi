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

#include <memory>
#include <stdexcept>

#include "delegatee.h"
#include "v1/BAI.h"
#include "v2/BAI.h"

namespace MMAI::BAI {
    // Inheritable
    void Delegatee::error(const std::string &text) const { logAi->error("BAI::V%d-%s [%s] %s", version, addrstr, colorname, text); }
    void Delegatee::warn(const std::string &text) const { logAi->warn("BAI::V%d-%s [%s] %s", version, addrstr, colorname, text); }
    void Delegatee::info(const std::string &text) const { logAi->info("BAI::V%d-%s [%s] %s", version, addrstr, colorname, text); }
    void Delegatee::debug(const std::string &text) const { logAi->debug("BAI::V%d-%s [%s] %s", version, addrstr, colorname, text); }

    template<typename T, typename ... Args> void Delegatee::error(const std::string &format, T t, Args ... args) const { logAi->error("BAI::V%d-%s [%s] " + format, version, addrstr, colorname, args...); }
    template<typename T, typename ... Args> void Delegatee::warn(const std::string &format, T t, Args ... args) const { logAi->warn("BAI::V%d-%s [%s] " + format, version, addrstr, colorname, args...); }
    template<typename T, typename ... Args> void Delegatee::info(const std::string &format, T t, Args ... args) const { logAi->info("BAI::V%d-%s [%s] " + format, version, addrstr, colorname, args...); }
    template<typename T, typename ... Args> void Delegatee::debug(const std::string &format, T t, Args ... args) const { logAi->debug("BAI::V%d-%s [%s] " + format, version, addrstr, colorname, args...); }

    // Local
    void Delegatee::_error(const std::string &text) const { logAi->error("Delegatee-%s [%s] %s", addrstr, colorname, text); }
    void Delegatee::_warn(const std::string &text) const { logAi->warn("Delegatee-%s [%s] %s", addrstr, colorname, text); }
    void Delegatee::_info(const std::string &text) const { logAi->info("Delegatee-%s [%s] %s", addrstr, colorname, text); }
    void Delegatee::_debug(const std::string &text) const { logAi->debug("Delegatee-%s [%s] %s", addrstr, colorname, text); }

    // static
    std::unique_ptr<Delegatee> Delegatee::Create(
        const std::string colorname,
        const Schema::Baggage* baggage,
        const std::shared_ptr<Environment> env,
        const std::shared_ptr<CBattleCallback> cb
    ) {
        std::unique_ptr<Delegatee> res;

        auto version = colorname == "red" ? baggage->versionRed : baggage->versionBlue;

        switch (version) {
        break; case 1:
            res = std::make_unique<V1::BAI>(version, colorname, baggage, env, cb);
        break; case 2:
            res = std::make_unique<V2::BAI>(version, colorname, baggage, env, cb);
        break; default:
            throw std::runtime_error("Unsupported schema version: " + std::to_string(version));
        }

        res->init();
        return res;
    }

    Delegatee::Delegatee(
        const int version_,
        const std::string colorname_,
        const Schema::Baggage* baggage_,
        const std::shared_ptr<Environment> env_,
        const std::shared_ptr<CBattleCallback> cb_
    ) : version(version_),
        colorname(colorname_),
        baggage(baggage_),
        env(env_),
        cb(cb_)
    {
        std::ostringstream oss;
        oss << this; // Store this memory address
        addrstr = oss.str();
        info("+++ constructor +++");

        // Not sure if this is needed
        cb->waitTillRealize = false;
        cb->unlockGsWhenWaiting = false;

        if (colorname == "red") {
            // ansicolor = "\033[41m";  // red background
            f_getAction = baggage->f_getActionRed;
            f_getValue = baggage->f_getValueRed;
            debug("using f_getActionRed");
        } else if (colorname == "blue") {
            // ansicolor = "\033[44m";  // blue background
            f_getAction = baggage->f_getActionBlue;
            f_getValue = baggage->f_getValueBlue;
            debug("using f_getActionBlue");
        } else {
            error("expected red or blue, got: " + colorname);
            throw std::runtime_error("unexpected color");
        }
    }
}
