#include <gtest/gtest.h>

#include <utility>

#include "GolfPenalty.h"
#include "GolfScoringDisplay.h"
#include "GolfStats.h"

namespace {

class GolfStatsTest : public ::testing::Test {
 protected:
  GolfRound round{};

  void SetUp() override {
    initializeGolfPlayerDefaults(round);
    round.holeCount = 18;
    round.players[0].tee = TeeSelection::Blue;
    for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  }

  GolfPlayerScore& score(const uint8_t player = 0) { return round.players[player].score; }
  const GolfPlayerScore& score(const uint8_t player = 0) const { return round.players[player].score; }

  void fillPartialRound(const uint8_t player = 0) {
    round.par[0] = 4;
    round.par[1] = 3;
    round.par[2] = 5;
    score(player).putts[0] = 1;
    score(player).in100[0] = 2;
    score(player).out100[0] = 3;
    score(player).putts[1] = 8;
    score(player).putts[2] = 3;
    score(player).in100[2] = 4;
    score(player).out100[2] = 3;
  }
};

TEST_F(GolfStatsTest, TotalsIncludeEnteredHolesOnly) {
  fillPartialRound();
  EXPECT_EQ(golfScore(round, score()), 12);
  EXPECT_EQ(golfParTotal(round, score()), 9);
  EXPECT_TRUE(golfHasPar(round));
  EXPECT_EQ(golfToPar(round, score()), 3);
  EXPECT_EQ(golfThru(round, score()), 2);
  EXPECT_EQ(golfPuttsTotal(round, score()), 4);
  EXPECT_EQ(golfIn100Total(round, score()), 6);
  EXPECT_EQ(golfShortTotal(round, score()), 2);
  EXPECT_EQ(golfLongTotal(round, score()), 6);
  EXPECT_EQ(golfOnePutts(round, score()), 1);
  EXPECT_EQ(golfThreePutts(round, score()), 1);
}

TEST_F(GolfStatsTest, ParFreeRoundKeepsScoresWithoutParStatistics) {
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 0;
  score().putts[0] = 2;
  score().in100[0] = 3;
  score().out100[0] = 2;
  EXPECT_EQ(golfHoleScore(round, score(), 0), 5);
  EXPECT_EQ(golfScore(round, score()), 5);
  EXPECT_EQ(golfParTotal(round, score()), 0);
  EXPECT_EQ(golfToPar(round, score()), 0);
  EXPECT_FALSE(golfHasPar(round));
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  EXPECT_EQ(golfWorstHoles(round, score(), worst, GolfRound::MAX_HOLES), 0);
}

TEST_F(GolfStatsTest, ScoringHoleCellUsesCurrentHoleToParAndTracksCounterChanges) {
  round.currentHole = 0;
  score().in100[0] = 2;
  score().out100[0] = 3;
  EXPECT_EQ(static_cast<int16_t>(golfHoleScore(round, score(), round.currentHole)) - round.par[round.currentHole], 1);

  ASSERT_TRUE(incrementGolfCounter(score(), round.currentHole, GolfField::Out100).changed);
  EXPECT_EQ(static_cast<int16_t>(golfHoleScore(round, score(), round.currentHole)) - round.par[round.currentHole], 2);
}

TEST_F(GolfStatsTest, ScoringHoleCellUsesStrokeCountWhenCurrentHoleHasNoPar) {
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 0;
  round.currentHole = 0;
  score().in100[0] = 3;
  score().out100[0] = 2;

  EXPECT_EQ(round.par[round.currentHole], 0);
  EXPECT_EQ(golfHoleScore(round, score(), round.currentHole), 5);
}

TEST_F(GolfStatsTest, ScoringDisplaySeedsUntouchedParHolesAndKeepsCountersAndHoleCellAligned) {
  for (const uint8_t par : {3, 4, 5}) {
    round.par[0] = par;
    const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score(), 0);

    EXPECT_TRUE(display.seeded);
    EXPECT_EQ(display.counters[0], 2);
    EXPECT_EQ(display.counters[1], 2);
    EXPECT_EQ(display.counters[2], par - 2);
    EXPECT_EQ(display.score, par);
    EXPECT_EQ(static_cast<int16_t>(display.score) - par, 0);
  }
}

