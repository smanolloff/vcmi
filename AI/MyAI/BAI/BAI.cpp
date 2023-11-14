#include "NetPacks.h"
#include "StdInc.h"
#include "BAI.h"
#include "battle/BattleAction.h"
#include "battle/BattleHex.h"
#include "mytypes.h"
#include <memory>
#include <string>
#include <cstdio>

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


#define RETURN_ACT_OR_ERR(act) return {\
  (errmask == 0) ? std::make_shared<BattleAction>(act) : nullptr, \
  errmask, \
  errmsgs};

void BAI::error(const std::string &text) const { logAi->error("BAI %s", text); }
void BAI::warn(const std::string &text) const { logAi->warn("BAI %s", text); }
void BAI::info(const std::string &text) const { logAi->info("BAI %s", text); }
void BAI::debug(const std::string &text) const { logAi->debug("BAI %s", text); }

void BAI::activeStack(const CStack * astack)
{
  info("*** activeStack ***");
  // print("activeStack called for " + astack->nodeName());

  result.state = buildState(astack);
  result.type = ResultType::REGULAR;

  this->stack = astack;
  std::shared_ptr<BattleAction> ba;

  while(true) {
    action = getAction(result);

    const auto actname = allActionNames[action];
    debug("Got action: " + action_str(action) + " (" + actname + ")");
    auto tuple = buildAction(astack, result.state, action);
    ba = std::get<0>(tuple);
    auto errmask = std::get<1>(tuple);
    auto errmsgs = std::get<2>(tuple);

    if (ba) {
      assert(errmask == 0);
      info("Action is VALID: " + actname);
      break;
    } else {
      assert(errmask > 0);
      auto errstring = std::accumulate(errmsgs.begin(), errmsgs.end(), std::string(),
          [](auto &a, auto &b) { return a + "\n" + b; });

      warn("Action is INVALID: " + actname + ":\n" + errstring);
      result.errmask = errmask;
    }
  }

  result = {};

  debug("cb->battleMakeUnitAction(*ba)");
  cb->battleMakeUnitAction(*ba);

  return;
}

