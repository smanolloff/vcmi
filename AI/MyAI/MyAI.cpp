#include "StdInc.h"
#include "../../lib/AI_Base.h"
#include "MyAI.h"
#include "../../lib/CStack.h"
#include "../../CCallback.h"
#include "../../lib/CCreatureHandler.h"

static std::shared_ptr<CBattleCallback> cbc;

CMyAI::CMyAI()
  : side(-1),
  wasWaitingForRealize(false),
  wasUnlockingGs(false)
{
  print("constructor.");
}

CMyAI::~CMyAI()
{
  if(cb)
  {
    //Restore previous state of CB - it may be shared with the main AI (like VCAI)
    cb->waitTillRealize = wasWaitingForRealize;
    cb->unlockGsWhenWaiting = wasUnlockingGs;
  }
}

void CMyAI::print(const std::string &text) const
{
  logAi->trace("CMyAI  [%p]: %s", this, text);
}

void CMyAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB)
{
  print("init...");
  env = ENV;
  cbc = cb = CB;

  wasWaitingForRealize = CB->waitTillRealize;
  wasUnlockingGs = CB->unlockGsWhenWaiting;
  CB->waitTillRealize = false;
  CB->unlockGsWhenWaiting = false;
}

void CMyAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences)
{
  initBattleInterface(ENV, CB);
}

void CMyAI::actionFinished(const BattleAction &action)
{
  print("actionFinished called");
}

void CMyAI::actionStarted(const BattleAction &action)
{
  print("actionStarted called");
}

void CMyAI::yourTacticPhase(int distance)
{
  cb->battleMakeTacticAction(BattleAction::makeEndOFTacticPhase(cb->battleGetTacticsSide()));
}

void CMyAI::activeStack( const CStack * stack )
{
  print("activeStack called for " + stack->nodeName());

  // TODO

  // call python fn which should return a BattleAction derivative

  return;
}

void CMyAI::battleAttack(const BattleAttack *ba)
{
  print("battleAttack called");
}

void CMyAI::battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
  print("battleStacksAttacked called");
}

void CMyAI::battleEnd(const BattleResult *br, QueryID queryID)
{
  print("battleEnd called");
}

void CMyAI::battleNewRoundFirst(int round)
{
  print("battleNewRoundFirst called");
}

void CMyAI::battleNewRound(int round)
{
  print("battleNewRound called");
}

void CMyAI::battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport)
{
  print("battleStackMoved called");
}

void CMyAI::battleSpellCast(const BattleSpellCast *sc)
{
  print("battleSpellCast called");
}

void CMyAI::battleStacksEffectsSet(const SetStackEffect & sse)
{
  print("battleStacksEffectsSet called");
}

void CMyAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
{
  print("battleStart called");
  side = Side;
}

void CMyAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  print("battleCatapultAttacked called");
}