TEST_F(GolfStatsTest, ScoringDisplayUsesStoredCountersAfterEntry) {
  ASSERT_TRUE(seedGolfHoleAtPar(score(), 0, round.par[0]));
  ASSERT_TRUE(incrementGolfCounter(score(), 0, GolfField::Out100).changed);

  const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score(), 0);
  EXPECT_FALSE(display.seeded);
  EXPECT_EQ(display.counters[0], score().putts[0]);
  EXPECT_EQ(display.counters[1], score().in100[0]);
  EXPECT_EQ(display.counters[2], score().out100[0]);
  EXPECT_EQ(display.score, golfHoleScore(round, score(), 0));
  EXPECT_EQ(static_cast<int16_t>(display.score) - round.par[0], 1);
}

TEST_F(GolfStatsTest, ScoringDisplayLeavesUntouchedParFreeHoleUnseeded) {
  round.par[0] = 0;
  const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score(), 0);

  EXPECT_FALSE(display.seeded);
  EXPECT_EQ(display.counters[0], 0);
  EXPECT_EQ(display.counters[1], 0);
  EXPECT_EQ(display.counters[2], 0);
  EXPECT_EQ(display.score, 0);
  EXPECT_EQ(display.score, golfHoleScore(round, score(), 0));
}

TEST_F(GolfStatsTest, BucketIdentityHoldsPerEnteredHole) {
  fillPartialRound();
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (golfHoleScore(round, score(), hole) == 0) continue;
    const uint16_t shortGame = score().in100[hole] - score().putts[hole];
    EXPECT_EQ(static_cast<uint16_t>(golfLongGame(round, score(), hole)) + shortGame + score().putts[hole],
              golfHoleScore(round, score(), hole));
  }
}

TEST_F(GolfStatsTest, NineHoleRoundIgnoresLaterEntries) {
  fillPartialRound();
  round.holeCount = 9;
  score().in100[9] = 50;
  score().out100[9] = 49;
  EXPECT_EQ(golfScore(round, score()), 12);
  EXPECT_EQ(golfThru(round, score()), 2);
}

TEST_F(GolfStatsTest, WorstHolesAreSortedRelativeToPar) {
  fillPartialRound();
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  const uint8_t count = golfWorstHoles(round, score(), worst, GolfRound::MAX_HOLES);
  ASSERT_EQ(count, 2);
  EXPECT_EQ(worst[0].hole, 2);
  EXPECT_EQ(worst[0].toPar, 2);
  EXPECT_EQ(worst[1].hole, 0);
  EXPECT_EQ(worst[1].toPar, 1);
}

TEST_F(GolfStatsTest, StatsUseOnlyTheExplicitPlayer) {
  score(0).putts[0] = 1;
  score(0).in100[0] = 2;
  score(0).out100[0] = 2;
  round.players[1].tee = TeeSelection::White;
  score(1).putts[0] = 3;
  score(1).in100[0] = 4;
  score(1).out100[0] = 4;

  EXPECT_EQ(golfScore(round, score(0)), 4);
  EXPECT_EQ(golfScore(round, score(1)), 8);
  EXPECT_EQ(golfPuttsTotal(round, score(0)), 1);
  EXPECT_EQ(golfPuttsTotal(round, score(1)), 3);
}

