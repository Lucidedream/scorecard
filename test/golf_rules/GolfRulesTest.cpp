#include <gtest/gtest.h>

#include <cstring>

#include "CourseStore.h"
#include "GolfRules.h"

namespace {

class GolfRulesTest : public ::testing::Test {
 protected:
  GolfRound round{};
  GolfCourse course{};

  void SetUp() override {
    initializeGolfPlayerDefaults(round);
    round.holeCount = 18;
    round.par[0] = 4;
  }

  GolfPlayerScore& score(const uint8_t player = 0) { return round.players[player].score; }
};

TEST_F(GolfRulesTest, PuttsCarryIntoInside100) {
  score().putts[0] = 2;
  score().in100[0] = 2;
  const auto result = incrementGolfCounter(score(), 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.carriedIn100);
  EXPECT_EQ(score().putts[0], 3);
  EXPECT_EQ(score().in100[0], 3);
}

TEST_F(GolfRulesTest, PuttsBelowInside100DoNotMoveIt) {
  score().putts[0] = 1;
  score().in100[0] = 3;
  const auto result = incrementGolfCounter(score(), 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.carriedIn100);
  EXPECT_EQ(score().putts[0], 2);
  EXPECT_EQ(score().in100[0], 3);
}

TEST_F(GolfRulesTest, LoweringInside100CarriesPuttsDownAtFloor) {
  score().putts[0] = 2;
  score().in100[0] = 2;
  const auto result = decrementGolfCounter(score(), 0, GolfField::In100);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.loweredPutts);
  EXPECT_EQ(score().putts[0], 1);
  EXPECT_EQ(score().in100[0], 1);
}

TEST_F(GolfRulesTest, CountersClampAtZeroAndNinetyNine) {
  for (const auto field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    EXPECT_FALSE(decrementGolfCounter(score(), 0, field).changed);
  }
  score().putts[0] = 99;
  score().in100[0] = 99;
  score().out100[0] = 99;
  for (const auto field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    EXPECT_FALSE(incrementGolfCounter(score(), 0, field).changed);
  }
}

TEST_F(GolfRulesTest, Out100ChangesIndependently) {
  EXPECT_TRUE(incrementGolfCounter(score(), 0, GolfField::Out100).changed);
  EXPECT_EQ(score().out100[0], 1);
  EXPECT_EQ(score().putts[0], 0);
  EXPECT_EQ(score().in100[0], 0);
  EXPECT_TRUE(decrementGolfCounter(score(), 0, GolfField::Out100).changed);
}

TEST_F(GolfRulesTest, CounterMutationRejectsStorageOutOfRange) {
  EXPECT_FALSE(incrementGolfCounter(score(), GolfRound::MAX_HOLES, GolfField::Out100).changed);
  EXPECT_FALSE(decrementGolfCounter(score(), GolfRound::MAX_HOLES, GolfField::Out100).changed);
}

TEST_F(GolfRulesTest, MutationOnlyTouchesExplicitPlayerScore) {
  score(1).putts[0] = 7;
  score(1).in100[0] = 8;
  score(1).out100[0] = 9;
  const GolfPlayerScore untouched = score(1);

  ASSERT_TRUE(incrementGolfCounter(score(0), 0, GolfField::Out100).changed);
  EXPECT_EQ(memcmp(&score(1), &untouched, sizeof(untouched)), 0);
}

TEST_F(GolfRulesTest, SeedReconstructsPar) {
  ASSERT_TRUE(seedGolfHoleAtPar(score(), 0, round.par[0]));
  EXPECT_EQ(score().putts[0], 2);
  EXPECT_EQ(score().in100[0], 2);
  EXPECT_EQ(score().out100[0], 2);
  EXPECT_FALSE(seedGolfHoleAtPar(score(), 0, round.par[0]));
}

TEST_F(GolfRulesTest, ParFreeHoleDoesNotPreseed) {
  round.par[0] = 0;
  EXPECT_FALSE(seedGolfHoleAtPar(score(), 0, round.par[0]));
  EXPECT_EQ(score().putts[0], 0);
  EXPECT_EQ(score().in100[0], 0);
  EXPECT_EQ(score().out100[0], 0);
}

TEST_F(GolfRulesTest, InitializesStableDefaultNamesWithoutEnablingPlayers) {
  EXPECT_STREQ(round.players[0].name, "Noah");
  EXPECT_STREQ(round.players[1].name, "Player B");
  EXPECT_STREQ(round.players[2].name, "Player C");
  EXPECT_STREQ(round.players[3].name, "Player D");
  for (const GolfPlayer& player : round.players) EXPECT_FALSE(golfPlayerIsEnabled(player));
}

