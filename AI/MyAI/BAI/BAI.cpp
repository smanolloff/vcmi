#include "NetPacks.h"
#include "StdInc.h"
#include "BAI.h"
#include "battle/BattleAction.h"
#include "battle/BattleHex.h"
#include "mytypes.h"
#include <memory>
#include <string>

MMAI_NS_BEGIN

// HNS = short for HexNState
#define HNS(hs) hexStateNMap.at(static_cast<size_t>(HexState::hs))

#define MIN_QTY 0
#define MAX_QTY 5000
#define MIN_ATT 0
#define MAX_ATT 100
#define MIN_DEF 0
#define MAX_DEF 100
#define MIN_MORALE -6   // spell max is +3
#define MAX_MORALE 6    // spell max is -3
#define MIN_LUCK -6     // spell max is +3
#define MAX_LUCK 6      // spell max is -3
#define MIN_SHOTS 0
#define MAX_SHOTS 24
#define MIN_DMG 0
#define MAX_DMG 100     // 80 + bless (+10%) + ?
#define MIN_HP 0
#define MAX_HP 1500     // 1000 + vial of lifeblood (+25%) + ?
#define MIN_SPEED 0
#define MAX_SPEED 30

#define ADD_ERR(err) errors.emplace_back("*** Invalid action: " + info + ": " + err)

#define RETURN_ACT_OR_ERR(act) if (errors.empty()) { \
    print("Action: " + info); \
    return {std::make_shared<BattleAction>(act), {}}; \
  } else { \
    return {nullptr, errors}; \
  }

void BAI::print(const std::string &text) const
{
  logAi->error("BAI  [%p]: %s", this, text);
}

void BAI::debug(const std::string &text) const
{
  logAi->warn("BAI  [%p]: %s", this, text);
}

// Called by GymEnv on every "step()" call
void BAI::cppcb(const GymAction &gymaction) {
    print(std::string("called with gymaction: ") + gymaction_str(gymaction));

    debug("Assign this->gymaction = gymaction");
    this->gymaction = gymaction;

    // Unblock "activeStack"
    debug("acquire lock");
    boost::unique_lock<boost::mutex> lock(m);
    debug("cond.notify_one()");
    cond.notify_one();

    debug("return");
}

void BAI::activeStack(const CStack * astack)
{
  print("*** activeStack ***");
  // print("activeStack called for " + astack->nodeName());

  gymresult.state = buildState(astack);
  std::shared_ptr<BattleAction> ba;

  while(!ba) {
    boost::unique_lock<boost::mutex> lock(m);
    awaitingAction = true;
    print("Sending result: " + gymresult_str(gymresult));
    cbprovider->pycb(gymresult);

    // We've set some events in motion:
    //  - in python, "env" now has our cppcb stored (via pycbinit)
    //  - in python, "env" now has the state stored (via pycb)
    //  - in python, "env" constructor can now return (pycb also set an event)
    //  - in python, env.step(action) will be called, which will call cppcb
    // our cppcb will then call AI->cb->makeAction()
    // ...we wait until that happens, and FINALLY we can return from yourTurn
    cond.wait(lock);
    awaitingAction = false;

    debug("Got action: " + gymaction_str(gymaction));
    auto pair = buildAction(astack, gymresult.state, gymaction);
    auto errors = pair.second;
    ba = pair.first;

    if (ba) {
      assert(errors.empty());
      debug("Action was VALID");
      gymresult = {};
    } else {
      assert(!errors.empty());
      auto errstring = std::accumulate(errors.begin(), errors.end(), std::string(),
          [](auto &a, auto &b) { return a + "\n" + b; });

      print("Action was INVALID:" + errstring);
      gymresult.n_errors = errors.size();
    }
  }

  cb->battleMakeUnitAction(*ba);

  debug("return");
  return;
}

