#include <gtest/gtest.h>

#include <cstring>

#include "GolfCsv.h"

namespace {

GolfIndexRow makeRow(const char* course) {
  GolfIndexRow row{};
  strcpy(row.date, "2026-08-29");
  strncpy(row.course, course, sizeof(row.course) - 1);
  row.holes = 18;
  row.strokes = 86;
  row.par = 72;
  row.putts = 33;
  row.in100 = 21;
  row.out100 = 32;
  strcpy(row.file, "round-0001-course.json");
  return row;
}

void expectRoundTrip(const char* course, const char* encodedCourse) {
  const GolfIndexRow source = makeRow(course);
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, output, sizeof(output)));
  EXPECT_NE(strstr(output, encodedCourse), nullptr);
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_STREQ(parsed.date, source.date);
  EXPECT_STREQ(parsed.course, source.course);
  EXPECT_EQ(parsed.holes, source.holes);
  EXPECT_EQ(parsed.strokes, source.strokes);
  EXPECT_EQ(parsed.par, source.par);
  EXPECT_EQ(parsed.putts, source.putts);
  EXPECT_EQ(parsed.in100, source.in100);
  EXPECT_EQ(parsed.out100, source.out100);
  EXPECT_STREQ(parsed.file, source.file);
}

}  // namespace

TEST(GolfCsv, RoundTripsPlainCourseName) { expectRoundTrip("Pebble Beach", "Pebble Beach"); }
TEST(GolfCsv, QuotesAndRoundTripsComma) { expectRoundTrip("Pebble, Beach", "\"Pebble, Beach\""); }
TEST(GolfCsv, EscapesAndRoundTripsQuote) { expectRoundTrip("Pebble \"Beach\"", "\"Pebble \"\"Beach\"\"\""); }
TEST(GolfCsv, EscapesAndRoundTripsCommaAndQuote) { expectRoundTrip("Pebble, \"Beach\"", "\"Pebble, \"\"Beach\"\"\""); }

TEST(GolfCsv, RoundTripsUnknownDateAsEmptyCell) {
  GolfIndexRow source = makeRow("Practice 9");
  source.date[0] = '\0';
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, output, sizeof(output)));
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output, parsed));
  EXPECT_STREQ(parsed.date, "");
}

TEST(GolfCsv, RejectsUnterminatedQuotedCourse) {
  GolfIndexRow row{};
  EXPECT_FALSE(golfParseIndexRow("2026-08-29,\"Pebble,18,86,72,33,21,32,round.json", row));
}
