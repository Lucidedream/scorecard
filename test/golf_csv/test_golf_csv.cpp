#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "GolfCsv.h"
#include "GolfPenalty.h"

namespace {

GolfIndexRow makeRow(const char* course, const char* playerName = "Noah") {
  GolfIndexRow row{};
  strcpy(row.date, "2026-08-29");
  strncpy(row.course, course, sizeof(row.course) - 1);
  strncpy(row.playerName, playerName, sizeof(row.playerName) - 1);
  row.holes = 18;
  row.playerSlot = 0;
  row.strokes = 86;
  row.par = 72;
  row.putts = 33;
  row.in100 = 54;
  row.out100 = 30;
  row.hazards = 2;
  row.obs = 1;
  row.penaltiesRecorded = true;
  row.fairways = 7;
  row.fairwayHoles = 14;
  row.fairwaysRecorded = true;
  row.gir = 5;
  row.girHoles = 16;
  row.girRecorded = true;
  strcpy(row.file, "round-0001-course.json");
  return row;
}

void expectRoundTrip(const char* course, const char* playerName) {
  const GolfIndexRow source = makeRow(course, playerName);
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, output, sizeof(output)));
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_STREQ(parsed.date, source.date);
  EXPECT_STREQ(parsed.course, source.course);
  EXPECT_EQ(parsed.holes, source.holes);
  EXPECT_EQ(parsed.playerSlot, source.playerSlot);
  EXPECT_STREQ(parsed.playerName, source.playerName);
  EXPECT_EQ(parsed.strokes, source.strokes);
  EXPECT_EQ(parsed.par, source.par);
  EXPECT_EQ(parsed.putts, source.putts);
  EXPECT_EQ(parsed.in100, source.in100);
  EXPECT_EQ(parsed.out100, source.out100);
  EXPECT_EQ(parsed.hazards, source.hazards);
  EXPECT_EQ(parsed.obs, source.obs);
  EXPECT_EQ(parsed.penaltiesRecorded, source.penaltiesRecorded);
  EXPECT_EQ(parsed.fairways, source.fairways);
  EXPECT_EQ(parsed.fairwayHoles, source.fairwayHoles);
  EXPECT_EQ(parsed.fairwaysRecorded, source.fairwaysRecorded);
  EXPECT_EQ(parsed.gir, source.gir);
  EXPECT_EQ(parsed.girHoles, source.girHoles);
  EXPECT_EQ(parsed.girRecorded, source.girRecorded);
  EXPECT_STREQ(parsed.file, source.file);
}

struct GroupSink {
  std::string output;
  uint8_t calls = 0;
  uint8_t failAt = UINT8_MAX;
};

bool collectGroupRow(const char* data, const size_t size, void* user) {
  auto* sink = static_cast<GroupSink*>(user);
  if (sink->calls++ == sink->failAt) return false;
  sink->output.append(data, size);
  return true;
}

class GolfCsvRoundTest : public ::testing::Test {
 protected:
  GolfRound round{};

  void SetUp() override {
    initializeGolfPlayerDefaults(round);
    strcpy(round.courseName, "Course");
    round.holeCount = 18;
    golfSetTee(round.players[0], "Blue");
    golfSetTee(round.players[2], "White");
    for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  }
};

// A v5 index row with every optional cell populated (17 fields).
constexpr char V5_ROW_FULL[] = ",Course,18,0,Noah,86,72,33,54,30,2,1,7,14,5,16,round.json\r\n";

}  // namespace

TEST(GolfCsvV5, RoundTripsAllFourRegulationFields) {
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(V5_ROW_FULL, parsed));
  EXPECT_EQ(parsed.hazards, 2);
  EXPECT_EQ(parsed.obs, 1);
  EXPECT_TRUE(parsed.penaltiesRecorded);
  EXPECT_EQ(parsed.fairways, 7);
  EXPECT_EQ(parsed.fairwayHoles, 14);
  EXPECT_TRUE(parsed.fairwaysRecorded);
  EXPECT_EQ(parsed.gir, 5);
  EXPECT_EQ(parsed.girHoles, 16);
  EXPECT_TRUE(parsed.girRecorded);

  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(parsed, output, sizeof(output)));
  EXPECT_STREQ(output, V5_ROW_FULL);
}

