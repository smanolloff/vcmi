#include "StdInc.h"
#include "../../lib/AI_Base.h"
#include "MyBattleAI.h"
#include "../../lib/CStack.h"
#include "../../CCallback.h"
#include "../../lib/CCreatureHandler.h"

static std::shared_ptr<CBattleCallback> cbc;

CMyBattleAI::CMyBattleAI()
  : side(-1),
  wasWaitingForRealize(false),
  wasUnlockingGs(false)
{
  print("constructor.");
}

CMyBattleAI::~CMyBattleAI()
{
  if(cb)
  {
    //Restore previous state of CB - it may be shared with the main AI (like VCAI)
    cb->waitTillRealize = wasWaitingForRealize;
    cb->unlockGsWhenWaiting = wasUnlockingGs;
  }
}

void CMyBattleAI::print(const std::string &text) const
{
  logAi->trace("CMyBattleAI  [%p]: %s", this, text);
}

void CMyBattleAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB)
{
  print("init...");
  env = ENV;
  cbc = cb = CB;

  wasWaitingForRealize = CB->waitTillRealize;
  wasUnlockingGs = CB->unlockGsWhenWaiting;
  CB->waitTillRealize = false;
  CB->unlockGsWhenWaiting = false;
}

void CMyBattleAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences)
{
  initBattleInterface(ENV, CB);
}

void CMyBattleAI::actionFinished(const BattleAction &action)
{
  print("actionFinished called");
}

void CMyBattleAI::actionStarted(const BattleAction &action)
{
  print("actionStarted called");
}

void CMyBattleAI::yourTacticPhase(int distance)
{
  cb->battleMakeTacticAction(BattleAction::makeEndOFTacticPhase(cb->battleGetTacticsSide()));
}

void CMyBattleAI::activeStack( const CStack * stack )
{
  print("activeStack called for " + stack->nodeName());

  // TODO

  // call python fn which should return a BattleAction derivative

  return;
}

void CMyBattleAI::battleAttack(const BattleAttack *ba)
{
  print("battleAttack called");
}

void CMyBattleAI::battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
  print("battleStacksAttacked called");
}

void CMyBattleAI::battleEnd(const BattleResult *br, QueryID queryID)
{
  print("battleEnd called");
}

void CMyBattleAI::battleNewRoundFirst(int round)
{
  print("battleNewRoundFirst called");
}

void CMyBattleAI::battleNewRound(int round)
{
  print("battleNewRound called");
}

void CMyBattleAI::battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport)
{
  print("battleStackMoved called");
}

void CMyBattleAI::battleSpellCast(const BattleSpellCast *sc)
{
  print("battleSpellCast called");
}

void CMyBattleAI::battleStacksEffectsSet(const SetStackEffect & sse)
{
  print("battleStacksEffectsSet called");
}

void CMyBattleAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
{
  print("battleStart called");
  side = Side;
}

void CMyBattleAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  print("battleCatapultAttacked called");
}
