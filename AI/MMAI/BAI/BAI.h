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
#include "lib/AI_Base.h"
#include "export.h"
#include "types/action.h"
#include "types/battlefield.h"
#include "build_action_result.h"

namespace MMAI {
    constexpr int ACTION_UNSET = -1;

    struct AttackLog {
        AttackLog(
            SlotID attslot_, SlotID defslot_, bool isOurStackAttacked_,
            int dmg_, int units_, int value_
        ) : attslot(attslot_), defslot(defslot_), isOurStackAttacked(isOurStackAttacked_),
                dmg(dmg_), units(units_), value(value_) {}

        // attacker dealing dmg might be our friendly fire
        // If we look at Attacker POV, we would count our friendly fire as "dmg dealt"
        // So we look at Defender POV, so our friendly fire is counted as "dmg received"
        // This means that if the enemy does friendly fire dmg,
        //  we would count it as our dmg dealt - that is OK (we have "tricked" the enemy!)
        const bool isOurStackAttacked;

        const SlotID attslot;  // XXX: can be INVALID when dmg is not from creature
        const SlotID defslot;
        const int dmg;
        const int units;

        // NOTE: "value" is hard-coded in original H3 and can be found online:
        // https://heroes.thelazy.net/index.php/List_of_creatures
        const int value;
    };

    class BAI : public CBattleGameInterface
    {
        friend class AAI;

        // XXX: those mess up the regular log colors => leave blank
        std::string ansicolor = "";
        std::string ansireset = "";

        std::string addrstr = "?";
        std::string color = "?";
        BattleSide::Type side;
        std::shared_ptr<CBattleCallback> cb;
        std::shared_ptr<Environment> env;
        std::shared_ptr<CPlayerBattleCallback> battle;
        bool isMorale; // if our activeStack is called after good morale effect


        // std::shared_ptr<CPlayerBattleCallback> battle;

        //Previous setting of cb
        bool wasWaitingForRealize;
        bool wasUnlockingGs;

        std::unique_ptr<Battlefield> battlefield;
        std::vector<AttackLog> attackLogs;

        std::unique_ptr<Action> action;
        std::unique_ptr<Export::Result> result;
        Export::F_GetAction getAction;
        Export::F_GetValue getValue;

        // used only if when initialized with baggage (ie. via CPI)
        Export::F_GetAction getActionOrig;

        BuildActionResult buildAction(const BattleID &bid, Battlefield &bf, Action &action);
        Export::Result buildResult(const BattleID &bid, Battlefield &bf);

        std::string renderANSI();

        static std::string PadLeft(const std::string& input, size_t desiredLength, char paddingChar);
        static std::string PadRight(const std::string& input, size_t desiredLength, char paddingChar);

        static std::tuple<Hexes, const CStack*> Reconstruct(
            const Export::Result &r,
            const std::shared_ptr<CBattleCallback> cb,
            const std::shared_ptr<CPlayerBattleCallback> battle
        );

        static void Verify(const Battlefield &bf, Hexes &hexes, const CStack* astack);

        static std::string Render(
            const Export::Result &r,
            const std::shared_ptr<CBattleCallback> cb,
            const std::shared_ptr<CPlayerBattleCallback> battle,
            const Battlefield &bf,
            const std::string color,
            const Action *action,
            const std::vector<AttackLog> attackLogs
        );

        // DEBUG ONLY
        std::string debugInfo(const BattleID &bid, Action &action, const CStack *astack, BattleHex *nbh);
        // DEBUG ONLY
        std::vector<Export::Action> allactions;

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;

        void setInitBattleInterfaceVars(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB);
        void myInitBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, Export::F_GetAction f_getAction, Export::F_GetValue f_getValue);
    public:
        BAI();
        ~BAI();


        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage, std::string colorstr) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
        void activeStack(const BattleID &bid, const CStack * stack) override; //called when it's turn of that stack
        void yourTacticPhase(const BattleID &bid, int distance) override;

        void actionFinished(const BattleID &bid, const BattleAction &action) override;//occurs AFTER every action taken by any stack or by the hero
        void actionStarted(const BattleID &bid, const BattleAction &action) override;//occurs BEFORE every action taken by any stack or by the hero

        void battleAttack(const BattleID &bid, const BattleAttack *ba) override; //called when stack is performing attack
        void battleStacksAttacked(const BattleID &bid, const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
        void battleEnd(const BattleID &bid, const BattleResult *br, QueryID queryID) override;
        void battleNewRoundFirst(const BattleID &bid) override; //called at the beginning of each turn before changes are applied;
        void battleNewRound(const BattleID &bid) override; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
        // void battleLogMessage(const BattleID &bid, const std::vector<MetaString> & lines) override;
        void battleStackMoved(const BattleID &bid, const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleSpellCast(const BattleID &bid, const BattleSpellCast *sc) override;
        void battleStacksEffectsSet(const BattleID &bid, const SetStackEffect & sse) override;//called when a specific effect is set to stacks
        void battleTriggerEffect(const BattleID &bid, const BattleTriggerEffect & bte) override;
        //void battleStartBefore(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override; //called just before battle start
        void battleStart(const BattleID &bid, const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right
        void battleUnitsChanged(const BattleID &bid, const std::vector<UnitChanges> & units) override;
        //void battleObstaclesChanged(const BattleID &bid, const std::vector<ObstacleChanges> & obstacles) override;
        void battleCatapultAttacked(const BattleID &bid, const CatapultAttack & ca) override; //called when catapult makes an attack
        //void battleGateStateChanged(const BattleID &bid, const EGateState state) override;
    };
}
