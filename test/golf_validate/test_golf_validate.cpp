#include <gtest/gtest.h>

#include <cstring>

#include "GolfRules.h"
#include "GolfValidate.h"

namespace {

GolfRound makeRound() {
  GolfRound round{};
  round.holeCount = 18;
  return round;
}

}  // namespace

TEST(GolfValidate, ClearsShortGameCountsOnUnenteredHole) {
  GolfRound round = makeRound();
  round.putts[2] = 2;
  round.in100[2] = 3;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_EQ(round.putts[2], 0);
  EXPECT_EQ(round.in100[2], 0);
  EXPECT_TRUE(result.holePuttsRepaired(2));
  EXPECT_TRUE(result.holeIn100Repaired(2));
}

TEST(GolfValidate, ReducesIn100BeforePuttsAndPreservesStrokes) {
  GolfRound round = makeRound();
  round.strokes[4] = 3;
  round.putts[4] = 4;
  round.in100[4] = 2;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_EQ(round.strokes[4], 3);
  EXPECT_EQ(round.in100[4], 0);
  EXPECT_EQ(round.putts[4], 3);
  EXPECT_TRUE(result.holeIn100Repaired(4));
  EXPECT_TRUE(result.holePuttsRepaired(4));
}

TEST(GolfValidate, RepairsCorruptionBeforeRulesMutation) {
  GolfRound round = makeRound();
  round.putts[0] = 1;
  ASSERT_TRUE(validateGolfRound(round).valid);
  ASSERT_EQ(round.putts[0], 0);
  const GolfMutationResult result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_EQ(round.strokes[0], 1);
  EXPECT_EQ(round.putts[0], 1);
  EXPECT_LE(static_cast<uint16_t>(round.putts[0]) + round.in100[0], round.strokes[0]);
}

TEST(GolfValidate, RejectsUnsupportedHoleCountWithoutMutation) {
  GolfRound round = makeRound();
  round.holeCount = 12;
  round.currentHole = 15;
  round.strokes[0] = 1;
  round.putts[0] = 2;
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

TEST(GolfValidate, LeavesValidRoundUnchangedAndReportsNoRepairs) {
  GolfRound round = makeRound();
  round.currentHole = 7;
  round.strokes[0] = 5;
  round.putts[0] = 2;
  round.in100[0] = 1;
  const GolfRound before = round;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.repaired());
  EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
}