const State BAI::buildState(const CStack * astack) {
  State state = {};

  auto accessibility = cb->getAccesibility();
  auto rinfo = cb->getReachability(astack);
  auto speed = astack->speed();

  // TODO: remove (one-time sanity check)
  assert(allBattleHexes.size() == 165);

  // "global" i-counter for state
  int gi = 0;

  for (const BattleHex& hex : allBattleHexes) {
    switch(accessibility[hex.hex]) {
    case LIB_CLIENT::EAccessibility::ACCESSIBLE:
      state[gi++] = (rinfo.distances[hex] <= speed)
        ? HNS(FREE_REACHABLE)
        : HNS(FREE_UNREACHABLE);

      break;
    case LIB_CLIENT::EAccessibility::OBSTACLE:
      state[gi++] = HNS(OBSTACLE);
      break;
    case LIB_CLIENT::EAccessibility::ALIVE_STACK:
      // simply map astack to HexNormalizedState[STACK_X]
      // NOTE: switch gives errors if declaring variable here
      // auto state = cb->battleGetStackByPos(hex, true);
      state[gi++] = stackHNSMap.at(cb->battleGetStackByPos(hex, true));
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

    // 0th slot is gi+0..gi+ATTRS_PER_STACK
    int i = gi + slot*ATTRS_PER_STACK;

    std::string prefix = "";

    if (stack->unitSide() == astack->unitSide()) {
      prefix = "F" + std::to_string(slot+1) + " ";
    } else {
      // enemy units in state are in "slots" 8..14
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

    state[i++] = NValue(prefix + "qty", stack->getCount(), MIN_QTY, MAX_QTY);
    state[i++] = NValue(prefix + "att", stack->getAttack(false), MIN_ATT, MAX_ATT);
    state[i++] = NValue(prefix + "def", stack->getDefense(false), MIN_DEF, MAX_DEF);
    state[i++] = NValue(prefix + "shots", stack->shots.available(), MIN_SHOTS, MAX_SHOTS);
    state[i++] = NValue(prefix + "dmg (min, melee)", stack->getMinDamage(false), MIN_DMG, MAX_DMG);
    state[i++] = NValue(prefix + "dmg (max, melee)", stack->getMaxDamage(false), MIN_DMG, MAX_DMG);
    state[i++] = NValue(prefix + "dmg (min, ranged)", stack->getMinDamage(true), MIN_DMG, MAX_DMG);
    state[i++] = NValue(prefix + "dmg (min, ranged)", stack->getMaxDamage(true), MIN_DMG, MAX_DMG);
    state[i++] = NValue(prefix + "HP", stack->getMaxHealth(), MIN_HP, MAX_HP);
    state[i++] = NValue(prefix + "HP left", stack->getFirstHPleft(), MIN_HP, MAX_HP);
    state[i++] = NValue(prefix + "speed", stack->speed(), MIN_SPEED, MAX_SPEED);
    state[i++] = NValue(prefix + "waited", stack->waitedThisTurn, 0, 1);

    // TODO:
    // More feature extraction to consider:
    //
    //     cb->battleGetTurnOrder();
    //     astack->canMove()
    //     astack->canShoot()
    //     astack->willMove()
    //     astack->ableToRetaliate()
    //     astack->movedThisRound

    assert(i == i0 + ATTRS_PER_STACK);
  }

  gi += (14 * ATTRS_PER_STACK);

  state[gi++] = NValue("active stack", astack->unitSlot(), 0, 7);

  assert(gi == state.size());
  return state;
}


void addError(ErrType type, ErrMask &errmask, std::vector<std::string> &errmsgs) {
  auto& [flag, name, msg] = ERRORS.at(type);
  errmask |= flag;
  errmsgs.emplace_back("(" + name + ") " + msg);
}

ActionResult BAI::buildAction(const CStack * astack, State state, Action action) {
  ErrMask errmask = 0;
  std::vector<std::string> errmsgs {};

  if (action == 0) {
    // TODO: handle if retreat is impossible (can't know in advance)
    //       Will it be a query dialog?
    RETURN_ACT_OR_ERR(BattleAction::makeRetreat(cb->battleGetMySide()))
  }
  if (action == 1) {
    RETURN_ACT_OR_ERR(BattleAction::makeDefend(astack))
  }
  if (action == 2) {
    if (astack->waitedThisTurn) {
      // TODO: assert state actually reported that the stack has waited
      addError(ERR_ALREADY_WAITED, errmask, errmsgs);
    }

    RETURN_ACT_OR_ERR(BattleAction::makeWait(astack))
  }

  auto subaction = (action-3) % 8;
  auto y = ((action-3) / 8) / BF_XMAX;
  auto x = ((action-3) / 8) % BF_XMAX;
  auto dest = BattleHex(x + 1, y); // "real" hex is offset by 1 (left side col)
  auto hexstate = state[x + y*BF_XMAX];

  // self-destination is OK if shooting, or if attacking a neighbour
  auto ownhexes = astack->getHexes();
  auto destself = (std::find(ownhexes.begin(), ownhexes.end(), dest) != ownhexes.end());

  if (destself && subaction == 0) {
    // move-only does not allow "moving" to self
    addError(ERR_MOVE_SELF, errmask, errmsgs);
  } else if (!destself) {
    if (hexstate.orig == static_cast<int>(HexState::FREE_UNREACHABLE))
      addError(ERR_HEX_UNREACHABLE, errmask, errmsgs);
    else if (hexstate.orig != static_cast<int>(HexState::FREE_REACHABLE))
      addError(ERR_HEX_BLOCKED, errmask, errmsgs);
  }

  // just move
  if (subaction == 0) {
    RETURN_ACT_OR_ERR(BattleAction::makeMove(astack, dest))
  }

  // move and attack enemy unit 0..6
  auto slot = subaction - 1;
  auto targets = cb->battleGetStacksIf([&astack, &slot](const CStack * stack) {
    return stack->unitSlot() == slot && stack->getOwner() != astack->getOwner();
  });

  if (targets.empty()) {
    addError(ERR_STACK_NA, errmask, errmsgs);
    return {nullptr, errmask, errmsgs};
  }

  assert(targets.size() <= 1);
  auto stack = targets[0];

  if (!stack->alive())
    addError(ERR_STACK_DEAD, errmask, errmsgs);
  else if (!stack->isValidTarget())
    addError(ERR_STACK_INVALID, errmask, errmsgs);

  if (cb->battleCanShoot(astack)) {
    if (!destself)
      addError(ERR_MOVE_SHOOT, errmask, errmsgs);

    RETURN_ACT_OR_ERR(BattleAction::makeShotAttack(astack, stack));
  }

  if (!astack->isMeleeAttackPossible(astack, stack))
    addError(ERR_ATTACK_IMPOSSIBLE, errmask, errmsgs);

  RETURN_ACT_OR_ERR(BattleAction::makeMeleeAttack(astack, stack->position, dest));
}

void BAI::battleStacksAttacked(const std::vector<BattleStackAttacked> & bsa, bool ranged)
{
  info("*** battleStacksAttacked ***");

  for(auto & elem : bsa) {
    auto * defender = cb->battleGetStackByID(elem.stackAttacked, false);

    auto dmg = elem.damageAmount;
    auto units = elem.killedAmount;

    // TODO: AIValue is unclear how it's calculated (maybe builtin the game?)
    //       Maybe use a generic calc, eg:
    //       BaseHP + AvgDmg + att + def + speed*2 + bonuses (eg. archer/marskman)
    //       ... but that sounds too complicated
    //
    auto value = elem.killedAmount * defender->creatureId().toCreature()->getAIValue();

    // attacker dealing dmg might be our friendly fire
    // => use defender
    if (defender->getOwner() == cb->getPlayerID()) {
      // note: in VCMI there is no excess dmg if stack is killed
      result.dmgReceived += dmg;
      result.unitsLost += units;
      result.valueLost += value;
    } else {
      result.dmgDealt += dmg;
      result.unitsKilled += units;
      result.valueKilled += value;
    }
  }
}

void BAI::battleStart(const CCreatureSet *army1, const CCreatureSet *army2, int3 tile, const CGHeroInstance *hero1, const CGHeroInstance *hero2, bool Side, bool replayAllowed)
{
  info("*** battleStart ***");
  side = Side;
  initStackHNSMap();
}

void BAI::battleEnd(const BattleResult *br, QueryID queryID) {
  info("*** battleEnd ***");

  result.ended = true;
  result.victory = br->winner == cb->battleGetMySide();
  result.type = ResultType::REGULAR; // BATTLE_END?
}

MMAI_NS_END