const GymState BAI::buildState(const CStack * astack) {
  GymState gymstate = {};

  auto accessibility = cb->getAccesibility();
  auto rinfo = cb->getReachability(astack);
  auto speed = astack->speed();

  // TODO: remove (one-time sanity check)
  assert(allBattleHexes.size() == 165);

  // "global" i-counter for gymstate
  int gi = 0;

  for (const BattleHex& hex : allBattleHexes) {
    switch(accessibility[hex.hex]) {
    case LIB_CLIENT::EAccessibility::ACCESSIBLE:
      gymstate[gi++] = (rinfo.distances[hex] <= speed)
        ? HNS(FREE_REACHABLE)
        : HNS(FREE_UNREACHABLE);

      break;
    case LIB_CLIENT::EAccessibility::OBSTACLE:
      gymstate[gi++] = HNS(OBSTACLE);
      break;
    case LIB_CLIENT::EAccessibility::ALIVE_STACK:
      // simply map astack to HexNormalizedState[STACK_X]
      // NOTE: switch gives errors if declaring variable here
      // auto state = cb->battleGetStackByPos(hex, true);
      gymstate[gi++] = stackHNSMap.at(cb->battleGetStackByPos(hex, true));
      break;

    // XXX: unhandled hex states
    // case LIB_CLIENT::EAccessibility::DESTRUCTIBLE_WALL:
    // case LIB_CLIENT::EAccessibility::GATE:
    // case LIB_CLIENT::EAccessibility::UNAVAILABLE:
    // case LIB_CLIENT::EAccessibility::SIDE_COLUMN:
    default:
      throw std::runtime_error(
        "Unexpected hex accessibility for hex "+ std::to_string(hex.hex) + ": "
          + std::to_string(static_cast<int>(accessibility[hex.hex]))
      );

    }
  }

  auto allstacks = cb->battleGetStacks();
  assert(allstacks.size() <= 14);

  for (auto &stack : allstacks) {
    int slot = stack->unitSlot();
    assert(slot >= 0 && slot < 7); // same as assert(slot.validSlot())

    // 0th slot is gi+0..gi+10
    int i = gi + slot*ATTRS_PER_STACK;

    std::string prefix = "";

    if (stack->unitSide() == astack->unitSide()) {
      prefix = "F" + std::to_string(slot+1) + " ";
    } else {
      // enemy units in gymstate are in "slots" 8..14
      prefix = "E" + std::to_string(slot+1) + " ";
      i += (7 * ATTRS_PER_STACK);
    }

    int i0 = i; // for sanity check later

    // it seems ranged and melee defense is always the same (even with air shield)
    // TODO: check if same same for ranged dmg? (with precision)

    // NOTE:
    // dmg(min, melee), dmg(max, melee), dmg(min, ranged), dmg(max, ranged)
    // could be replaced by
    // dmg(avg, melee), dmg(avg, ranged), dmg(deviation_factor)
    // (saves a total of 14 values)

    gymstate[i++] = NValue(prefix + "qty", stack->getCount(), MIN_QTY, MAX_QTY);
    gymstate[i++] = NValue(prefix + "att", stack->getAttack(false), MIN_ATT, MAX_ATT);
    gymstate[i++] = NValue(prefix + "def", stack->getDefense(false), MIN_DEF, MAX_DEF);
    gymstate[i++] = NValue(prefix + "shots", stack->shots.available(), MIN_SHOTS, MAX_SHOTS);
    gymstate[i++] = NValue(prefix + "dmg (min, melee)", stack->getMinDamage(false), MIN_DMG, MAX_DMG);
    gymstate[i++] = NValue(prefix + "dmg (max, melee)", stack->getMaxDamage(false), MIN_DMG, MAX_DMG);
    gymstate[i++] = NValue(prefix + "dmg (min, ranged)", stack->getMinDamage(true), MIN_DMG, MAX_DMG);
    gymstate[i++] = NValue(prefix + "dmg (min, ranged)", stack->getMaxDamage(true), MIN_DMG, MAX_DMG);
    gymstate[i++] = NValue(prefix + "HP", stack->getMaxHealth(), MIN_HP, MAX_HP);
    gymstate[i++] = NValue(prefix + "HP left", stack->getFirstHPleft(), MIN_HP, MAX_HP);
    gymstate[i++] = NValue(prefix + "speed", stack->speed(), MIN_SPEED, MAX_SPEED);
    gymstate[i++] = NValue(prefix + "waited", stack->waitedThisTurn, 0, 1);

    assert(i == i0 + ATTRS_PER_STACK);
  }

  gi += (14 * ATTRS_PER_STACK);

  gymstate[gi++] = NValue("active stack", astack->unitSlot(), 0, 7);

  assert(gi == gymstate.size());
  return gymstate;
}


