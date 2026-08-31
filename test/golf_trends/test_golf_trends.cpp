#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "GolfTrends.h"

namespace {

constexpr char HEADER[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file\r\n";

std::string row(const uint8_t holes, const uint16_t strokes, const uint16_t par, const uint16_t putts,
                const uint16_t in100, const uint16_t out100) {
  char output[160];
  snprintf(output, sizeof(output), ",Course,%u,%u,%u,%u,%u,%u,0,0,round.json\r\n", holes, strokes, par, putts, in100,
           out100);
  return output;
}

// A round that recorded penalty data (real hazards/obs counts).
std::string penaltyRow(const uint16_t strokes, const uint16_t par, const uint16_t putts, const uint16_t in100,
                       const uint16_t out100, const uint16_t hazards, const uint16_t obs) {
  char output[160];
  snprintf(output, sizeof(output), ",Course,18,%u,%u,%u,%u,%u,%u,%u,round.json\r\n", strokes, par, putts, in100, out100,
           hazards, obs);
  return output;
}

// A round played before penalty tracking: v2 row shape, no penalty cells.
std::string prePenaltyRow(const uint16_t strokes, const uint16_t par, const uint16_t putts, const uint16_t in100,
                          const uint16_t out100) {
  char output[160];
  snprintf(output, sizeof(output), ",Course,18,%u,%u,%u,%u,%u,round.json\r\n", strokes, par, putts, in100, out100);
  return output;
}

GolfTrendStats calculate(const std::string& rows) {
  GolfHistoryReader history;
  history.reset();
  const std::string input = std::string(HEADER) + rows;
  history.feed(input.data(), input.size());
  history.finish();
  return golfCalculateTrends(history);
}

}  // namespace

TEST(GolfTrends, AveragesSeveralRoundsWithOneDecimalRounding) {
  const GolfTrendStats stats =
      calculate(row(18, 80, 72, 31, 53, 27) + row(18, 81, 72, 32, 54, 27) + row(18, 80, 72, 32, 53, 27));

  EXPECT_TRUE(stats.enoughRounds());
  EXPECT_EQ(stats.rounds, 3);
  EXPECT_EQ(stats.scoringAverageTenths, 803u);
  EXPECT_EQ(stats.toParAverageTenths, 83);
  EXPECT_EQ(stats.puttsAverageTenths, 317u);
  EXPECT_EQ(stats.longAverageTenths, 270u);
  EXPECT_EQ(stats.shortAverageTenths, 217u);
  EXPECT_EQ(stats.puttingAverageTenths, 317u);
}

TEST(GolfTrends, ExcludesNineHoleRoundsFromEveryFigure) {
  const GolfTrendStats baseline = calculate(row(18, 80, 72, 30, 50, 30) + row(18, 82, 72, 32, 52, 30));
  const GolfTrendStats mixed =
      calculate(row(9, 38, 36, 12, 20, 18) + row(18, 80, 72, 30, 50, 30) + row(18, 82, 72, 32, 52, 30));

  EXPECT_EQ(mixed.rounds, baseline.rounds);
  EXPECT_EQ(mixed.scoringAverageTenths, baseline.scoringAverageTenths);
  EXPECT_EQ(mixed.toParAverageTenths, baseline.toParAverageTenths);
  EXPECT_EQ(mixed.best, baseline.best);
  EXPECT_EQ(mixed.worst, baseline.worst);
  EXPECT_EQ(mixed.puttsAverageTenths, baseline.puttsAverageTenths);
  EXPECT_EQ(mixed.longAverageTenths, baseline.longAverageTenths);
  EXPECT_EQ(mixed.shortAverageTenths, baseline.shortAverageTenths);
  EXPECT_EQ(mixed.puttingAverageTenths, baseline.puttingAverageTenths);
  EXPECT_EQ(mixed.longPercentTenths, baseline.longPercentTenths);
  EXPECT_EQ(mixed.shortPercentTenths, baseline.shortPercentTenths);
  EXPECT_EQ(mixed.puttingPercentTenths, baseline.puttingPercentTenths);
}

TEST(GolfTrends, ZeroAndOneRoundAreNotEnough) {
  EXPECT_FALSE(calculate("").enoughRounds());
  const GolfTrendStats one = calculate(row(18, 80, 72, 30, 50, 30));
  EXPECT_EQ(one.rounds, 1);
  EXPECT_FALSE(one.enoughRounds());
}

TEST(GolfTrends, ParFreeRoundSuppressesOnlyToPar) {
  const GolfTrendStats stats = calculate(row(18, 80, 0, 30, 50, 30) + row(18, 82, 72, 32, 52, 30));

  EXPECT_FALSE(stats.showsToPar);
  EXPECT_EQ(stats.scoringAverageTenths, 810u);
  EXPECT_EQ(stats.best, 80);
  EXPECT_EQ(stats.worst, 82);
  EXPECT_EQ(stats.puttsAverageTenths, 310u);
}

TEST(GolfTrends, BucketPercentagesSumSensiblyAndZeroStrokesDoesNotDivide) {
  const GolfTrendStats stats = calculate(row(18, 80, 72, 30, 50, 30) + row(18, 80, 72, 30, 50, 30));
  EXPECT_EQ(stats.longPercentTenths + stats.shortPercentTenths + stats.puttingPercentTenths, 1000u);

  const GolfTrendStats zero = calculate(row(18, 0, 0, 0, 0, 0) + row(18, 0, 0, 0, 0, 0));
  EXPECT_EQ(zero.longPercentTenths, 0u);
  EXPECT_EQ(zero.shortPercentTenths, 0u);
  EXPECT_EQ(zero.puttingPercentTenths, 0u);
}

TEST(GolfTrends, BestAndWorstHandleSingleRoundAndTies) {
  const GolfTrendStats one = calculate(row(18, 77, 72, 30, 47, 30));
  EXPECT_EQ(one.best, 77);
  EXPECT_EQ(one.worst, 77);

  const GolfTrendStats ties =
      calculate(row(18, 80, 72, 30, 50, 30) + row(18, 74, 72, 28, 44, 30) + row(18, 91, 72, 35, 61, 30) +
                row(18, 74, 72, 29, 44, 30) + row(18, 91, 72, 36, 61, 30));
  EXPECT_EQ(ties.best, 74);
  EXPECT_EQ(ties.worst, 91);
}

TEST(GolfTrends, NegativeToParUsesSymmetricRounding) {
  const GolfTrendStats stats = calculate(row(18, 69, 72, 28, 43, 26) + row(18, 70, 72, 29, 44, 26));
  EXPECT_EQ(stats.toParAverageTenths, -25);
}

TEST(GolfTrends, PenaltyAveragesFoldOverRoundsThatRecordedPenaltyData) {
  // hazards 2 + 4 -> mean 3.0; obs 1 + 1 -> mean 1.0; strokes cost 4 + 6 -> mean 5.0.
  const GolfTrendStats stats = calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + penaltyRow(90, 72, 34, 54, 30, 4, 1));
  EXPECT_TRUE(stats.showsPenalties);
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_EQ(stats.hazardsAverageTenths, 30u);
  EXPECT_EQ(stats.obsAverageTenths, 10u);
  EXPECT_EQ(stats.penaltyStrokesAverageTenths, 50u);
}

