#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "GolfTrends.h"

namespace {

constexpr char HEADER[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,fairwayHoles,gir,"
    "girHoles,file\r\n";

// A blank cell for an unrecorded FIR/GIR pair; a non-negative value fills it.
std::string cell(const int value) { return value < 0 ? std::string() : std::to_string(value); }

std::string row(const uint8_t holes, const uint16_t strokes, const uint16_t par, const uint16_t putts,
                const uint16_t in100, const uint16_t out100, const uint8_t slot = 0, const char* name = "Noah",
                const int fairways = -1, const int fairwayHoles = -1, const int gir = -1, const int girHoles = -1) {
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  snprintf(output, sizeof(output), ",Course,%u,%u,%s,%u,%u,%u,%u,%u,0,0,%s,%s,%s,%s,round.json\r\n", holes, slot, name,
           strokes, par, putts, in100, out100, cell(fairways).c_str(), cell(fairwayHoles).c_str(), cell(gir).c_str(),
           cell(girHoles).c_str());
  return output;
}

// A round that recorded penalty data (real hazards/obs counts).
std::string penaltyRow(const uint16_t strokes, const uint16_t par, const uint16_t putts, const uint16_t in100,
                       const uint16_t out100, const uint16_t hazards, const uint16_t obs, const int fairways = -1,
                       const int fairwayHoles = -1, const int gir = -1, const int girHoles = -1) {
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  snprintf(output, sizeof(output), ",Course,18,0,Noah,%u,%u,%u,%u,%u,%u,%u,%s,%s,%s,%s,round.json\r\n", strokes, par,
           putts, in100, out100, hazards, obs, cell(fairways).c_str(), cell(fairwayHoles).c_str(), cell(gir).c_str(),
           cell(girHoles).c_str());
  return output;
}

// A migrated pre-penalty round uses the strict v5 shape with the penalty and
// FIR/GIR cells empty; normal readers never accept the original 9-column row.
std::string prePenaltyRow(const uint16_t strokes, const uint16_t par, const uint16_t putts, const uint16_t in100,
                          const uint16_t out100) {
  char output[160];
  snprintf(output, sizeof(output), ",Course,18,0,Noah,%u,%u,%u,%u,%u,,,,,,,round.json\r\n", strokes, par, putts, in100,
           out100);
  return output;
}

GolfTrendStats calculate(const std::string& rows, const uint8_t playerSlot = 0) {
  GolfHistoryReader history;
  history.reset(playerSlot);
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

TEST(GolfTrends, MixPercentagesUseSharedPopulationAndSumToOneHundredPercent) {
  const GolfTrendStats stats = calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + penaltyRow(90, 72, 34, 54, 30, 4, 1) +
                                         prePenaltyRow(200, 72, 100, 150, 50));
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_EQ(
      stats.longPercentTenths + stats.shortPercentTenths + stats.puttingPercentTenths + stats.penaltyPercentTenths,
      1000u);
  EXPECT_EQ(stats.longAverageTenths, 300u);
  EXPECT_EQ(stats.shortAverageTenths, 195u);
  EXPECT_EQ(stats.puttingAverageTenths, 335u);
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
  EXPECT_EQ(one.longPercentTenths + one.shortPercentTenths + one.puttingPercentTenths + one.penaltyPercentTenths, 0u);
}

TEST(GolfTrends, HeadlineAveragesStillIncludePrePenaltyRounds) {
  const GolfTrendStats stats = calculate(penaltyRow(80, 72, 30, 50, 28, 1, 0) + penaltyRow(82, 72, 31, 51, 30, 1, 0) +
                                         prePenaltyRow(98, 72, 40, 68, 30));
  EXPECT_EQ(stats.rounds, 3);
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_EQ(stats.scoringAverageTenths, 867u);
  EXPECT_EQ(stats.puttsAverageTenths, 337u);
  EXPECT_EQ(stats.best, 80);
  EXPECT_EQ(stats.worst, 98);
}

TEST(GolfTrends, NineHolePenaltyRoundsDoNotCountTowardPenaltyFigures) {
  const std::string nineWithPenalty = ",Course,9,0,Noah,44,36,18,28,16,3,1,,,,,round.json\r\n";
  const GolfTrendStats stats =
      calculate(penaltyRow(86, 72, 33, 52, 30, 2, 1) + penaltyRow(90, 72, 34, 54, 30, 4, 1) + nineWithPenalty);
  EXPECT_EQ(stats.penaltyRounds, 2);
  EXPECT_EQ(stats.penaltyStrokesAverageTenths, 50u);
}

TEST(GolfTrends, SelectedSlotNeverConsumesInterleavedPlayers) {
  const std::string rows = row(18, 80, 72, 30, 50, 30, 0, "Noah") + row(18, 120, 72, 45, 80, 40, 2, "Guest") +
                           row(18, 82, 72, 31, 51, 31, 0, "Noah") + row(18, 122, 72, 46, 81, 41, 2, "Guest");
  const GolfTrendStats noah = calculate(rows, 0);
  const GolfTrendStats guest = calculate(rows, 2);
  EXPECT_EQ(noah.rounds, 2);
  EXPECT_EQ(noah.scoringAverageTenths, 810u);
  EXPECT_EQ(guest.rounds, 2);
  EXPECT_EQ(guest.scoringAverageTenths, 1210u);
  EXPECT_EQ(noah.best, 80);
  EXPECT_EQ(guest.best, 120);
}

TEST(GolfTrends, FairwayAndGreenPercentagesFoldOverRecordedRounds) {
  const GolfTrendStats stats = calculate(row(18, 80, 72, 30, 50, 30, 0, "Noah", 7, 14, 10, 18) +
                                         row(18, 82, 72, 32, 52, 30, 0, "Noah", 9, 14, 8, 18));
  EXPECT_TRUE(stats.showsFir);
  EXPECT_TRUE(stats.showsGir);
  EXPECT_EQ(stats.firRounds, 2);
  EXPECT_EQ(stats.girRounds, 2);
  EXPECT_EQ(stats.firPercentTenths, 571u);  // 16 hit / 28 eligible
  EXPECT_EQ(stats.girPercentTenths, 500u);  // 18 / 36
}

TEST(GolfTrends, GreenInRegFoldsDerivedRoundsWhileFairwayNeedsTheRecord) {
  const GolfTrendStats stats = calculate(
      row(18, 88, 72, 34, 56, 32, 0, "Noah", -1, -1, 12, 18) + row(18, 90, 72, 35, 58, 32, 0, "Noah", -1, -1, 9, 18) +
      row(18, 80, 72, 30, 50, 30, 0, "Noah", 8, 14, 10, 18) + row(18, 82, 72, 31, 52, 30, 0, "Noah", 6, 14, 6, 18));
  EXPECT_EQ(stats.girRounds, 4);
  EXPECT_EQ(stats.firRounds, 2);
  EXPECT_TRUE(stats.showsGir);
  EXPECT_TRUE(stats.showsFir);
  EXPECT_EQ(stats.girPercentTenths, 513u);  // 37 / 72
  EXPECT_EQ(stats.firPercentTenths, 500u);  // 14 / 28
}

TEST(GolfTrends, SingleFairwayRoundDoesNotShowFairwayPercentage) {
  const GolfTrendStats stats = calculate(row(18, 80, 72, 30, 50, 30, 0, "Noah", 7, 14, 10, 18) +
                                         row(18, 82, 72, 32, 52, 30, 0, "Noah", -1, -1, 8, 18));
  EXPECT_EQ(stats.firRounds, 1);
  EXPECT_FALSE(stats.showsFir);
  EXPECT_EQ(stats.firPercentTenths, 0u);
  EXPECT_EQ(stats.girRounds, 2);
  EXPECT_TRUE(stats.showsGir);
}

TEST(GolfTrends, ParFreeRoundIsExcludedFromFairwayAndGreenPercentages) {
  const std::string good =
      row(18, 80, 72, 30, 50, 30, 0, "Noah", 7, 14, 9, 18) + row(18, 82, 72, 32, 52, 30, 0, "Noah", 9, 14, 9, 18);
  const GolfTrendStats baseline = calculate(good);
  const GolfTrendStats withParFree = calculate(good + row(18, 95, 0, 40, 60, 35, 0, "Noah", 0, 0, 0, 0));
  EXPECT_EQ(withParFree.firRounds, baseline.firRounds);
  EXPECT_EQ(withParFree.girRounds, baseline.girRounds);
  EXPECT_EQ(withParFree.firPercentTenths, baseline.firPercentTenths);
  EXPECT_EQ(withParFree.girPercentTenths, baseline.girPercentTenths);
  EXPECT_EQ(withParFree.firRounds, 2);
}
