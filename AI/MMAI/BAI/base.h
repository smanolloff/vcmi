/*
 * base.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

// CI build fails without this
#include "Global.h"

#include "battle/CPlayerBattleCallback.h"
#include "callback/CBattleGameInterface.h"
#include "callback/CBattleCallback.h"

#include "schema/base.h"

namespace MMAI::BAI {
    class Base : public CBattleGameInterface {
    public:
        // Factory method for versioned derived BAI (e.g. BAI::V1)
        static std::shared_ptr<Base> Create(
            Schema::IModel* model,
            const std::shared_ptr<Environment> env,
            const std::shared_ptr<CBattleCallback> cb,
            const bool enableSpellsUsage
        );

        Base() = delete;
        Base(
            Schema::IModel* model,
            const int version,
            const std::shared_ptr<Environment> env,
            const std::shared_ptr<CBattleCallback> cb
        );


        /*
         * These methods MUST be overridden by derived BAI (e.g. BAI::V1)
         * Their base implementation is is for logging purposes only.
         */

        virtual Schema::Action getNonRenderAction() = 0;
        virtual void activeStack(const BattleID &bid, const CStack * stack) override;
        virtual void yourTacticPhase(const BattleID &bid, int distance) override;

        /*
         * These methods MAY be overriden by derived BAI (e.g. BAI::V1)
         * Their base implementation is for logging purposes only.
         */

        virtual void init(bool enableSpellsUsage);  // called shortly after object construction
        virtual void actionFinished(const BattleID &bid, const BattleAction &action) override;
        virtual void actionStarted(const BattleID &bid, const BattleAction &action) override;
        virtual void battleAttack(const BattleID &bid, const BattleAttack *ba) override;
        virtual void battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) override;
        virtual void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        virtual void battleGateStateChanged(const BattleID &bid, const EGateState state) override;
        virtual void battleLogMessage(const BattleID &bid, const std::vector<MetaString> & lines) override;
        virtual void battleNewRound(const BattleID &bid) override;
        virtual void battleNewRoundFirst(const BattleID &bid) override;
        virtual void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> &obstacles) override;
        virtual void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        virtual void battleStackMoved(const BattleID &bid, const CStack *stack, const BattleHexArray & dest, int distance, bool teleport) override;
        virtual void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> &bsa, bool ranged) override;
        virtual void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) override;
        virtual void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, BattleSide side, bool replayAllowed) override;
        virtual void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override;
        virtual void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & changes) override;

        /*
         * These methods MUST NOT be called.
         * Their base implementation throws a runtime error
         * (whistleblower for developer mistakes)
         */
        virtual void initBattleInterface(std::shared_ptr<Environment> _1, std::shared_ptr<CBattleCallback> _2) override { throw std::runtime_error("BAI (base class) received initBattleInterface call"); }
        virtual void initBattleInterface(std::shared_ptr<Environment> _1, std::shared_ptr<CBattleCallback> _2, AutocombatPreferences _3) override { throw std::runtime_error("BAI (base class) received initBattleInterface call"); }

        const int version;
        const std::string name = "BAI"; // used in logging
        const std::string colorname;
    protected:
        const std::shared_ptr<Environment> env;
        const std::shared_ptr<CBattleCallback> cb;
        Schema::IModel* model;

        std::string addrstr = "?";

        // Set via VCMI_BAI_VERBOSE env var ("1" to enable)
        bool verbose = false;

        bool enableSpellsUsage = false;

        /*
         * Templates defined in the header
         * Needed to prevent linker errors for calls from derived classes
         */

        template<typename ... Args> void _log(const ELogLevel::ELogLevel level, const std::string &format, Args ... args) const {
            logAi->log(level, "%s-%s [%s] " + format, name, addrstr, colorname, args...);
        }

        template<typename ... Args> void error(const std::string &format, Args ... args) const { log(ELogLevel::ERROR, format, args...); }
        template<typename ... Args> void warn(const std::string &format, Args ... args) const { log(ELogLevel::WARN, format, args...); }
        template<typename ... Args> void info(const std::string &format, Args ... args) const { log(ELogLevel::INFO, format, args...); }
        template<typename ... Args> void debug(const std::string &format, Args ... args) const { log(ELogLevel::DEBUG, format, args...); }
        template<typename ... Args> void trace(const std::string &format, Args ... args) const { log(ELogLevel::DEBUG, format, args...); }
        template<typename ... Args> void log(ELogLevel::ELogLevel level, const std::string &format, Args ... args) const {
            if (logAi->getEffectiveLevel() <= level) _log(level, format, args...);
        }


        void error(const std::string &text) const { log(ELogLevel::ERROR, text); }
        void warn(const std::string &text) const { log(ELogLevel::WARN, text); }
        void info(const std::string &text) const { log(ELogLevel::INFO, text); }
        void debug(const std::string &text) const { log(ELogLevel::DEBUG, text); }
        void trace(const std::string &text) const { log(ELogLevel::TRACE, text); }
        void log(ELogLevel::ELogLevel level, const std::string &text) const {
            if (logAi->getEffectiveLevel() <= level) _log(level, "%s", text);
        }

        void error(const std::function<std::string()> &cb) const { log(ELogLevel::ERROR, cb); }
        void warn(const std::function<std::string()> &cb) const { log(ELogLevel::WARN, cb); }
        void info(const std::function<std::string()> &cb) const { log(ELogLevel::INFO, cb); }
        void debug(const std::function<std::string()> &cb) const { log(ELogLevel::DEBUG, cb); }
        void trace(const std::function<std::string()> &cb) const { log(ELogLevel::TRACE, cb); }
        void log(ELogLevel::ELogLevel level, const std::function<std::string()> &cb) const {
            if (logAi->getEffectiveLevel() <= level) _log(level, "%s", cb());
        }
    };
}
