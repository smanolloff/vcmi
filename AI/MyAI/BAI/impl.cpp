#include "BAI.h"

// Contains boilerplate related implementations of virtual methods

MMAI_NS_BEGIN

void BAI::actionFinished(const BattleAction &action)
{
  debug("*** actionFinished ***");
}

void BAI::actionStarted(const BattleAction &action)
{
  debug("*** actionStarted ***");
}

void BAI::yourTacticPhase(int distance)
{
  cb->battleMakeTacticAction(BattleAction::makeEndOFTacticPhase(cb->battleGetTacticsSide()));
}

void BAI::battleAttack(const BattleAttack *ba)
{
  debug("*** battleAttack ***");
}

void BAI::battleNewRoundFirst(int round)
{
  debug("*** battleNewRoundFirst ***");
}

void BAI::battleNewRound(int round)
{
  debug("*** battleNewRound ***");
}

void BAI::battleStackMoved(const CStack * stack, std::vector<BattleHex> dest, int distance, bool teleport)
{
  debug("*** battleStackMoved ***");
}

void BAI::battleSpellCast(const BattleSpellCast *sc)
{
  debug("*** battleSpellCast ***");
}

void BAI::battleStacksEffectsSet(const SetStackEffect & sse)
{
  debug("*** battleStacksEffectsSet ***");
}

void BAI::battleCatapultAttacked(const CatapultAttack & ca)
{
  debug("*** battleCatapultAttacked ***");
}

MMAI_NS_END
