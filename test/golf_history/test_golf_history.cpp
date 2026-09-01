#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "GolfHistory.h"

namespace {

constexpr char HEADER[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,file\r\n";
static_assert(sizeof(GolfHistoryEntry) <= 96);

std::string row(const int number, const uint8_t slot = 0, const char* name = "Noah", const uint16_t par = 72) {
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  snprintf(output, sizeof(output), ",Course %d,18,%u,%s,%d,%u,32,54,32,1,2,round-%04d.json\r\n", number, slot,
           name, 80 + number, par, number);
  return output;
}

GolfHistoryReader read(const std::string& input, const uint8_t slot) {
  GolfHistoryReader reader;
  EXPECT_TRUE(reader.reset(slot));
  for (size_t offset = 0; offset < input.size(); offset += 17) {
    const size_t remaining = input.size() - offset;
    reader.feed(input.data() + offset, remaining < 17 ? remaining : 17);
  }
  reader.finish();
  return reader;
}

GolfIndexFileLocator locate(const std::string& input, const uint8_t slot, const uint8_t newestIndex,
                            const uint32_t totalFilteredRows) {
  GolfIndexFileLocator locator;
  locator.reset(slot, newestIndex, totalFilteredRows);
  for (size_t offset = 0; offset < input.size(); offset += 13) {
    const size_t remaining = input.size() - offset;
    locator.feed(input.data() + offset, remaining < 13 ? remaining : 13);
  }
  locator.finish();
  return locator;
}

void countMalformed(uint32_t, void* user) { ++*static_cast<uint8_t*>(user); }

GolfPlayerNamesReader readNames(const std::string& input) {
  GolfPlayerNamesReader reader;
  reader.reset();
  for (size_t offset = 0; offset < input.size(); offset += 11) {
    const size_t remaining = input.size() - offset;
    reader.feed(input.data() + offset, remaining < 11 ? remaining : 11);
  }
  reader.finish();
  return reader;
}

}  // namespace

TEST(GolfHistory, FiltersBySlotBeforeFiftyEntryCap) {
  std::string input = HEADER;
  for (int number = 1; number <= 60; ++number) {
    input += row(number, 0, "Noah");
    input += row(number, 2, "Guest");
  }
  const GolfHistoryReader reader = read(input, 0);
  ASSERT_EQ(reader.count(), 50);
  EXPECT_TRUE(reader.overflowed());
  EXPECT_EQ(reader.totalValidRows(), 60u);
  EXPECT_STREQ(reader.newest(0).course, "Course 60");
  EXPECT_STREQ(reader.newest(49).course, "Course 11");
  for (uint8_t index = 0; index < reader.count(); ++index) EXPECT_EQ(reader.newest(index).playerSlot, 0);
}

TEST(GolfHistory, KeepsSelectedRowsNewestFirstWhenUnderCapacity) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1, 0) + row(2, 1, "B") + row(3, 0), 0);
  ASSERT_EQ(reader.count(), 2);
  EXPECT_FALSE(reader.overflowed());
  EXPECT_STREQ(reader.newest(0).course, "Course 3");
  EXPECT_STREQ(reader.newest(1).course, "Course 1");
}

TEST(GolfHistory, SkipsMalformedRowsAndDiscardsTruncatedTail) {
  const GolfHistoryReader reader =
      read(std::string(HEADER) + row(1) + "broken,row\r\n" + row(2) + ",Course 3,18,0,Noah,83", 0);
  ASSERT_EQ(reader.count(), 2);
  EXPECT_STREQ(reader.newest(0).course, "Course 2");
}

TEST(GolfHistory, NormalReaderDoesNotMapLegacyRowsIntoSlotZero) {
  const std::string input = std::string("date,course,holes,strokes,par,putts,in100,out100,file\r\n") +
                            ",Old,18,85,72,33,52,30,round-0001.json\r\n" +
                            ",New,18,90,72,35,55,30,2,1,round-0002.json\r\n";
  EXPECT_EQ(read(input, 0).count(), 0);
  EXPECT_EQ(read(input, 1).count(), 0);
}