TEST_F(GolfStatsTest, EveryScoreFigureIncludesPenaltyStrokes) {
  score().in100[0] = 2;
  score().out100[0] = 3;
  const uint16_t scoreBefore = golfScore(round, score());
  ASSERT_EQ(golfAppendPenalty(score(), 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);

  EXPECT_EQ(golfHoleScore(round, score(), 0), 8);
  EXPECT_EQ(golfScore(round, score()), scoreBefore + 3);
  EXPECT_EQ(golfToPar(round, score()), 4);
  EXPECT_EQ(golfPenaltyTotal(round, score()), 2);
  EXPECT_EQ(golfParTotal(round, score()), 4);
  EXPECT_EQ(golfThru(round, score()), 1);
  GolfWorstHole worst[GolfRound::MAX_HOLES]{};
  ASSERT_EQ(golfWorstHoles(round, score(), worst, GolfRound::MAX_HOLES), 1);
  EXPECT_EQ(worst[0].toPar, 4);
}

TEST_F(GolfStatsTest, WorkedPenaltyHolesUseDerivedStrokeArithmetic) {
  for (const auto [kind, expected] : {std::pair{GolfPenaltyKind::Hazard, 6}, std::pair{GolfPenaltyKind::Ob, 7}}) {
    GolfPlayerScore& playerScore = kind == GolfPenaltyKind::Hazard ? score(0) : score(1);
    ASSERT_EQ(golfAppendPenalty(playerScore, 0, GolfField::Out100, kind), GolfPenaltyMutationStatus::Changed);
    ASSERT_TRUE(incrementGolfCounter(playerScore, 0, GolfField::Out100).changed);
    ASSERT_TRUE(incrementGolfCounter(playerScore, 0, GolfField::Out100).changed);
    ASSERT_TRUE(incrementGolfCounter(playerScore, 0, GolfField::Putts).changed);
    ASSERT_TRUE(incrementGolfCounter(playerScore, 0, GolfField::Putts).changed);
    EXPECT_EQ(golfHoleScore(round, playerScore, 0), expected);
  }
}

// --- Fairway hit bit accessors (CONTRACTS-V2 §31.5) ---

TEST_F(GolfStatsTest, FairwayAccessorsSetClearAndCountAreIdempotentAndBounded) {
  EXPECT_FALSE(golfFairwayHit(score(), 0));
  golfSetFairwayHit(score(), 0, true);
  golfSetFairwayHit(score(), 0, true);  // idempotent
  EXPECT_TRUE(golfFairwayHit(score(), 0));
  golfSetFairwayHit(score(), 17, true);
  golfSetFairwayHit(score(), 18, true);  // out of range -> no-op
  EXPECT_FALSE(golfFairwayHit(score(), 18));
  EXPECT_EQ(golfFairwayHitsForRound(score(), 18), 2);

  golfSetFairwayHit(score(), 0, false);
  golfSetFairwayHit(score(), 0, false);  // idempotent clear
  EXPECT_FALSE(golfFairwayHit(score(), 0));
  EXPECT_EQ(golfFairwayHitsForRound(score(), 18), 1);
  EXPECT_EQ(golfFairwayHitsForRound(score(), 9), 0);  // hole 17 outside a 9-hole round
}

TEST_F(GolfStatsTest, FairwayBitsAreIndependentAcrossAByteBoundary) {
  golfSetFairwayHit(score(), 7, true);
  golfSetFairwayHit(score(), 8, true);
  EXPECT_TRUE(golfFairwayHit(score(), 7));
  EXPECT_TRUE(golfFairwayHit(score(), 8));
  EXPECT_FALSE(golfFairwayHit(score(), 6));
  EXPECT_FALSE(golfFairwayHit(score(), 9));
  golfSetFairwayHit(score(), 7, false);
  EXPECT_FALSE(golfFairwayHit(score(), 7));
  EXPECT_TRUE(golfFairwayHit(score(), 8));
}

// --- Green in regulation (CONTRACTS-V2 §31.4) ---

TEST_F(GolfStatsTest, GreenInRegulationWorkedParFourCasesFromTheMock) {
  // Every par is 4 (SetUp); GIR threshold = par - 2 = 2.
  // Case 1: putts 2, in100 3, out100 1, no penalty -> holeScore 4, toGreen 2 -> GIR.
  score().putts[0] = 2;
  score().in100[0] = 3;
  score().out100[0] = 1;
  EXPECT_EQ(golfHoleScore(round, score(), 0), 4);
  EXPECT_TRUE(golfGreenInRegulation(round, score(), 0));

  // Case 2: putts 1, same strokes -> toGreen 3 -> not GIR.
  score().putts[1] = 1;
  score().in100[1] = 3;
  score().out100[1] = 1;
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 1));

  // Case 3: putts 2, in100 3, out100 1 + one hazard (out100 -> 2, +1 stroke) ->
  // holeScore 6, toGreen 4 -> not GIR.
  score().putts[2] = 2;
  score().in100[2] = 3;
  score().out100[2] = 1;
  ASSERT_EQ(golfAppendPenalty(score(), 2, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfHoleScore(round, score(), 2), 6);
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 2));

  EXPECT_EQ(golfGreensInRegulation(round, score()), 1);
  EXPECT_EQ(golfGreensEligible(round, score()), 3);
}