TEST(GolfCsvV5, GirPresentWhileFairwaysBlankIsARebuiltV4Round) {
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,,,,,5,16,round.json\r\n", parsed));
  EXPECT_FALSE(parsed.penaltiesRecorded);
  EXPECT_FALSE(parsed.fairwaysRecorded);
  EXPECT_EQ(parsed.fairways, 0);
  EXPECT_EQ(parsed.fairwayHoles, 0);
  EXPECT_TRUE(parsed.girRecorded);
  EXPECT_EQ(parsed.gir, 5);
  EXPECT_EQ(parsed.girHoles, 16);

  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(parsed, output, sizeof(output)));
  EXPECT_STREQ(output, ",Course,18,0,Noah,86,72,33,54,30,,,,,5,16,round.json\r\n");
}

TEST(GolfCsvV5, RejectsHalfFilledFairwayOrGirPair) {
  GolfIndexRow row{};
  // fairways present, fairwayHoles blank
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,,,7,,5,16,round.json\r\n", row));
  // fairwayHoles present, fairways blank
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,,,,14,5,16,round.json\r\n", row));
  // gir present, girHoles blank
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,,,7,14,5,,round.json\r\n", row));
  // girHoles present, gir blank
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,,,7,14,,16,round.json\r\n", row));
}

TEST(GolfCsvV5, RejectsFieldCountMismatch) {
  GolfIndexRow row{};
  // 13 fields (a bare v4 row) is no longer accepted by the normal reader.
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,2,1,round.json\r\n", row));
  // 16 fields (one short).
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,2,1,7,14,5,round.json\r\n", row));
  // 18 fields (one long).
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,86,72,33,54,30,2,1,7,14,5,16,0,round.json\r\n", row));
}

TEST(GolfCsv, RoundTripsPlainFields) { expectRoundTrip("Pebble Beach", "Noah"); }
TEST(GolfCsv, QuotesAndRoundTripsCourseAndPlayerName) { expectRoundTrip("Pebble, \"Beach\"", "A, \"B\""); }

TEST(GolfCsv, FixedBufferFitsWorstCaseQuotedNames) {
  GolfIndexRow row{};
  memset(row.course, '"', sizeof(row.course) - 1);
  memset(row.playerName, '"', sizeof(row.playerName) - 1);
  memset(row.file, 'f', sizeof(row.file) - 1);
  strcpy(row.date, "2127-12-31");
  row.holes = UINT8_MAX;
  row.playerSlot = 3;
  row.strokes = UINT16_MAX;
  row.par = UINT16_MAX;
  row.putts = UINT16_MAX;
  row.in100 = UINT16_MAX;
  row.out100 = UINT16_MAX;
  row.hazards = UINT16_MAX;
  row.obs = UINT16_MAX;
  row.penaltiesRecorded = true;
  row.fairways = UINT16_MAX;
  row.fairwayHoles = UINT16_MAX;
  row.fairwaysRecorded = true;
  row.gir = UINT16_MAX;
  row.girHoles = UINT16_MAX;
  row.girRecorded = true;
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(row, output, sizeof(output)));
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_STREQ(parsed.course, row.course);
  EXPECT_STREQ(parsed.playerName, row.playerName);
  EXPECT_EQ(parsed.fairways, UINT16_MAX);
  EXPECT_EQ(parsed.fairwayHoles, UINT16_MAX);
  EXPECT_EQ(parsed.gir, UINT16_MAX);
  EXPECT_EQ(parsed.girHoles, UINT16_MAX);
}

TEST(GolfCsv, RoundTripsUnknownDateAsEmptyCell) {
  GolfIndexRow source = makeRow("Practice 9");
  source.date[0] = '\0';
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, output, sizeof(output)));
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_STREQ(parsed.date, "");
}

