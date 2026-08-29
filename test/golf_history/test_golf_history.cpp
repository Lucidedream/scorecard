#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "GolfHistory.h"

namespace {

constexpr char HEADER[] = "date,course,holes,strokes,par,putts,in100,out100,file\r\n";
static_assert(sizeof(GolfHistoryEntry) <= 56);

std::string row(const int number, const uint16_t par = 72) {
  char output[160];
  snprintf(output, sizeof(output), ",Course %d,18,%d,%u,32,54,32,round-%04d.json\r\n", number, 80 + number, par,
           number);
  return output;
}

GolfHistoryReader read(const std::string& input) {
  GolfHistoryReader reader;
  reader.reset();
  for (size_t offset = 0; offset < input.size(); offset += 17) {
    const size_t remaining = input.size() - offset;
    reader.feed(input.data() + offset, remaining < 17 ? remaining : 17);
  }
  reader.finish();
  return reader;
}

}  // namespace

TEST(GolfHistory, KeepsLastFiftyNewestFirstAndReportsOverflow) {
  std::string input = HEADER;
  for (int number = 1; number <= 60; ++number) input += row(number);
  const GolfHistoryReader reader = read(input);
  ASSERT_EQ(reader.count(), 50);
  EXPECT_TRUE(reader.overflowed());
  EXPECT_EQ(reader.totalValidRows(), 60u);
  EXPECT_STREQ(reader.newest(0).course, "Course 60");
  EXPECT_STREQ(reader.newest(49).course, "Course 11");
}

TEST(GolfHistory, KeepsAllRowsNewestFirstWhenUnderCapacity) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1) + row(2) + row(3));
  ASSERT_EQ(reader.count(), 3);
  EXPECT_FALSE(reader.overflowed());
  EXPECT_STREQ(reader.newest(0).course, "Course 3");
  EXPECT_STREQ(reader.newest(2).course, "Course 1");
}

TEST(GolfHistory, SkipsMalformedMiddleRowAndContinues) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1) + "broken,row\r\n" + row(2));
  ASSERT_EQ(reader.count(), 2);
  EXPECT_STREQ(reader.newest(0).course, "Course 2");
  EXPECT_STREQ(reader.newest(1).course, "Course 1");
}

TEST(GolfHistory, DiscardsTruncatedFinalLineWithoutCorruptingLastValidRow) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1) + ",Course 2,18,82,72");
  ASSERT_EQ(reader.count(), 1);
  EXPECT_STREQ(reader.newest(0).course, "Course 1");
}

TEST(GolfHistory, ParFreeRowSuppressesToPar) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1, 0));
  ASSERT_EQ(reader.count(), 1);
  EXPECT_FALSE(golfHistoryShowsToPar(reader.newest(0)));
}

TEST(GolfHistory, EmptyAndHeaderOnlyYieldNoRows) {
  EXPECT_EQ(read("").count(), 0);
  EXPECT_EQ(read(HEADER).count(), 0);
}

namespace {

GolfIndexFileLocator locate(const std::string& input, const uint8_t newestIndex, const uint32_t totalValidRows) {
  GolfIndexFileLocator locator;
  locator.reset(newestIndex, totalValidRows);
  for (size_t offset = 0; offset < input.size(); offset += 13) {
    const size_t remaining = input.size() - offset;
    locator.feed(input.data() + offset, remaining < 13 ? remaining : 13);
  }
  locator.finish();
  return locator;
}

}  // namespace

TEST(GolfIndexFileLocator, ResolvesNewestFirstRowToItsFileColumn) {
  std::string input = HEADER;
  for (int number = 1; number <= 5; ++number) input += row(number);

  EXPECT_STREQ(locate(input, 0, 5).filename(), "round-0005.json");  // newest row appended last
  EXPECT_STREQ(locate(input, 4, 5).filename(), "round-0001.json");  // oldest of the five
  EXPECT_STREQ(locate(input, 2, 5).filename(), "round-0003.json");
}

TEST(GolfIndexFileLocator, HandlesOverflowNewestWindow) {
  std::string input = HEADER;
  for (int number = 1; number <= 60; ++number) input += row(number);

  const GolfIndexFileLocator newest = locate(input, 0, 60);
  EXPECT_TRUE(newest.found());
  EXPECT_STREQ(newest.filename(), "round-0060.json");
  EXPECT_STREQ(locate(input, 49, 60).filename(), "round-0011.json");
}

TEST(GolfIndexFileLocator, SkipsMalformedRowsWhenCounting) {
  const GolfIndexFileLocator locator = locate(std::string(HEADER) + row(1) + "broken,row\r\n" + row(2), 0, 2);
  EXPECT_TRUE(locator.found());
  EXPECT_STREQ(locator.filename(), "round-0002.json");
}

TEST(GolfIndexFileLocator, ReportsNotFoundForOutOfRangeIndex) {
  const GolfIndexFileLocator locator = locate(std::string(HEADER) + row(1) + row(2), 2, 2);
  EXPECT_FALSE(locator.found());
  EXPECT_STREQ(locator.filename(), "");
}

TEST(GolfIndexFileLocator, ReportsNotFoundWhenIndexEmpty) {
  const GolfIndexFileLocator locator = locate("", 0, 0);
  EXPECT_FALSE(locator.found());
}