ActionResult BAI::buildAction(const CStack * astack, GymState gymstate, GymAction gymaction) {
  std::vector<std::string> errors {};
  std::string info;

  if (gymaction == 0) {
    info = "Retreat";
    // TODO: handle if retreat is impossible (can't know in advance)
    //       Will it be a query dialog?
    RETURN_ACT_OR_ERR(BattleAction::makeRetreat(cb->battleGetMySide()))
  }
  if (gymaction == 1) {
    info = "Defend";
    RETURN_ACT_OR_ERR(BattleAction::makeDefend(astack))
  }
  if (gymaction == 2) {
    info = "Wait";
    if (astack->waitedThisTurn) {
      // TODO: assert gymstate actually reported that the stack has waited
      ADD_ERR("already waited this turn");
    }

    RETURN_ACT_OR_ERR(BattleAction::makeWait(astack))
    return {std::make_shared<BattleAction>(BattleAction::makeWait(astack)), {}};
  }

  auto subaction = (gymaction-3) % 8;
  auto y = ((gymaction-3) / 8) / BF_XMAX;
  auto x = ((gymaction-3) / 8) % BF_XMAX;
  auto dest = BattleHex(x + 1, y); // "real" hex is offset by 1 (left side col)
  auto hexstate = gymstate[x + y*BF_XMAX];

  info = "Move to ("+ std::to_string(x) + "," + std::to_string(y) + ")";

  // self-destination is OK if shooting, or if attacking a neighbour
  auto ownhexes = astack->getHexes();
  auto destself = (std::find(ownhexes.begin(), ownhexes.end(), dest) != ownhexes.end());

  if (destself && subaction == 0) {
    // move-only does not allow "moving" to self
    ADD_ERR("bad target hex (self):" + hexstate.name);
  } else if (!destself && hexstate.orig != static_cast<int>(HexState::FREE_REACHABLE)) {
    ADD_ERR("bad target hex (blocked or unreachable): " + hexstate.name);
  }

  // just move
  if (subaction == 0) {
    RETURN_ACT_OR_ERR(BattleAction::makeMove(astack, dest))
  }

  // move and attack enemy unit 0..6
  auto slot = subaction - 1;
  info += " and attack enemy stack #" + std::to_string(slot);

  auto targets = cb->battleGetStacksIf([&astack, &slot](const CStack * stack) {
    return stack->unitSlot() == slot && stack->getOwner() != astack->getOwner();
  });

  if (targets.empty()) {
    ADD_ERR("no such stack");
    return {nullptr, errors};
  }

  assert(targets.size() <= 1);
  auto stack = targets[0];

  if (!stack->alive())
    ADD_ERR("stack is dead");
  else if (!stack->isValidTarget())
    ADD_ERR("stack is not a valid target (turret?)");

  if (cb->battleCanShoot(astack)) {
    if (!destself)
      ADD_ERR("trying to both move and shoot");

    RETURN_ACT_OR_ERR(BattleAction::makeShotAttack(astack, stack));
  }

  if (!astack->isMeleeAttackPossible(astack, stack))
    ADD_ERR("melee attack not possible");

  RETURN_ACT_OR_ERR(BattleAction::makeMeleeAttack(astack, stack->position, dest));
}

void BAI::battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
  print("*** battleStacksAttacked ***");

  for(auto & elem : bsa) {
    auto * defender = cb->battleGetStackByID(elem.stackAttacked, false);

    // attacker dealing dmg might be our friendly fire
    // => use defender
    if (defender->getOwner() == cb->getPlayerID())
      gymresult.dmgReceived += elem.damageAmount;
    else
      gymresult.dmgDealt += elem.damageAmount;
  }
}

void BAI::battleEnd(const BattleResult *br, QueryID queryID)
{
  print("*** battleEnd (QID: " + std::to_string(queryID) + ") ***");

  gymresult = {};
  gymresult.ended = true;
  gymresult.victory = br->winner == cb->battleGetMySide();

  print("Sending result: " + gymresult_str(gymresult));

  cbprovider->pycb(gymresult);

}

void BAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
{
  debug("*** battleStart ***");
  side = Side;
  initStackHNSMap();
}


MMAI_NS_END