TEST(GolfCsv, LegacyRowsRequireAnExplicitVersionedMigrationParse) {
  constexpr char V2_ROW[] = ",Course,18,80,72,30,50,30,round.json\r\n";
  constexpr char V3_ROW[] = ",Course,18,80,72,30,50,30,2,1,round.json\r\n";
  GolfIndexRow v2{};
  EXPECT_FALSE(golfParseIndexRow(V2_ROW, v2));
  ASSERT_TRUE(golfParseIndexRow(V2_ROW, GolfIndexVersion::V2, v2));
  EXPECT_FALSE(golfParseIndexRow(V2_ROW, GolfIndexVersion::V3, v2));
  EXPECT_EQ(v2.playerSlot, 0);
  EXPECT_STREQ(v2.playerName, "Noah");
  EXPECT_FALSE(v2.penaltiesRecorded);

  GolfIndexRow v3{};
  EXPECT_FALSE(golfParseIndexRow(V3_ROW, v3));
  ASSERT_TRUE(golfParseIndexRow(V3_ROW, GolfIndexVersion::V3, v3));
  EXPECT_FALSE(golfParseIndexRow(V3_ROW, GolfIndexVersion::V2, v3));
  EXPECT_EQ(v3.playerSlot, 0);
  EXPECT_STREQ(v3.playerName, "Noah");
  EXPECT_TRUE(v3.penaltiesRecorded);
}

TEST(GolfCsv, ParsesMigratedRowsWithEmptyRegulationCellsAsNotRecorded) {
  GolfIndexRow row{};
  ASSERT_TRUE(golfParseIndexRow(",Course,18,0,Noah,80,72,30,50,30,,,,,,,round.json\r\n", row));
  EXPECT_FALSE(row.penaltiesRecorded);
  EXPECT_FALSE(row.fairwaysRecorded);
  EXPECT_FALSE(row.girRecorded);
  EXPECT_STREQ(row.file, "round.json");
}

TEST(GolfCsv, RejectsInvalidSlotMalformedUtf8AndUnterminatedQuote) {
  GolfIndexRow row{};
  EXPECT_FALSE(golfParseIndexRow(",Course,18,4,Noah,80,72,30,50,30,0,0,0,0,0,0,round.json\r\n", row));
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,\xc3,80,72,30,50,30,0,0,0,0,0,0,round.json\r\n", row));
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,\"Noah,80,72,30,50,30,0,0,0,0,0,0,round.json\r\n", row));
}

TEST(GolfCsv, RejectsCrLfInSingleLineCourseAndPlayerFields) {
  GolfIndexRow row = makeRow("Course");
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  strcpy(row.course, "Course\nTwo");
  EXPECT_FALSE(golfFormatIndexRow(row, output, sizeof(output)));
  row = makeRow("Course", "Player\rTwo");
  EXPECT_FALSE(golfFormatIndexRow(row, output, sizeof(output)));
}

TEST(GolfCsv, RejectsOnlyOnePopulatedPenaltyCell) {
  GolfIndexRow row{};
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,80,72,30,50,30,2,,,,,,round.json\r\n", row));
  EXPECT_FALSE(golfParseIndexRow(",Course,18,0,Noah,80,72,30,50,30,,1,,,,,round.json\r\n", row));
}

TEST(GolfCsv, FormatsNotRecordedRowWithEmptyRegulationCells) {
  GolfIndexRow source = makeRow("Pebble Beach");
  source.penaltiesRecorded = false;
  source.fairwaysRecorded = false;
  source.girRecorded = false;
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, output, sizeof(output)));
  EXPECT_NE(strstr(output, ",30,,,,,,,round-0001-course.json\r\n"), nullptr);
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_FALSE(parsed.penaltiesRecorded);
  EXPECT_FALSE(parsed.fairwaysRecorded);
  EXPECT_FALSE(parsed.girRecorded);
}

TEST_F(GolfCsvRoundTest, BuildsOneNormalizedRowPerEnabledStableSlot) {
  strcpy(round.players[2].name, "Guest, Two");
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;
  round.players[2].score.in100[0] = 3;
  round.players[2].score.out100[0] = 4;

  EXPECT_EQ(golfEnabledPlayerMask(round), 0x05);
  GolfIndexRow first{};
  GolfIndexRow third{};
  ASSERT_TRUE(golfMakeIndexRow(round, 0, "round-0001.json", true, first));
  ASSERT_TRUE(golfMakeIndexRow(round, 2, "round-0001.json", true, third));
  EXPECT_EQ(first.playerSlot, 0);
  EXPECT_EQ(first.strokes, 4);
  EXPECT_EQ(third.playerSlot, 2);
  EXPECT_STREQ(third.playerName, "Guest, Two");
  EXPECT_EQ(third.strokes, 7);
  EXPECT_STREQ(first.file, third.file);
  // Par is set for every hole in SetUp, so GIR is recorded; only hole 0 is
  // entered, so the denominator is 1.
  EXPECT_TRUE(first.girRecorded);
  EXPECT_EQ(first.girHoles, 1);
  EXPECT_TRUE(first.fairwaysRecorded);
  EXPECT_FALSE(golfMakeIndexRow(round, 1, "round-0001.json", true, first));
}

