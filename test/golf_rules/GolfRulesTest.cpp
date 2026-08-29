#include <gtest/gtest.h>

#include "GolfRules.h"

namespace {
GolfRound oneHole() {
  GolfRound round{};
  round.holeCount = 1;
  round.par[0] = 4;
  return round;
}

TEST(GolfRules, PuttsCarryIntoInside100) {
  auto round = oneHole();
  round.putts[0] = 2;
  round.in100[0] = 2;
  const auto result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.carriedIn100);
  EXPECT_EQ(round.putts[0], 3);
  EXPECT_EQ(round.in100[0], 3);
}

TEST(GolfRules, PuttsBelowInside100DoNotMoveIt) {
  auto round = oneHole();
  round.putts[0] = 1;
  round.in100[0] = 3;
  const auto result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.carriedIn100);
  EXPECT_EQ(round.putts[0], 2);
  EXPECT_EQ(round.in100[0], 3);
}

TEST(GolfRules, LoweringInside100CarriesPuttsDownAtFloor) {
  auto round = oneHole();
  round.putts[0] = 2;
  round.in100[0] = 2;
  const auto result = decrementGolfCounter(round, 0, GolfField::In100);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.loweredPutts);
  EXPECT_EQ(round.putts[0], 1);
  EXPECT_EQ(round.in100[0], 1);
}

TEST(GolfRules, CountersClampAtZeroAndNinetyNine) {
  auto round = oneHole();
  for (const auto field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    EXPECT_FALSE(decrementGolfCounter(round, 0, field).changed);
  }
  round.putts[0] = 99;
  round.in100[0] = 99;
  round.out100[0] = 99;
  for (const auto field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    EXPECT_FALSE(incrementGolfCounter(round, 0, field).changed);
  }
}

TEST(GolfRules, Out100ChangesIndependently) {
  auto round = oneHole();
  EXPECT_TRUE(incrementGolfCounter(round, 0, GolfField::Out100).changed);
  EXPECT_EQ(round.out100[0], 1);
  EXPECT_EQ(round.putts[0], 0);
  EXPECT_EQ(round.in100[0], 0);
  EXPECT_TRUE(decrementGolfCounter(round, 0, GolfField::Out100).changed);
}

TEST(GolfRules, SeedReconstructsPar) {
  auto round = oneHole();
  ASSERT_TRUE(seedGolfHoleAtPar(round, 0));
  EXPECT_EQ(round.putts[0], 2);
  EXPECT_EQ(round.in100[0], 2);
  EXPECT_EQ(round.out100[0], 2);
  EXPECT_FALSE(seedGolfHoleAtPar(round, 0));
}

TEST(GolfRules, ParFreeHoleDoesNotPreseed) {
  auto round = oneHole();
  round.par[0] = 0;
  EXPECT_FALSE(seedGolfHoleAtPar(round, 0));
  EXPECT_EQ(round.putts[0], 0);
  EXPECT_EQ(round.in100[0], 0);
  EXPECT_EQ(round.out100[0], 0);
}

TEST(GolfRules, FocusCyclesInEntryOrder) {
  EXPECT_EQ(nextGolfField(GolfField::Putts), GolfField::In100);
  EXPECT_EQ(nextGolfField(GolfField::In100), GolfField::Out100);
  EXPECT_EQ(nextGolfField(GolfField::Out100), GolfField::Putts);
}
}  // namespace