TEST_F(GolfStatsTest, GreenInRegulationParThreeParFiveChipInAndIneligiblePars) {
  round.par[0] = 3;  // holeScore 3, putts 2 -> toGreen 1 <= 1 -> GIR
  score().putts[0] = 2;
  score().in100[0] = 2;
  score().out100[0] = 1;
  EXPECT_TRUE(golfGreenInRegulation(round, score(), 0));

  round.par[1] = 3;  // holeScore 3, putts 1 -> toGreen 2 > 1 -> not GIR
  score().putts[1] = 1;
  score().in100[1] = 1;
  score().out100[1] = 2;
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 1));

  round.par[2] = 5;  // holeScore 5, putts 2 -> toGreen 3 <= 3 -> GIR
  score().putts[2] = 2;
  score().in100[2] = 3;
  score().out100[2] = 2;
  EXPECT_TRUE(golfGreenInRegulation(round, score(), 2));

  // Chip-in birdie par 4: holeScore 3, putts 0 -> toGreen 3 > 2 -> not GIR.
  score().putts[3] = 0;
  score().in100[3] = 1;
  score().out100[3] = 2;
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 3));

  // Not entered -> false, and counted in neither total.
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 5));

  // Par 6 and par 0 (par-free) holes: entered but never GIR / GIR-eligible.
  round.par[6] = 6;
  score().putts[6] = 2;
  score().in100[6] = 3;
  score().out100[6] = 1;
  round.par[7] = 0;
  score().putts[7] = 2;
  score().in100[7] = 3;
  score().out100[7] = 1;
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 6));
  EXPECT_FALSE(golfGreenInRegulation(round, score(), 7));

  EXPECT_EQ(golfGreensEligible(round, score()), 4);      // entered holes 0,1,2,3 (par 3,3,5,4)
  EXPECT_EQ(golfGreensInRegulation(round, score()), 2);  // holes 0 and 2
}

// --- Fairways in regulation (CONTRACTS-V2 §31.4 / §31.7) ---

TEST_F(GolfStatsTest, FairwaysHitCountsEnteredSetBitsAndEligibleCountsParFourFive) {
  round.par[0] = 4;
  round.par[1] = 5;
  round.par[2] = 3;
  round.par[3] = 4;
  for (uint8_t hole = 0; hole < 3; ++hole) {
    score().in100[hole] = 2;
    score().out100[hole] = 2;
  }
  golfSetFairwayHit(score(), 0, true);
  golfSetFairwayHit(score(), 1, true);
  golfSetFairwayHit(score(), 2, true);  // par 3 -- helper does not re-filter by par
  golfSetFairwayHit(score(), 3, true);  // hole 3 not entered -> not counted
  EXPECT_EQ(golfFairwaysHit(round, score()), 3);
  EXPECT_EQ(golfFairwaysEligible(round, score()), 2);  // entered par 4/5 = holes 0,1
}

TEST_F(GolfStatsTest, FairwaysEligibleIsZeroOnParFreeRound) {
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 0;
  score().in100[0] = 2;
  score().out100[0] = 2;
  golfSetFairwayHit(score(), 0, true);
  EXPECT_EQ(golfFairwaysEligible(round, score()), 0);
  EXPECT_EQ(golfFairwaysHit(round, score()), 1);  // the bit is still counted
}

}  // namespace
