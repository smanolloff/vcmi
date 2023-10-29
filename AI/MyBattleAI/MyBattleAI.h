#pragma once
#include "../../lib/AI_Base.h"

class CMyBattleAI : public CBattleGameInterface
{
  int side;
  std::shared_ptr<CBattleCallback> cb;
  std::shared_ptr<Environment> env;

  //Previous setting of cb
  bool wasWaitingForRealize;
  bool wasUnlockingGs;

public:
  CMyBattleAI();
  ~CMyBattleAI();


  void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
  void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
  void activeStack(const CStack * stack) override; //called when it's turn of that stack
  void yourTacticPhase(int distance) override;

  void actionFinished(const BattleAction &action) override;//occurs AFTER every action taken by any stack or by the hero
  void actionStarted(const BattleAction &action) override;//occurs BEFORE every action taken by any stack or by the hero

  void battleAttack(const BattleAttack *ba) override; //called when stack is performing attack
  void battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged) override; //called when stack receives damage (after battleAttack())
  void battleEnd(const BattleResult *br, QueryID queryID) override;
  void battleNewRoundFirst(int round) override; //called at the beginning of each turn before changes are applied;
  void battleNewRound(int round) override; //called at the beginning of each turn, round=-1 is the tactic phase, round=0 is the first "normal" turn
  //void battleLogMessage(const std::vector<MetaString> & lines) override;
  void battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport) override;
  void battleSpellCast(const BattleSpellCast *sc) override;
  void battleStacksEffectsSet(const SetStackEffect & sse) override;//called when a specific effect is set to stacks
  //void battleTriggerEffect(const BattleTriggerEffect & bte) override;
  //void battleStartBefore(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2) override; //called just before battle start
  void battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool side, bool replayAllowed) override; //called by engine when battle starts; side=0 - left, side=1 - right
  //void battleUnitsChanged(const std::vector<UnitChanges> & units) override;
  //void battleObstaclesChanged(const std::vector<ObstacleChanges> & obstacles) override;
  void battleCatapultAttacked(const CatapultAttack & ca) override; //called when catapult makes an attack
  //void battleGateStateChanged(const EGateState state) override;

private:
  void print(const std::string &text) const;
};