TEST(GolfHistory, V4HeaderFlagsEveryLegacyShapedNonemptyRow) {
  const std::string input = std::string(HEADER) + ",Old,18,85,72,33,52,30,round-0001.json\r\n" +
                            ",New,18,90,72,35,55,30,2,1,round-0002.json\r\n";
  GolfHistoryReader reader;
  ASSERT_TRUE(reader.reset(0));
  uint8_t malformed = 0;
  reader.feed(input.data(), input.size(), &countMalformed, &malformed);
  reader.finish(&countMalformed, &malformed);
  EXPECT_EQ(reader.count(), 0);
  EXPECT_EQ(malformed, 2);
}

TEST(GolfHistory, ParFreeRowSuppressesToPar) {
  const GolfHistoryReader reader = read(std::string(HEADER) + row(1, 0, "Noah", 0), 0);
  ASSERT_EQ(reader.count(), 1);
  EXPECT_FALSE(golfHistoryShowsToPar(reader.newest(0)));
}

TEST(GolfHistory, RejectsInvalidSelectedSlot) {
  GolfHistoryReader reader;
  EXPECT_FALSE(reader.reset(GolfRound::MAX_PLAYERS));
  reader.feed(HEADER, sizeof(HEADER) - 1);
  reader.finish();
  EXPECT_EQ(reader.count(), 0);
}

TEST(GolfIndexFileLocator, ResolvesSameSlotFilteredOrdinal) {
  std::string input = HEADER;
  for (int number = 1; number <= 5; ++number) {
    input += row(number, 0, "Noah");
    input += row(number, 2, "Guest");
  }
  EXPECT_STREQ(locate(input, 0, 0, 5).filename(), "round-0005.json");
  EXPECT_STREQ(locate(input, 0, 4, 5).filename(), "round-0001.json");
  EXPECT_STREQ(locate(input, 2, 2, 5).filename(), "round-0003.json");
}

TEST(GolfIndexFileLocator, HandlesFilteredOverflowWindow) {
  std::string input = HEADER;
  for (int number = 1; number <= 60; ++number) {
    input += row(number, 1, "B");
    input += row(number, 3, "D");
  }
  EXPECT_STREQ(locate(input, 3, 0, 60).filename(), "round-0060.json");
  EXPECT_STREQ(locate(input, 3, 49, 60).filename(), "round-0011.json");
}

TEST(GolfIndexFileLocator, SkipsMalformedAndOtherSlotRowsWhenCounting) {
  const std::string input = std::string(HEADER) + row(1, 0) + row(2, 1, "B") + "broken,row\r\n" + row(3, 0);
  const GolfIndexFileLocator locator = locate(input, 0, 0, 2);
  EXPECT_TRUE(locator.found());
  EXPECT_STREQ(locator.filename(), "round-0003.json");
}

TEST(GolfIndexFileLocator, ReportsNotFoundForInvalidSelection) {
  const std::string input = std::string(HEADER) + row(1, 0);
  EXPECT_FALSE(locate(input, 0, 1, 1).found());
  EXPECT_FALSE(locate(input, GolfRound::MAX_PLAYERS, 0, 1).found());
}

TEST(GolfPlayerNamesReader, FindsFirstPresentStableSlotForInitialSelection) {
  const GolfPlayerNamesReader none = readNames(HEADER);
  EXPECT_EQ(none.firstPresent(), GolfRound::NO_PLAYER);

  const GolfPlayerNamesReader laterSlots =
      readNames(std::string(HEADER) + row(1, 3, "Fourth") + row(2, 1, "Second"));
  EXPECT_EQ(laterSlots.firstPresent(), 1);
}

TEST(GolfPlayerNamesReader, KeepsLatestSnapshotPerStableSlotWithDefaults) {
  const std::string input = std::string(HEADER) + row(1, 0, "Old Noah") + row(1, 2, "First Guest") +
                            row(2, 0, "New Noah") + row(2, 2, "Latest Guest");
  const GolfPlayerNamesReader names = readNames(input);
  EXPECT_TRUE(names.present(0));
  EXPECT_TRUE(names.present(2));
  EXPECT_FALSE(names.present(1));
  EXPECT_STREQ(names.name(0), "New Noah");
  EXPECT_STREQ(names.name(1), "Player B");
  EXPECT_STREQ(names.name(2), "Latest Guest");
  EXPECT_STREQ(names.name(3), "Player D");
}
