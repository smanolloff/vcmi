/*
 * BattleCheckpointTest.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "test/StdInc.h"
#include "test/server/battles/BattleTestFixture.h"

#include "lib/battle/BattleAction.h"
#include "lib/callback/GameRandomizer.h"
#include "lib/serializer/CMemorySerializer.h"
#include "server/CGameHandler.h"
#include "server/activities/ActivityProcessor.h"
#include "server/activities/BattleActivities.h"

namespace
{
class BattleCheckpointTest : public BattleTestFixture
{
protected:
	std::vector<std::byte> rngState() const
	{
		CMemorySerializer serializer;
		serializer.oser & *gameHandler->randomizer;
		return serializer.extractBuffer();
	}
};

TEST_F(BattleCheckpointTest, RestoresIndependentBranchesAfterACombatActionAndTerminalResult)
{
	startGame();
	ML::Config config;
	config.statsMode = "disabled";
	gameHandler->mlplugin = std::make_shared<ML::ServerPlugin>(gameHandler.get(), gameState().get(), config);
	startBattle();
	gameHandler->activities->addActivity(std::make_shared<BattleActivity>(gameHandler.get(), battle()));

	const CreatureID pikeman = attackerSideHero->getStack(SlotID(0)).getId();
	auto * attacker = addStack(BattleSide::ATTACKER, pikeman, BattleHex(leftHex - 1), 10);
	auto * defender = addStack(BattleSide::DEFENDER, pikeman, BattleHex(rightHex), 100);
	ASSERT_NE(attacker, nullptr);
	ASSERT_NE(defender, nullptr);
	const auto attackerID = attacker->unitId();
	const auto defenderID = defender->unitId();
	blockRetaliation(attacker);
	beginCombat();
	battle()->moveUnit(attackerID, BattleHex(leftHex));
	battle()->activeStack = attackerID;
	teachSpell(attackerSideHero, SpellID::MAGIC_ARROW);

	auto & plugin = *gameHandler->mlplugin;
	const auto battleID = battle()->getBattleID();
	const auto mana = attackerSideHero->mana;
	const auto health = defender->getAvailableHealth();
	const auto round = battle()->getRound();
	const auto capture = plugin.captureCheckpoint(battleID);
	EXPECT_NE(capture, nullptr);
	EXPECT_FALSE(plugin.isRollingOut(battleID));
	EXPECT_EQ(battle()->getActiveStackID(), attackerID);

	const auto root = plugin.checkpoint(battleID);

	ASSERT_TRUE(castAsHero(attackerSideHero, SpellID::MAGIC_ARROW, defender));
	ASSERT_TRUE(attack(attacker, BattleHex(rightHex)));
	const auto branchHealth = defender->getAvailableHealth();
	const auto branchMana = attackerSideHero->mana;
	const auto branchRng = rngState();
	const auto branchRound = battle()->getRound();
	EXPECT_LT(branchHealth, health);
	EXPECT_LT(branchMana, mana);

	plugin.restore(*root);
	EXPECT_EQ(battle()->getRound(), round);
	EXPECT_EQ(battle()->getActiveStackID(), attackerID);
	EXPECT_EQ(battle()->getStack(attackerID)->getPosition(), BattleHex(leftHex));
	EXPECT_EQ(attackerSideHero->mana, mana);
	EXPECT_EQ(battle()->getStack(defenderID)->getAvailableHealth(), health);
	EXPECT_TRUE(battle()->getUsedSpells(BattleSide::ATTACKER).empty());

	ASSERT_TRUE(act(BattleAction::makeRetreat(BattleSide::ATTACKER)));
	const auto result = plugin.rolloutResult(battleID);
	ASSERT_TRUE(result);
	EXPECT_EQ(result->result, EBattleResult::ESCAPE);
	EXPECT_EQ(result->winner, BattleSide::DEFENDER);
	EXPECT_TRUE(server.battleResults.empty());
	EXPECT_FALSE(act(BattleAction::makeRetreat(BattleSide::ATTACKER)));
	ASSERT_EQ(gameState()->currentBattles.size(), 1u);

	plugin.restore(*root);
	EXPECT_FALSE(plugin.rolloutResult(battleID));
	ASSERT_TRUE(castAsHero(attackerSideHero, SpellID::MAGIC_ARROW, battle()->getStack(defenderID)));
	ASSERT_TRUE(attack(battle()->getStack(attackerID), BattleHex(rightHex)));
	EXPECT_EQ(battle()->getStack(defenderID)->getAvailableHealth(), branchHealth);
	EXPECT_EQ(attackerSideHero->mana, branchMana);
	EXPECT_EQ(battle()->getRound(), branchRound);
	EXPECT_EQ(rngState(), branchRng);

	plugin.finishRollout(*root);
	EXPECT_FALSE(plugin.isRollingOut(battleID));
	EXPECT_EQ(attackerSideHero->mana, mana);
	EXPECT_EQ(battle()->getStack(defenderID)->getAvailableHealth(), health);

	ASSERT_TRUE(act(BattleAction::makeRetreat(BattleSide::ATTACKER)));
	ASSERT_EQ(server.battleResults.size(), 1u);
	EXPECT_EQ(server.battleResults.front().result, EBattleResult::ESCAPE);
	EXPECT_THROW(plugin.checkpoint(battleID), std::logic_error);
}
}
