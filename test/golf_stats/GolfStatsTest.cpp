#include <gtest/gtest.h>

#include <utility>

#include "GolfPenalty.h"
#include "GolfStats.h"

namespace {
GolfRound partialRound() {
  GolfRound round{};
  round.holeCount = 18;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  round.par[0] = 4;
  round.par[1] = 3;
  round.par[2] = 5;
  round.putts[0] = 1;
  round.in100[0] = 2;
  round.out100[0] = 3;
  round.putts[1] = 8;
  round.putts[2] = 3;
  round.in100[2] = 4;
  round.out100[2] = 3;
  return round;
}

TEST(GolfStats, TotalsIncludeEnteredHolesOnly) {
  const auto round = partialRound();
  EXPECT_EQ(golfScore(round), 12);
  EXPECT_EQ(golfParTotal(round), 9);
  EXPECT_TRUE(golfHasPar(round));
  EXPECT_EQ(golfToPar(round), 3);
  EXPECT_EQ(golfThru(round), 2);
  EXPECT_EQ(golfPuttsTotal(round), 4);
  EXPECT_EQ(golfIn100Total(round), 6);
  EXPECT_EQ(golfShortTotal(round), 2);
  EXPECT_EQ(golfLongTotal(round), 6);
  EXPECT_EQ(golfOnePutts(round), 1);
  EXPECT_EQ(golfThreePutts(round), 1);
}

TEST(GolfStats, ParFreeRoundKeepsScoresWithoutParStatistics) {
  GolfRound round{};
  round.holeCount = 18;
  round.putts[0] = 2;
  round.in100[0] = 3;
  round.out100[0] = 2;
  EXPECT_EQ(golfHoleScore(round, 0), 5);
  EXPECT_EQ(golfScore(round), 5);
  EXPECT_EQ(golfParTotal(round), 0);
  EXPECT_EQ(golfToPar(round), 0);
  EXPECT_FALSE(golfHasPar(round));
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  EXPECT_EQ(golfWorstHoles(round, worst, GolfRound::MAX_HOLES), 0);
}

TEST(GolfStats, BucketIdentityHoldsPerEnteredHole) {
  const auto round = partialRound();
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (golfHoleScore(round, hole) == 0) continue;
    const uint16_t shortGame = round.in100[hole] - round.putts[hole];
    EXPECT_EQ(static_cast<uint16_t>(golfLongGame(round, hole)) + shortGame + round.putts[hole],
              golfHoleScore(round, hole));
  }
}

TEST(GolfStats, NineHoleRoundIgnoresLaterEntries) {
  auto round = partialRound();
  round.holeCount = 9;
  round.in100[9] = 50;
  round.out100[9] = 49;
  EXPECT_EQ(golfScore(round), 12);
  EXPECT_EQ(golfThru(round), 2);
}

TEST(GolfStats, WorstHolesAreSortedRelativeToPar) {
  const auto round = partialRound();
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  const uint8_t count = golfWorstHoles(round, worst, GolfRound::MAX_HOLES);
  ASSERT_EQ(count, 2);
  EXPECT_EQ(worst[0].hole, 2);
  EXPECT_EQ(worst[0].toPar, 2);
  EXPECT_EQ(worst[1].hole, 0);
  EXPECT_EQ(worst[1].toPar, 1);
}

TEST(GolfStats, EveryScoreFigureIncludesPenaltyStrokes) {
  GolfRound round{};
  round.holeCount = 18;
  round.par[0] = 4;
  round.in100[0] = 2;
  round.out100[0] = 3;
  const uint16_t scoreBefore = golfScore(round);
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);

  EXPECT_EQ(golfHoleScore(round, 0), 8);
  EXPECT_EQ(golfScore(round), scoreBefore + 3);  // one added field shot plus two OB strokes
  EXPECT_EQ(golfToPar(round), 4);
  EXPECT_EQ(golfPenaltyTotal(round), 2);
  EXPECT_EQ(golfParTotal(round), 4);
  EXPECT_EQ(golfThru(round), 1);
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  ASSERT_EQ(golfWorstHoles(round, worst, GolfRound::MAX_HOLES), 1);
  EXPECT_EQ(worst[0].toPar, 4);
}

TEST(GolfStats, WorkedPenaltyHolesUseDerivedStrokeArithmetic) {
  for (const auto [kind, expected] : {std::pair{GolfPenaltyKind::Hazard, 6}, std::pair{GolfPenaltyKind::Ob, 7}}) {
    GolfRound round{};
    round.holeCount = 18;
    round.par[0] = 4;
    ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, kind), GolfPenaltyMutationStatus::Changed);
    ASSERT_TRUE(incrementGolfCounter(round, 0, GolfField::Out100).changed);
    ASSERT_TRUE(incrementGolfCounter(round, 0, GolfField::Out100).changed);
    ASSERT_TRUE(incrementGolfCounter(round, 0, GolfField::Putts).changed);
    ASSERT_TRUE(incrementGolfCounter(round, 0, GolfField::Putts).changed);
    EXPECT_EQ(golfHoleScore(round, 0), expected);
  }
}
}  // namespace
