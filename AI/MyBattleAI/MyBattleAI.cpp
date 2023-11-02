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

void CMyBattleAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) {
  print("*** initBattleInterface -- BUT NO BAGGAGE ***");
  myInitBattleInterface(ENV, CB, std::any{});
}

void CMyBattleAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences)
{
  print("*** initBattleInterface -- BUT NO BAGGAGE ***");
  myInitBattleInterface(ENV, CB, std::any{});
}

void CMyBattleAI::myInitBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage) {
  print("*** myInitBattleInterface ***");
  env = ENV;
  cbc = cb = CB;

  wasWaitingForRealize = CB->waitTillRealize;
  wasUnlockingGs = CB->unlockGsWhenWaiting;
  CB->waitTillRealize = false;
  CB->unlockGsWhenWaiting = false;

  // See note in MyAdventureAI::initGameInterface
  assert(baggage.has_value());
  assert(baggage.type() == typeid(CBProvider*));
  cbprovider = std::any_cast<CBProvider*>(baggage);

  print("*** call cbprovider->pycbinit([this](const ActionF &arr) { cbprovider->cppcb(arr) })");
  cbprovider->pycbinit([this](const ActionF &arr) { this->cppcb(arr); });
}

// Called by GymEnv on every "step()" call
void CMyBattleAI::cppcb(const ActionF &arr) {
    print(std::string("called with arr: ") + actionToStr(arr));

    print("Assign action = arr");
    action = arr;

    // Unblock "activeStack"
    print("acquire lock");
    boost::unique_lock<boost::mutex> lock(m);
    print("cond.notify_one()");
    cond.notify_one();

    print("return");
}

void CMyBattleAI::activeStack(const CStack * stack)
{
  print("activeStack called for " + stack->nodeName());

  const StateF state_ary = {1, 2, 4};

  print("acquire lock");
  boost::unique_lock<boost::mutex> lock(m);

  print("call this->pycb(state_ary)");
  cbprovider->pycb(state_ary);


  // We've set some events in motion:
  //  - in python, "env" now has our cppcb stored (via pycbinit)
  //  - in python, "env" now has the state stored (via pycb)
  //  - in python, "env" constructor can now return (pycb also set an event)
  //  - in python, env.step(action) will be called, which will call cppcb
  // our cppcb will then call AI->cb->makeAction()
  // ...we wait until that happens, and FINALLY we can return from yourTurn
  print("cond.wait()");
  cond.wait(lock);

  print(std::string("this->action: ") + actionToStr(action));

  // we have the action from GymEnv, now make it
  print("TODO: call cb->makeAction()");

  print("return");
  return;
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

std::string CMyBattleAI::stateToStr(const StateF &arr) {
    return std::to_string(arr[0]) + " " + std::to_string(arr[1]) + " " + std::to_string(arr[2]);
}

std::string CMyBattleAI::actionToStr(const ActionF &arr) {
    return std::to_string(arr[0]) + " " + std::to_string(arr[1]) + " " + std::to_string(arr[2]);
}

void CMyBattleAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  print("battleCatapultAttacked called");
}
