#include "BAI.h"

// Contains boilerplate related implementations of virtual methods

MMAI_NS_BEGIN

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
  initStackHNSMap();
}

void BAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  print("battleCatapultAttacked called");
}

MMAI_NS_END