TEST(GolfTrends, ExcludesPrePenaltyRoundsRatherThanCountingThemAsZero) {
  // Two rounds with data (strokes cost 4 and 6 -> mean 5.0) plus two pre-penalty
  // rounds. Counting the old rounds as zero would drag the mean to 2.5.
  const GolfTrendStats stats = calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + penaltyRow(90, 72, 34, 54, 30, 4, 1) +
                                         prePenaltyRow(85, 72, 35, 55, 30) + prePenaltyRow(83, 72, 33, 53, 30));
  EXPECT_EQ(stats.rounds, 4);
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_TRUE(stats.showsPenalties);
  EXPECT_EQ(stats.penaltyStrokesAverageTenths, 50u);
}

TEST(GolfTrends, SuppressesPenaltyFiguresBelowTwoRecordedRounds) {
  const GolfTrendStats none = calculate(prePenaltyRow(85, 72, 35, 55, 30) + prePenaltyRow(83, 72, 33, 53, 30));
  EXPECT_FALSE(none.showsPenalties);
  EXPECT_EQ(none.penaltyRounds, 0);
  EXPECT_EQ(none.penaltyStrokesAverageTenths, 0u);

  const GolfTrendStats one = calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + prePenaltyRow(83, 72, 33, 53, 30));
  EXPECT_FALSE(one.showsPenalties);
  EXPECT_EQ(one.penaltyRounds, 1);
  EXPECT_EQ(one.penaltyStrokesAverageTenths, 0u);
}

TEST(GolfTrends, NineHolePenaltyRoundsDoNotCountTowardPenaltyFigures) {
  const std::string nineWithPenalty = ",Course,9,44,36,18,28,16,3,1,round.json\r\n";
  const GolfTrendStats stats =
      calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + penaltyRow(90, 72, 34, 54, 30, 4, 1) + nineWithPenalty);
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_EQ(stats.penaltyStrokesAverageTenths, 50u);
}
