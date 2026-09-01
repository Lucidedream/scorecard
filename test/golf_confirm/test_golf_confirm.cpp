#include <gtest/gtest.h>

#include "GolfConfirm.h"
#include "GolfRules.h"
#include "GolfStats.h"

namespace {

class GolfConfirmTest : public ::testing::Test {
 protected:
  GolfRound round{};

  void SetUp() override {
    initializeGolfPlayerDefaults(round);
    round.holeCount = 18;
    round.players[0].tee = TeeSelection::Blue;
    round.currentPlayer = 0;
    for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) round.par[hole] = 4;
  }

  GolfPlayerScore& score(const uint8_t player = 0) { return round.players[player].score; }

  GolfConfirmAction press(const GolfField focusedField, const uint8_t player = 0, const uint8_t hole = 0) {
    const bool logged = golfHoleScore(round, score(player), hole) != 0;
    const bool canCommit = !logged && round.par[hole] >= 3;
    return golfConfirmPress(focusedField, logged, canCommit);
  }
};

TEST_F(GolfConfirmTest, PuttsCyclesFocus) {
  EXPECT_EQ(press(GolfField::Putts), GolfConfirmAction::CycleFocus);
}

TEST_F(GolfConfirmTest, In100CyclesFocus) {
  EXPECT_EQ(press(GolfField::In100), GolfConfirmAction::CycleFocus);
}

TEST_F(GolfConfirmTest, Out100UnloggedWithValuesCommitsAndAdvances) {
  EXPECT_EQ(press(GolfField::Out100), GolfConfirmAction::CommitAndAdvance);
}

TEST_F(GolfConfirmTest, Out100LoggedAdvancesWithoutCommit) {
  incrementGolfCounter(score(), 0, GolfField::Out100);
  ASSERT_NE(golfHoleScore(round, score(), 0), 0);
  EXPECT_EQ(press(GolfField::Out100), GolfConfirmAction::AdvanceWithoutCommit);
}

TEST_F(GolfConfirmTest, Out100UnloggedWithoutValuesAdvancesAndStaysUnlogged) {
  round.par[0] = 0;
  ASSERT_EQ(golfHoleScore(round, score(), 0), 0);
  EXPECT_EQ(press(GolfField::Out100), GolfConfirmAction::AdvanceWithoutCommit);
  EXPECT_EQ(golfHoleScore(round, score(), 0), 0);
}

TEST_F(GolfConfirmTest, BlankAdvanceMovesToNextPlayerWithoutEnteringScore) {
  round.par[0] = 0;
  round.players[1].tee = TeeSelection::White;
  ASSERT_EQ(press(GolfField::Out100), GolfConfirmAction::AdvanceWithoutCommit);
  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentHole, 0);
  EXPECT_EQ(round.currentPlayer, 1);
  EXPECT_EQ(golfHoleScore(round, score(0), 0), 0);
}

TEST_F(GolfConfirmTest, CommittedValuesEqualParReconstruction) {
  for (const uint8_t par : {3, 4, 5}) {
    GolfPlayerScore& playerScore = score(static_cast<uint8_t>(par - 3));
    ASSERT_TRUE(seedGolfHoleAtPar(playerScore, 0, par));
    EXPECT_EQ(playerScore.putts[0], 2);
    EXPECT_EQ(playerScore.in100[0], 2);
    EXPECT_EQ(playerScore.out100[0], par - 2);
    EXPECT_EQ(golfHoleScore(round, playerScore, 0), par);
  }
}

TEST_F(GolfConfirmTest, LastEnabledPlayerAdvanceWrapsHole18ToHole1) {
  round.players[3].tee = TeeSelection::White;
  round.currentPlayer = 3;
  round.currentHole = 17;
  ASSERT_EQ(press(GolfField::Out100, 3, 17), GolfConfirmAction::CommitAndAdvance);

  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentHole, 0);
  EXPECT_EQ(round.currentPlayer, 0);
}

}  // namespace
