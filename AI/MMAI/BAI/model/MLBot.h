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

#include "battle/CPlayerBattleCallback.h"
#include "callback/CBattleGameInterface.h"

#include <boost/circular_buffer.hpp>

namespace MMAI::BAI
{

class MLBot : public CBattleGameInterface
{
public:
    explicit MLBot(const std::string & botname);
    ~MLBot() override;

    void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AICombatOptions aiCombatOptions) override;
    void battleStart(const BattleID & battleID, const CCreatureSet * army1, const CCreatureSet * army2, int3 tile, const CGHeroInstance * hero1, const CGHeroInstance * hero2, BattleSide side, bool replayAllowed) override;
    void yourTacticPhase(const BattleID & battleID, int distance) override;

    // Custom logic
    void activeStack(const BattleID & bid, const CStack * astack) override;

    // Debug
    void actionStarted(const BattleID & bid, const BattleAction & action) override;

private:
    std::shared_ptr<CBattleCallback> cb;
    std::shared_ptr<CBattleGameInterface> bot; // calls will be delegated to this object
    std::shared_ptr<CPlayerBattleCallback> battle = nullptr;

    std::string addrstr = "?";
    std::string colorname = "?";

    const CStack * vip = nullptr;
    const std::string botname = "?";
    int nturns = 0;
    int nrounds = 0;

    boost::circular_buffer<std::string> msgbuf;
    void addmsg(const CStack* astack, const CStack* vip, const std::string & event);

    void handleVip(const BattleID & bid, const CStack * vip);
    void handleGuard(const BattleID & bid, const CStack * guard, const CStack * vip);
    void battleNewRound(const BattleID & bid) override;

    /*
     * Logging
     */

    void error(const std::string & text) const;
    void warn(const std::string & text) const;
    void info(const std::string & text) const;
    void debug(const std::string & text) const;
    void trace(const std::string & text) const;
    void log(ELogLevel::ELogLevel level, const std::string & text) const;

    void error(const std::function<std::string()> & cb) const;
    void warn(const std::function<std::string()> & cb) const;
    void info(const std::function<std::string()> & cb) const;
    void debug(const std::function<std::string()> & cb) const;
    void trace(const std::function<std::string()> & cb) const;
    void log(ELogLevel::ELogLevel level, const std::function<std::string()> & cb) const;

    template<typename... Args>
    void error(const std::string & format, Args... args) const;
    template<typename... Args>
    void warn(const std::string & format, Args... args) const;
    template<typename... Args>
    void info(const std::string & format, Args... args) const;
    template<typename... Args>
    void debug(const std::string & format, Args... args) const;
    template<typename... Args>
    void trace(const std::string & format, Args... args) const;
    template<typename... Args>
    void log(ELogLevel::ELogLevel level, const std::string & format, Args... args) const;
    template<typename... Args>
    void _log(ELogLevel::ELogLevel level, const std::string & format, Args... args) const;

};
}
