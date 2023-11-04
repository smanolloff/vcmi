#include "StdInc.h"
#include "../../../lib/AI_Base.h"
#include "BAI.h"
#include "../../../lib/CStack.h"
#include "../../../CCallback.h"
#include "../../../lib/CCreatureHandler.h"

namespace MMAI {

BAI::BAI()
  : side(-1),
  wasWaitingForRealize(false),
  wasUnlockingGs(false)
{
  print("constructor.");
}

BAI::~BAI()
{
  if(cb)
  {
    //Restore previous state of CB - it may be shared with the main AI (like VCAI)
    cb->waitTillRealize = wasWaitingForRealize;
    cb->unlockGsWhenWaiting = wasUnlockingGs;
  }
}

void BAI::print(const std::string &text) const
{
  logAi->trace("BAI  [%p]: %s", this, text);
}

void BAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB) {
  print("*** initBattleInterface -- BUT NO BAGGAGE ***");
  myInitBattleInterface(ENV, CB, std::any{});
}

void BAI::initBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, AutocombatPreferences autocombatPreferences)
{
  print("*** initBattleInterface -- BUT NO BAGGAGE ***");
  myInitBattleInterface(ENV, CB, std::any{});
}

void BAI::myInitBattleInterface(std::shared_ptr<Environment> ENV, std::shared_ptr<CBattleCallback> CB, std::any baggage) {
  print("*** myInitBattleInterface ***");
  env = ENV;
  cb = CB;

  wasWaitingForRealize = CB->waitTillRealize;
  wasUnlockingGs = CB->unlockGsWhenWaiting;
  CB->waitTillRealize = false;
  CB->unlockGsWhenWaiting = false;

  // See note in AAI::initGameInterface
  assert(baggage.has_value());
  assert(baggage.type() == typeid(CBProvider*));
  cbprovider = std::any_cast<CBProvider*>(baggage);

  print("*** call cbprovider->pycbinit([this](const GymAction &ga) { cbprovider->cppcb(ga) })");
  cbprovider->pycbinit([this](const GymAction &ga) { this->cppcb(ga); });
}

// Called by GymEnv on every "step()" call
void BAI::cppcb(const GymAction &gymaction) {
    print(std::string("called with gymaction: ") + gymaction_str(gymaction));

    print("Assign this->gymaction = gymaction");
    this->gymaction = gymaction;

    // Unblock "activeStack"
    print("acquire lock");
    boost::unique_lock<boost::mutex> lock(m);
    print("cond.notify_one()");
    cond.notify_one();

    print("return");
}

void BAI::activeStack(const CStack * stack)
{
  print("activeStack called for " + stack->nodeName());

  const GymState gymstate = {1, 2, 4};

  print("acquire lock");
  boost::unique_lock<boost::mutex> lock(m);

  print("call this->pycb(gymstate)");
  cbprovider->pycb(gymstate);


  // We've set some events in motion:
  //  - in python, "env" now has our cppcb stored (via pycbinit)
  //  - in python, "env" now has the state stored (via pycb)
  //  - in python, "env" constructor can now return (pycb also set an event)
  //  - in python, env.step(action) will be called, which will call cppcb
  // our cppcb will then call AI->cb->makeAction()
  // ...we wait until that happens, and FINALLY we can return from yourTurn
  print("cond.wait()");
  cond.wait(lock);

  print(std::string("this->gymaction: ") + gymaction_str(gymaction));

  // auto hexes = cb->battleGetAvailableHexes(stack, true);
  // cb->battleMakeUnitAction(BattleAction::makeMove(stack, BattleHex(15, 1)));

  // if std::binary_search(v.begin(), v.end(), value)

  // // we have the action from GymEnv, now make it
  // print("TODO: call cb->makeAction()");

  print("return");
  return;
}


void BAI::actionFinished(const BattleAction &action)
{
  print("actionFinished called");
}

void BAI::actionStarted(const BattleAction &action)
{
  print("actionStarted called");
}

void BAI::yourTacticPhase(int distance)
{
  cb->battleMakeTacticAction(BattleAction::makeEndOFTacticPhase(cb->battleGetTacticsSide()));
}

void BAI::battleAttack(const BattleAttack *ba)
{
  print("battleAttack called");
}

void BAI::battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
  print("battleStacksAttacked called");
}

void BAI::battleEnd(const BattleResult *br, QueryID queryID)
{
  print("battleEnd called");
}

void BAI::battleNewRoundFirst(int round)
{
  print("battleNewRoundFirst called");
}

void BAI::battleNewRound(int round)
{
  print("battleNewRound called");
}

void BAI::battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport)
{
  print("battleStackMoved called");
}

void BAI::battleSpellCast(const BattleSpellCast *sc)
{
  print("battleSpellCast called");
}

void BAI::battleStacksEffectsSet(const SetStackEffect & sse)
{
  print("battleStacksEffectsSet called");
}

void BAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
{
  print("battleStart called");
  side = Side;
}

std::string BAI::gymaction_str(const GymAction &ga) {
  // TODO: redundant?
  auto tmp = static_cast<uint16_t>(ga);
  return std::to_string(tmp);
  // return std::to_string(ga[0]) + " " + std::to_string(ga[1]) + " " + std::to_string(ga[2]);
}

std::string BAI::gymstate_str(const GymState &gs) {
  return std::to_string(gs[0]) + " " + std::to_string(gs[1]) + " " + std::to_string(gs[2]);
}

void BAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  print("battleCatapultAttacked called");
}
}
