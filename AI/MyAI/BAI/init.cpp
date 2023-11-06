#include "BAI.h"
#include "mytypes.h"

// Contains initialization-related code

MMAI_NS_BEGIN

BAI::BAI()
  : side(-1),
  wasWaitingForRealize(false),
  wasUnlockingGs(false),
  hexStateNames(initHexStateNames()),
  hexStateValues(initHexStateValues()),
  hexStateNMap(initHexStateNMap()),
  allBattleHexes(initAllBattleHexes())
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

const std::map<HexState, std::string> BAI::initHexStateNames() {
  std::map<HexState, std::string> res = {
    {HexState::FREE_REACHABLE,   "FREE_REACHABLE"},
    {HexState::FREE_UNREACHABLE, "FREE_UNREACHABLE"},
    {HexState::OBSTACLE,         "OBSTACLE"},
    {HexState::FRIENDLY_STACK_1, "FRIENDLY_STACK_1"},
    {HexState::FRIENDLY_STACK_2, "FRIENDLY_STACK_2"},
    {HexState::FRIENDLY_STACK_3, "FRIENDLY_STACK_3"},
    {HexState::FRIENDLY_STACK_4, "FRIENDLY_STACK_4"},
    {HexState::FRIENDLY_STACK_5, "FRIENDLY_STACK_5"},
    {HexState::FRIENDLY_STACK_6, "FRIENDLY_STACK_6"},
    {HexState::FRIENDLY_STACK_7, "FRIENDLY_STACK_7"},
    {HexState::ENEMY_STACK_1,    "ENEMY_STACK_1"},
    {HexState::ENEMY_STACK_2,    "ENEMY_STACK_2"},
    {HexState::ENEMY_STACK_3,    "ENEMY_STACK_3"},
    {HexState::ENEMY_STACK_4,    "ENEMY_STACK_4"},
    {HexState::ENEMY_STACK_5,    "ENEMY_STACK_5"},
    {HexState::ENEMY_STACK_6,    "ENEMY_STACK_6"},
    {HexState::ENEMY_STACK_7,    "ENEMY_STACK_7"},
  };

  assert(res.size() == static_cast<int>(HexState::count));
  return res;
}

const std::map<std::string, HexState> BAI::initHexStateValues() {
  std::map<std::string, HexState> tmpres = {};

  // Invert the key-value pairs
  for (const auto& pair : hexStateNames) {
      tmpres[pair.second] = pair.first;
  }

  return tmpres;
}

const std::array<NValue, static_cast<int>(HexState::count)> BAI::initHexStateNMap() {
  std::array<NValue, static_cast<int>(HexState::count)> res = {};

  int vmax = static_cast<int>(HexState::count) - 1;

  for (const auto& kv : hexStateNames) {
    auto &hs = kv.first;
    auto &hsname = kv.second;
    auto i = static_cast<int>(hs);
    res[i] = NValue(hsname, i, 0, vmax);
  }

  return res;
}

std::array<BattleHex, 15*11> BAI::initAllBattleHexes() {
  std::array<BattleHex, 15*11> res = {};

  // 0 and 16 are unreachable "side" hexes => exclude
  int i = 0;

  for(int y=0; y<=10; y++) {
    for(int x=1; x<=15; x++) {
      res[i++] = BattleHex(x, y);
    }
  }

  return res;
}

void BAI::initStackHNSMap() {
  stackHNSMap = {};
  auto allstacks = cb->battleGetAllStacks();

  for (int i=0; i < allstacks.size(); i++) {
    auto stack = allstacks[i];
    auto slot = stack->unitSlot();

    assert(slot >= 0 && slot < 7); // same as assert(slot.validSlot())

    auto hsname = (stack->getOwner() == side)
      ? "FRIENDLY_STACK_"
      : "ENEMY_STACK_";

    HexState hs = hexStateValues.at(hsname + std::to_string(slot + 1));
    // HexState hs = hexStateValues[hsname + slot + 1];

    stackHNSMap[stack] = hexStateNMap.at(static_cast<size_t>(hs));
  }
}

MMAI_NS_END
