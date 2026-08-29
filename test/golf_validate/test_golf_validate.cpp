#include <gtest/gtest.h>

#include <cstring>

#include "GolfValidate.h"

namespace {
GolfRound makeRound() {
  GolfRound round{};
  round.holeCount = 18;
  return round;
}
}  // namespace

TEST(GolfValidate, ClearsPuttsOnUnenteredHole) {
  GolfRound round = makeRound();
  round.putts[2] = 2;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_EQ(round.putts[2], 0);
  EXPECT_TRUE(result.holePuttsRepaired(2));
}

TEST(GolfValidate, ReducesPuttsToInside100Floor) {
  GolfRound round = makeRound();
  round.putts[4] = 4;
  round.in100[4] = 2;
  round.out100[4] = 3;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_EQ(round.putts[4], 2);
  EXPECT_EQ(round.in100[4], 2);
  EXPECT_EQ(round.out100[4], 3);
  EXPECT_TRUE(result.holePuttsRepaired(4));
}

TEST(GolfValidate, AcceptsZeroOutside100OnEnteredHole) {
  GolfRound round = makeRound();
  round.in100[0] = 1;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.repaired());
}

TEST(GolfValidate, RejectsUnsupportedHoleCountWithoutMutation) {
  GolfRound round = makeRound();
  round.holeCount = 12;
  round.currentHole = 15;
  round.out100[0] = 1;
  const GolfRound before = round;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
}

TEST(GolfValidate, ResetsOutOfRangeCurrentHole) {
  GolfRound round = makeRound();
  round.currentHole = 18;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.currentHoleReset);
  EXPECT_EQ(round.currentHole, 0);
}

TEST(GolfValidate, LeavesValidRoundUnchanged) {
  GolfRound round = makeRound();
  round.currentHole = 7;
  round.putts[0] = 2;
  round.in100[0] = 3;
  round.out100[0] = 2;
  const GolfRound before = round;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.repaired());
  EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
}