TEST_F(GolfRulesTest, CourseApplySnapshotsSharedStrokeIndexAndDefaults) {
  memcpy(course.courseName, "Course", sizeof("Course"));
  course.holeCount = 18;
  course.par[0] = 4;
  course.si[0] = 7;
  course.hasSi = true;

  CourseStore::applyGolfCourse(course, round, 0);
  EXPECT_STREQ(round.courseName, "Course");
  EXPECT_EQ(round.par[0], 4);
  EXPECT_TRUE(round.hasSi);
  EXPECT_EQ(round.si[0], 7);
  EXPECT_STREQ(round.players[0].name, "Noah");
}

TEST_F(GolfRulesTest, CourseApplyLeavesMissingStrokeIndexZeroed) {
  course.holeCount = 9;
  course.si[0] = 7;
  course.hasSi = false;
  round.si[0] = 18;

  CourseStore::applyGolfCourse(course, round, 0);
  EXPECT_FALSE(round.hasSi);
  EXPECT_EQ(round.si[0], 0);
}

TEST_F(GolfRulesTest, EnabledPlayerTraversalPreservesStableSlots) {
  round.players[1].tee = TeeSelection::Blue;
  round.players[3].tee = TeeSelection::White;
  EXPECT_EQ(golfFirstEnabledPlayer(round), 1);
  EXPECT_EQ(golfNextEnabledPlayer(round, 1), 3);
  EXPECT_EQ(golfNextEnabledPlayer(round, 3), 1);
  EXPECT_EQ(golfPreviousEnabledPlayer(round, 1), 3);
  EXPECT_EQ(golfPreviousEnabledPlayer(round, 3), 1);
}

TEST_F(GolfRulesTest, SparseTwoPlayerTraversalIsSymmetric) {
  round.players[1].tee = TeeSelection::Blue;
  round.players[3].tee = TeeSelection::White;
  round.currentPlayer = 1;
  round.currentHole = 4;

  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 3);
  EXPECT_EQ(round.currentHole, 4);
  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 1);
  EXPECT_EQ(round.currentHole, 5);
  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 3);
  EXPECT_EQ(round.currentHole, 4);
  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 1);
  EXPECT_EQ(round.currentHole, 4);
}

TEST_F(GolfRulesTest, OnePlayerTraversalWrapsInBothDirections) {
  round.players[2].tee = TeeSelection::Blue;
  round.currentPlayer = 2;
  round.currentHole = 17;

  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 2);
  EXPECT_EQ(round.currentHole, 0);
  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 2);
  EXPECT_EQ(round.currentHole, 17);
}

TEST_F(GolfRulesTest, FourPlayerTraversalIsSymmetricAtPlayerBoundary) {
  for (GolfPlayer& player : round.players) player.tee = TeeSelection::Blue;
  round.currentPlayer = 0;
  round.currentHole = 8;

  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 3);
  EXPECT_EQ(round.currentHole, 7);
  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 0);
  EXPECT_EQ(round.currentHole, 8);
  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 1);
  EXPECT_EQ(round.currentHole, 8);
  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 0);
  EXPECT_EQ(round.currentHole, 8);
}

TEST_F(GolfRulesTest, MultiplayerFinalHoleWrapIsSymmetric) {
  round.players[0].tee = TeeSelection::Blue;
  round.players[2].tee = TeeSelection::White;
  round.currentPlayer = 2;
  round.currentHole = 17;

  ASSERT_TRUE(advanceGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 0);
  EXPECT_EQ(round.currentHole, 0);
  ASSERT_TRUE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentPlayer, 2);
  EXPECT_EQ(round.currentHole, 17);
}

TEST_F(GolfRulesTest, AdvanceRefusesDraftWithNoEnabledPlayers) {
  round.currentHole = 7;
  round.currentPlayer = 0;
  EXPECT_EQ(golfFirstEnabledPlayer(round), GolfRound::NO_PLAYER);
  EXPECT_EQ(golfNextEnabledPlayer(round, 0), GolfRound::NO_PLAYER);
  EXPECT_EQ(golfPreviousEnabledPlayer(round, 0), GolfRound::NO_PLAYER);
  EXPECT_FALSE(advanceGolfTurn(round));
  EXPECT_FALSE(retreatGolfTurn(round));
  EXPECT_EQ(round.currentHole, 7);
  EXPECT_EQ(round.currentPlayer, 0);
}

TEST(GolfRules, FocusCyclesInEntryOrder) {
  EXPECT_EQ(nextGolfField(GolfField::Putts), GolfField::In100);
  EXPECT_EQ(nextGolfField(GolfField::In100), GolfField::Out100);
  EXPECT_EQ(nextGolfField(GolfField::Out100), GolfField::Putts);
}

}  // namespace
