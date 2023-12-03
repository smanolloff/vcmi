#pragma once

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

        const SlotID attslot;
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

        int side;
        std::shared_ptr<CBattleCallback> cb;
        std::shared_ptr<Environment> env;

        //Previous setting of cb
        bool wasWaitingForRealize;
        bool wasUnlockingGs;

        std::unique_ptr<Battlefield> battlefield;
        std::vector<AttackLog> attackLogs;

        std::unique_ptr<Action> action;
        std::unique_ptr<Export::Result> result;
        Export::F_GetAction getAction;

        BuildActionResult buildAction(Battlefield &bf, Action &action);
        Export::Result buildResult(Battlefield &bf);

        std::string renderANSI();

        void error(const std::string &text) const;
        void warn(const std::string &text) const;
        void info(const std::string &text) const;
        void debug(const std::string &text) const;
    public:
        BAI();
        ~BAI();


        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
        void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
        void myInitBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, Export::F_GetAction f_getAction);
        void activeStack(const CStack * stack) override; //called when it's turn of that stack
        void yourTacticPhase(int distance) override;

        void actionFinished(const BattleAction &action) override;//occurs AFTER every action taken by any stack or by the hero
        void actionStarted(const BattleAction &action) override;//occurs BEFORE every action taken by any stack or by the hero

        void battleAttack(const BattleAttack *ba) override; //called when stack is performing attack
        void battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
        void battleEnd(const BattleResult *br, QueryID queryID) override;
        void battleNewRoundFirst(int round) override; //called at the beginning of each turn before changes are applied;
        void battleNewRound(int round) override; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
        // void battleLogMessage(const std::vector<MetaString> & lines) override;
        void battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
        void battleSpellCast(const BattleSpellCast *sc) override;
        void battleStacksEffectsSet(const SetStackEffect & sse) override;//called when a specific effect is set to stacks
        //void battleTriggerEffect(const BattleTriggerEffect & bte) override;
        //void battleStartBefore(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override; //called just before battle start
        void battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right
        void battleUnitsChanged(const std::vector<UnitChanges> & units) override;
        //void battleObstaclesChanged(const std::vector<ObstacleChanges> & obstacles) override;
        void battleCatapultAttacked(const CatapultAttack & ca) override; //called when catapult makes an attack
        //void battleGateStateChanged(const EGateState state) override;
    };
}
