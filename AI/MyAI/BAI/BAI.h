#pragma once
#include "lib/CStack.h"
#include "lib/AI_Base.h"
#include "CCallback.h"
#include "../mytypes.h"
#include "battle/BattleHex.h"
#include <memory>

#define MMAI_NS_BEGIN namespace MMAI {
#define MMAI_NS_END }

MMAI_NS_BEGIN

#define BF_XMAX 15    // GameConstants::BFIELD_WIDTH - 2 (ignore "side" cols)
#define BF_YMAX 11    // GameConstants::BFIELD_HEIGHT
#define BF_SIZE 165   // BF_XMAX * BF_YMAX
#define ATTRS_PER_STACK 12

//Accessibility is property of hex in battle. It doesn't depend on stack, side's perspective and so on.
enum class HexState : int
{
    FREE_REACHABLE,     // 0
    FREE_UNREACHABLE,   // 1
    OBSTACLE,           // 2
    FRIENDLY_STACK_1,   // 3
    FRIENDLY_STACK_2,   // 4
    FRIENDLY_STACK_3,   // 5
    FRIENDLY_STACK_4,   // 6
    FRIENDLY_STACK_5,   // 7
    FRIENDLY_STACK_6,   // 8
    FRIENDLY_STACK_7,   // 9
    ENEMY_STACK_1,      // 10
    ENEMY_STACK_2,      // 11
    ENEMY_STACK_3,      // 12
    ENEMY_STACK_4,      // 13
    ENEMY_STACK_5,      // 14
    ENEMY_STACK_6,      // 15
    ENEMY_STACK_7,      // 16
    count
};

using ActionResult = std::pair<
  const std::shared_ptr<BattleAction>,
  std::vector<std::string>
>;

class BAI : public CBattleGameInterface
{
  friend class AAI;

  int side;
  std::shared_ptr<CBattleCallback> cb;
  std::shared_ptr<Environment> env;

  //Previous setting of cb
  bool wasWaitingForRealize;
  bool wasUnlockingGs;

  CBProvider * cbprovider;

  boost::mutex m;
  boost::condition_variable cond;
  bool awaitingAction = false;

  // NOTE: those could be made static, no need to init at every battle

  const std::map<HexState, std::string> hexStateNames;
  const std::map<HexState, std::string> initHexStateNames();

  const std::map<std::string, HexState> hexStateValues;
  const std::map<std::string, HexState> initHexStateValues();

  const std::array<NValue, static_cast<int>(HexState::count)> hexStateNMap;
  const std::array<NValue, static_cast<int>(HexState::count)> initHexStateNMap();

  const std::array<BattleHex, BF_SIZE> allBattleHexes;
  const std::array<BattleHex, BF_SIZE> initAllBattleHexes();

  const std::array<std::string, actionMax> allGymActionNames;
  const std::array<std::string, actionMax> initAllGymActionNames();


  // can't be const -- called during battleStart (unavailable in constructor)
  std::map<const CStack*, NValue> stackHNSMap;
  void initStackHNSMap();

  // can't be const value (received after init)
  // => use const pointer
  GymAction gymaction = -1;
  GymResult gymresult;

  void cppcb(const GymAction &gymaction);
  std::string gymaction_str(const GymAction &gymaction);
  std::string gymresult_str(const GymResult &gymresult);
  const std::string buildReport(const GymResult &gr, const GymAction &ga, const CStack* astack);

  const GymState buildState(const CStack * stack);
  ActionResult buildAction(const CStack * stack, GymState gymstate, GymAction action);

public:
  BAI();
  ~BAI();


  void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) override;
  void initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences) override;
  void myInitBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage);
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
  void debug(const std::string &text) const;
};

MMAI_NS_END
