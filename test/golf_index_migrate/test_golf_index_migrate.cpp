#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "GolfCsv.h"
#include "GolfIndexMigrate.h"

namespace {

constexpr char HEADER_V2[] = "date,course,holes,strokes,par,putts,in100,out100,file\r\n";
constexpr char HEADER_V3[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file\r\n";

struct Sink {
  std::string out;
  bool fail = false;
  size_t allowBytes = SIZE_MAX;
};

bool sink(const char* data, size_t size, void* user) {
  auto* s = static_cast<Sink*>(user);
  if (s->fail) return false;
  if (s->out.size() + size > s->allowBytes) return false;
  s->out.append(data, size);
  return true;
}

// Feeds `input` to a migrator in small chunks, mirroring the firmware read loop.
GolfIndexMigrator migrate(const std::string& input, Sink& s) {
  GolfIndexMigrator migrator;
  migrator.reset();
  for (size_t offset = 0; offset < input.size(); offset += 7) {
    const size_t remaining = input.size() - offset;
    if (!migrator.feed(input.data() + offset, remaining < 7 ? remaining : 7, &sink, &s)) break;
  }
  migrator.finish();
  return migrator;
}

std::string v2Row(const char* course, uint16_t strokes, uint16_t par, uint16_t putts, uint16_t in100, uint16_t out100,
                  const char* file) {
  char buf[192];
  snprintf(buf, sizeof(buf), ",%s,18,%u,%u,%u,%u,%u,%s\r\n", course, strokes, par, putts, in100, out100, file);
  return buf;
}

}  // namespace

TEST(GolfIndexHeaderVersion, ClassifiesBothHeadersAndRejectsOthers) {
  EXPECT_EQ(golfIndexHeaderVersion("date,course,holes,strokes,par,putts,in100,out100,file"), GolfIndexVersion::V2);
  EXPECT_EQ(golfIndexHeaderVersion("date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file"),
            GolfIndexVersion::V3);
  EXPECT_EQ(golfIndexHeaderVersion("something,else"), GolfIndexVersion::Unknown);
  EXPECT_EQ(golfIndexHeaderVersion(""), GolfIndexVersion::Unknown);
}

TEST(GolfIndexMigrate, V2FileMigratesToV3PreservingRowCountAndValuesWithEmptyPenalties) {
  const std::string input = std::string(HEADER_V2) + v2Row("Pebble", 82, 72, 33, 52, 30, "round-0001-pebble.json") +
                            v2Row("Sanyang", 90, 71, 36, 58, 32, "round-0002-sanyang.json") +
                            v2Row("MoganShan", 85, 72, 34, 55, 30, "round-0003-mogan.json");
  Sink s;
  const GolfIndexMigrator migrator = migrate(input, s);

  EXPECT_EQ(migrator.sourceVersion(), GolfIndexVersion::V2);
  EXPECT_TRUE(migrator.needsMigration());
  EXPECT_FALSE(migrator.aborted());
  EXPECT_EQ(migrator.dataRows(), 3u);

  // v3 header, then three widened rows.
  ASSERT_EQ(s.out.rfind(HEADER_V3, 0), 0u);
  EXPECT_NE(s.out.find(",52,30,,,round-0001-pebble.json\r\n"), std::string::npos);
  EXPECT_NE(s.out.find(",58,32,,,round-0002-sanyang.json\r\n"), std::string::npos);

  // Every emitted row parses as v3, keeps its totals, and reads as "not recorded".
  size_t newline = s.out.find('\n');  // skip header
  int rows = 0;
  for (size_t start = newline + 1; start < s.out.size();) {
    const size_t end = s.out.find('\n', start);
    const std::string line = s.out.substr(start, end - start + 1);
    GolfIndexRow row{};
    ASSERT_TRUE(golfParseIndexRow(line.c_str(), row)) << line;
    EXPECT_FALSE(row.penaltiesRecorded);
    EXPECT_EQ(row.hazards, 0);
    EXPECT_EQ(row.obs, 0);
    EXPECT_EQ(row.holes, 18);
    ++rows;
    start = end + 1;
  }
  EXPECT_EQ(rows, 3);
}

TEST(GolfIndexMigrate, AlreadyV3FileIsNotRewritten) {
  const std::string input = std::string(HEADER_V3) + ",Pebble,18,82,72,33,52,30,2,1,round-0001.json\r\n";
  Sink s;
  const GolfIndexMigrator migrator = migrate(input, s);

  EXPECT_EQ(migrator.sourceVersion(), GolfIndexVersion::V3);
  EXPECT_FALSE(migrator.needsMigration());
  EXPECT_EQ(migrator.dataRows(), 1u);  // counted for verification, not emitted
  EXPECT_TRUE(s.out.empty());
}

TEST(GolfIndexMigrate, EmptyAndHeaderOnlyFilesMigrateWithoutError) {
  Sink empty;
  const GolfIndexMigrator emptyMigrator = migrate("", empty);
  EXPECT_EQ(emptyMigrator.sourceVersion(), GolfIndexVersion::Unknown);
  EXPECT_FALSE(emptyMigrator.needsMigration());
  EXPECT_FALSE(emptyMigrator.aborted());
  EXPECT_TRUE(empty.out.empty());

  Sink headerOnly;
  const GolfIndexMigrator headerMigrator = migrate(HEADER_V2, headerOnly);
  EXPECT_TRUE(headerMigrator.needsMigration());
  EXPECT_EQ(headerMigrator.dataRows(), 0u);
  EXPECT_EQ(headerOnly.out, HEADER_V3);
}

TEST(GolfIndexMigrate, MalformedRowsAreDroppedExactlyAsTheReaderDropsThem) {
  const std::string input = std::string(HEADER_V2) + v2Row("Pebble", 82, 72, 33, 52, 30, "round-0001.json") +
                            "garbage,not,a,row\r\n" + v2Row("Sanyang", 90, 71, 36, 58, 32, "round-0002.json");
  Sink s;
  const GolfIndexMigrator migrator = migrate(input, s);

  EXPECT_EQ(migrator.dataRows(), 2u);
  EXPECT_EQ(std::count(s.out.begin(), s.out.end(), '\n'), 3);  // header + 2 rows
}

TEST(GolfIndexMigrate, UnterminatedFinalRowIsDroppedLikeAnInterruptedWrite) {
  const std::string input = std::string(HEADER_V2) + v2Row("Pebble", 82, 72, 33, 52, 30, "round-0001.json") +
                            ",Sanyang,18,90,71,36";  // power-loss tail, no newline
  Sink s;
  const GolfIndexMigrator migrator = migrate(input, s);

  EXPECT_EQ(migrator.dataRows(), 1u);
  EXPECT_EQ(std::count(s.out.begin(), s.out.end(), '\n'), 2);
}

TEST(GolfIndexMigrate, WriteFailureAbortsWithoutClaimingSuccess) {
  const std::string input = std::string(HEADER_V2) + v2Row("Pebble", 82, 72, 33, 52, 30, "round-0001.json") +
                            v2Row("Sanyang", 90, 71, 36, 58, 32, "round-0002.json");
  Sink s;
  s.allowBytes = sizeof(HEADER_V3) + 4;  // header fits, first row does not
  const GolfIndexMigrator migrator = migrate(input, s);

  EXPECT_TRUE(migrator.aborted());
}

TEST(GolfIndexMigrate, RoundTripsThroughItsOwnOutputForVerification) {
  const std::string input = std::string(HEADER_V2) + v2Row("Pebble", 82, 72, 33, 52, 30, "round-0001.json") +
                            v2Row("Sanyang", 90, 71, 36, 58, 32, "round-0002.json");
  Sink first;
  const GolfIndexMigrator writer = migrate(input, first);

  Sink second;
  const GolfIndexMigrator verifier = migrate(first.out, second);
  EXPECT_EQ(verifier.sourceVersion(), GolfIndexVersion::V3);
  EXPECT_EQ(verifier.dataRows(), writer.dataRows());
  EXPECT_TRUE(second.out.empty());
}
