#include <gtest/gtest.h>

#include "GolfStats.h"

namespace {

GolfRound partialRound() {
  GolfRound round{};
  round.holeCount = 18;
  round.par[0] = 4;
  round.par[1] = 3;
  round.par[2] = 5;
  round.strokes[0] = 5;
  round.putts[0] = 1;
  round.in100[0] = 2;
  round.putts[1] = 8;
  round.in100[1] = 7;
  round.strokes[2] = 7;
  round.putts[2] = 3;
  round.in100[2] = 1;
  return round;
}

TEST(GolfStats, EveryTotalIncludesEnteredHolesOnlyAndSkipsMiddleHole) {
  const auto round = partialRound();
  EXPECT_EQ(golfScore(round), 12);
  EXPECT_EQ(golfToPar(round), 3);
  EXPECT_EQ(golfThru(round), 2);
  EXPECT_EQ(golfPuttsTotal(round), 4);
  EXPECT_EQ(golfIn100Total(round), 3);
  EXPECT_EQ(golfLongTotal(round), 5);
  EXPECT_EQ(golfOnePutts(round), 1);
  EXPECT_EQ(golfThreePutts(round), 1);
}

TEST(GolfStats, LongGameIsDerivedAndBucketIdentityHoldsPerEnteredHole) {
  const auto round = partialRound();
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (round.strokes[hole] == 0) {
      continue;
    }
    EXPECT_EQ(static_cast<uint16_t>(golfLongGame(round, hole)) + round.putts[hole] + round.in100[hole],
              round.strokes[hole]);
  }
}

TEST(GolfStats, NineHoleRoundIgnoresLaterArrayEntries) {
  auto round = partialRound();
  round.holeCount = 9;
  round.strokes[9] = 99;
  round.par[9] = 1;
  round.putts[9] = 50;
  round.in100[9] = 40;
  EXPECT_EQ(golfScore(round), 12);
  EXPECT_EQ(golfToPar(round), 3);
  EXPECT_EQ(golfThru(round), 2);
  EXPECT_EQ(golfPuttsTotal(round), 4);
  EXPECT_EQ(golfIn100Total(round), 3);
  EXPECT_EQ(golfLongTotal(round), 5);
  EXPECT_EQ(golfOnePutts(round), 1);
  EXPECT_EQ(golfThreePutts(round), 1);
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  EXPECT_EQ(golfWorstHoles(round, worst, GolfRound::MAX_HOLES), 2);
}

TEST(GolfStats, WorstHolesAreEnteredAndSortedByScoreRelativeToPar) {
  const auto round = partialRound();
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  const uint8_t count = golfWorstHoles(round, worst, GolfRound::MAX_HOLES);
  ASSERT_EQ(count, 2);
  EXPECT_EQ(worst[0].hole, 2);
  EXPECT_EQ(worst[0].toPar, 2);
  EXPECT_EQ(worst[1].hole, 0);
  EXPECT_EQ(worst[1].toPar, 1);
}

TEST(GolfStats, WorstHolesHonorsCallerCapacityWithoutAllocation) {
  const auto round = partialRound();
  GolfWorstHole worst[1]{};
  EXPECT_EQ(golfWorstHoles(round, worst, 1), 1);
  EXPECT_EQ(worst[0].hole, 2);
}

}  // namespace
