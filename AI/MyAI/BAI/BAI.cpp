#include "StdInc.h"
#include "BAI.h"

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

#define ATTRS_PER_STACK 12

void BAI::print(const std::string &text) const
{
  logAi->trace("BAI  [%p]: %s", this, text);
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

void BAI::activeStack(const CStack * astack)
{
  print("activeStack called for " + astack->nodeName());

  auto gymstate = buildState(astack);

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

  // auto hexes = cb->battleGetAvailableHexes(astack, true);
  // cb->battleMakeUnitAction(BattleAction::makeMove(astack, BattleHex(15, 1)));

  // if std::binary_search(v.begin(), v.end(), value)

  // // we have the action from GymEnv, now make it
  // print("TODO: call cb->makeAction()");

  print("return");
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

MMAI_NS_END