TEST_F(GolfCsvRoundTest, MakeIndexRowLeavesFairwaysBlankWhenNotRecorded) {
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;
  golfSetFairwayHit(round.players[0].score, 0, true);

  GolfIndexRow recorded{};
  ASSERT_TRUE(golfMakeIndexRow(round, 0, "round-0001.json", true, recorded));
  EXPECT_TRUE(recorded.fairwaysRecorded);
  EXPECT_EQ(recorded.fairways, 1);
  EXPECT_EQ(recorded.fairwayHoles, 1);
  EXPECT_TRUE(recorded.girRecorded);

  GolfIndexRow legacy{};
  ASSERT_TRUE(golfMakeIndexRow(round, 0, "round-0001.json", false, legacy));
  EXPECT_FALSE(legacy.fairwaysRecorded);
  EXPECT_EQ(legacy.fairways, 0);
  EXPECT_EQ(legacy.fairwayHoles, 0);
  // GIR is still filled: it needs no stored input.
  EXPECT_TRUE(legacy.girRecorded);
  EXPECT_EQ(legacy.girHoles, 1);
}

TEST_F(GolfCsvRoundTest, MakeIndexRowLeavesBothPairsBlankOnAParFreeRound) {
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 0;
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;
  golfSetFairwayHit(round.players[0].score, 0, true);

  GolfIndexRow row{};
  ASSERT_TRUE(golfMakeIndexRow(round, 0, "round-0001.json", true, row));
  EXPECT_FALSE(row.girRecorded);
  EXPECT_EQ(row.gir, 0);
  EXPECT_EQ(row.girHoles, 0);
  // FIR is still "recorded" for a v5 round, but the helper reports a 0
  // denominator on a par-free course.
  EXPECT_TRUE(row.fairwaysRecorded);
  EXPECT_EQ(row.fairwayHoles, 0);
}

TEST_F(GolfCsvRoundTest, GroupWriterRejectsMultilinePersistedNames) {
  char rowBuffer[GOLF_CSV_ROW_BUFFER_SIZE];
  GolfIndexRow rowScratch{};
  GroupSink sink;
  strcpy(round.players[0].name, "Noah\nGuest");
  const GolfIndexGroupWriteResult written = golfWriteIndexGroupRows(
      round, "round-0001.json", true, rowScratch, rowBuffer, sizeof(rowBuffer), &collectGroupRow, &sink);
  EXPECT_FALSE(written.complete);
  EXPECT_EQ(written.rowCount, 0);
  EXPECT_TRUE(sink.output.empty());
}

TEST_F(GolfCsvRoundTest, GroupWriterReportsCompleteOnlyAfterEveryEnabledRow) {
  char rowBuffer[GOLF_CSV_ROW_BUFFER_SIZE];
  GolfIndexRow rowScratch{};
  GroupSink complete;
  const GolfIndexGroupWriteResult written = golfWriteIndexGroupRows(
      round, "round-0001.json", true, rowScratch, rowBuffer, sizeof(rowBuffer), &collectGroupRow, &complete);
  EXPECT_TRUE(written.complete);
  EXPECT_EQ(written.rowCount, 2);
  EXPECT_EQ(written.slotMask, 0x05);

  const std::string live = "unchanged live index";
  GroupSink failed;
  failed.failAt = 1;
  const GolfIndexGroupWriteResult partial = golfWriteIndexGroupRows(
      round, "round-0001.json", true, rowScratch, rowBuffer, sizeof(rowBuffer), &collectGroupRow, &failed);
  EXPECT_FALSE(partial.complete);
  EXPECT_EQ(partial.rowCount, 1);
  EXPECT_EQ(partial.slotMask, 0x01);
  EXPECT_EQ(live, "unchanged live index");
}
