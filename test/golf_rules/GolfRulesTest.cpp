#include <gtest/gtest.h>

#include "GolfRules.h"

namespace {

constexpr uint8_t bit(GolfField field) { return 1u << static_cast<uint8_t>(field); }

GolfRound roundWithOneHole() {
  GolfRound round{};
  round.holeCount = 1;
  round.par[0] = 4;
  return round;
}

TEST(GolfRules, IncrementStrokeMarksUnenteredHoleEntered) {
  auto round = roundWithOneHole();
  const auto result = incrementGolfCounter(round, 0, GolfField::Strokes);
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.blocked);
  EXPECT_EQ(result.blockingFields, 0);
  EXPECT_FALSE(result.autoBumpedStrokes);
  EXPECT_EQ(round.strokes[0], 1);
}

TEST(GolfRules, IncrementPuttsAtCeilingAlsoIncrementsStrokes) {
  auto round = roundWithOneHole();
  round.strokes[0] = 2;
  round.putts[0] = 1;
  round.in100[0] = 1;
  const auto result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.autoBumpedStrokes);
  EXPECT_EQ(round.strokes[0], 3);
  EXPECT_EQ(round.putts[0], 2);
}

TEST(GolfRules, IncrementIn100AtCeilingAlsoIncrementsStrokes) {
  auto round = roundWithOneHole();
  round.strokes[0] = 2;
  round.putts[0] = 1;
  round.in100[0] = 1;
  const auto result = incrementGolfCounter(round, 0, GolfField::In100);
  EXPECT_TRUE(result.changed);
  EXPECT_TRUE(result.autoBumpedStrokes);
  EXPECT_EQ(round.strokes[0], 3);
  EXPECT_EQ(round.in100[0], 2);
}

TEST(GolfRules, IncrementShortCounterMarksUnenteredHoleEntered) {
  auto round = roundWithOneHole();
  for (const auto field : {GolfField::Putts, GolfField::In100}) {
    round.strokes[0] = 0;
    round.putts[0] = 0;
    round.in100[0] = 0;
    const auto result = incrementGolfCounter(round, 0, field);
    EXPECT_TRUE(result.changed);
    EXPECT_TRUE(result.autoBumpedStrokes);
    EXPECT_EQ(round.strokes[0], 1);
  }
}

TEST(GolfRules, IncrementShortCounterBelowCeilingDoesNotBumpStrokes) {
  auto round = roundWithOneHole();
  round.strokes[0] = 4;
  const auto result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.blocked);
  EXPECT_FALSE(result.autoBumpedStrokes);
  EXPECT_EQ(round.strokes[0], 4);
  EXPECT_EQ(round.putts[0], 1);
}

TEST(GolfRules, IncrementStrokeStopsAtNinetyNine) {
  auto round = roundWithOneHole();
  round.strokes[0] = 99;
  const auto result = incrementGolfCounter(round, 0, GolfField::Strokes);
  EXPECT_FALSE(result.changed);
  EXPECT_FALSE(result.blocked);
  EXPECT_EQ(result.blockingFields, 0);
  EXPECT_EQ(round.strokes[0], 99);
}

TEST(GolfRules, IncrementShortCounterStopsAtStrokeCeiling) {
  auto round = roundWithOneHole();
  round.strokes[0] = 99;
  round.putts[0] = 99;
  const auto result = incrementGolfCounter(round, 0, GolfField::Putts);
  EXPECT_FALSE(result.changed);
  EXPECT_FALSE(result.blocked);
  EXPECT_EQ(result.blockingFields, 0);
  EXPECT_EQ(round.putts[0], 99);
}

TEST(GolfRules, DecrementStrokeBelowPuttsAndIn100IsRefused) {
  auto round = roundWithOneHole();
  round.strokes[0] = 5;
  round.putts[0] = 2;
  round.in100[0] = 3;
  const auto result = decrementGolfCounter(round, 0, GolfField::Strokes);
  EXPECT_FALSE(result.changed);
  EXPECT_TRUE(result.blocked);
  EXPECT_EQ(result.blockingFields, bit(GolfField::Putts) | bit(GolfField::In100));
  EXPECT_EQ(round.strokes[0], 5);
}

TEST(GolfRules, In100IsReportedWhenItAloneHoldsStrokeFloor) {
  auto round = roundWithOneHole();
  round.strokes[0] = 2;
  round.in100[0] = 2;
  const auto result = decrementGolfCounter(round, 0, GolfField::Strokes);
  EXPECT_TRUE(result.blocked);
  EXPECT_EQ(result.blockingFields, bit(GolfField::In100));
}

TEST(GolfRules, PuttsIsReportedWhenItAloneHoldsStrokeFloor) {
  auto round = roundWithOneHole();
  round.strokes[0] = 2;
  round.putts[0] = 2;
  const auto result = decrementGolfCounter(round, 0, GolfField::Strokes);
  EXPECT_TRUE(result.blocked);
  EXPECT_EQ(result.blockingFields, bit(GolfField::Putts));
}

TEST(GolfRules, DecrementAnyCounterBelowZeroIsRefused) {
  auto round = roundWithOneHole();
  for (const auto field : {GolfField::Strokes, GolfField::Putts, GolfField::In100}) {
    const auto result = decrementGolfCounter(round, 0, field);
    EXPECT_FALSE(result.changed);
    EXPECT_FALSE(result.blocked);
    EXPECT_EQ(result.blockingFields, 0);
  }
}

TEST(GolfRules, DecrementEachCounterWhenAllowed) {
  auto round = roundWithOneHole();
  round.strokes[0] = 4;
  round.putts[0] = 1;
  round.in100[0] = 1;
  EXPECT_TRUE(decrementGolfCounter(round, 0, GolfField::Strokes).changed);
  EXPECT_TRUE(decrementGolfCounter(round, 0, GolfField::Putts).changed);
  EXPECT_TRUE(decrementGolfCounter(round, 0, GolfField::In100).changed);
  EXPECT_EQ(round.strokes[0], 3);
  EXPECT_EQ(round.putts[0], 0);
  EXPECT_EQ(round.in100[0], 0);
}

TEST(GolfRules, FocusCyclesInContractOrder) {
  EXPECT_EQ(nextGolfField(GolfField::Strokes), GolfField::Putts);
  EXPECT_EQ(nextGolfField(GolfField::Putts), GolfField::In100);
  EXPECT_EQ(nextGolfField(GolfField::In100), GolfField::Strokes);
}

}  // namespace
