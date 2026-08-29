#include <gtest/gtest.h>

#include "GolfConfirm.h"
#include "GolfRules.h"
#include "GolfStats.h"

namespace {

GolfRound holeWithPar(const uint8_t par) {
  GolfRound round{};
  round.holeCount = 18;
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) round.par[hole] = par;
  return round;
}

GolfConfirmAction press(const GolfField focusedField, const GolfRound& round, const uint8_t hole) {
  const bool logged = golfHoleScore(round, hole) != 0;
  const bool canCommit = !logged && round.par[hole] >= 3;
  return golfConfirmPress(focusedField, logged, canCommit);
}

}  // namespace

TEST(GolfConfirm, PuttsCyclesFocus) {
  const GolfRound round = holeWithPar(4);
  EXPECT_EQ(press(GolfField::Putts, round, 0), GolfConfirmAction::CycleFocus);
}

TEST(GolfConfirm, In100CyclesFocus) {
  const GolfRound round = holeWithPar(4);
  EXPECT_EQ(press(GolfField::In100, round, 0), GolfConfirmAction::CycleFocus);
}

TEST(GolfConfirm, Out100UnloggedWithValuesCommitsAndAdvances) {
  const GolfRound round = holeWithPar(4);
  EXPECT_EQ(press(GolfField::Out100, round, 0), GolfConfirmAction::CommitAndAdvance);
}

TEST(GolfConfirm, Out100LoggedAdvancesWithoutCommit) {
  GolfRound round = holeWithPar(4);
  incrementGolfCounter(round, 0, GolfField::Out100);
  ASSERT_NE(golfHoleScore(round, 0), 0);

  EXPECT_EQ(press(GolfField::Out100, round, 0), GolfConfirmAction::AdvanceWithoutCommit);
}

TEST(GolfConfirm, Out100UnloggedWithoutValuesAdvancesAndStaysUnlogged) {
  GolfRound round = holeWithPar(0);
  ASSERT_EQ(golfHoleScore(round, 0), 0);
  EXPECT_EQ(press(GolfField::Out100, round, 0), GolfConfirmAction::AdvanceWithoutCommit);
  EXPECT_EQ(golfHoleScore(round, 0), 0);
}

TEST(GolfConfirm, CommittedValuesEqualParReconstruction) {
  for (const uint8_t par : {3, 4, 5}) {
    GolfRound round = holeWithPar(par);
    ASSERT_TRUE(seedGolfHoleAtPar(round, 0));
    EXPECT_EQ(round.putts[0], 2);
    EXPECT_EQ(round.in100[0], 2);
    EXPECT_EQ(round.out100[0], par - 2);
    EXPECT_EQ(golfHoleScore(round, 0), par);
  }
}

TEST(GolfConfirm, AdvanceWrapsHole18ToHole1LikeRight) {
  GolfRound round = holeWithPar(4);
  round.currentHole = 17;
  ASSERT_EQ(press(GolfField::Out100, round, round.currentHole), GolfConfirmAction::CommitAndAdvance);

  round.currentHole = static_cast<uint8_t>((round.currentHole + round.holeCount + 1) % round.holeCount);
  EXPECT_EQ(round.currentHole, 0);
}
