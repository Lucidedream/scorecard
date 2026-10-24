#include <HalStorage.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <string>

#include "CourseStore.h"
#include "GolfCatalog.h"
#include "GolfCourseCatalogInfo.h"
#include "GolfCourseParse.h"

namespace {
std::string record(uint32_t id, const char* name, const char* rating = "46", uint32_t reviews = 12,
                   uint8_t region = 0) {
  char b[128];
  snprintf(b, sizeof(b), "%08u|%02u|2|072|%s|%05u|Y|%-40s|                         \n", id, region, rating, reviews,
           name);
  return b;
}
JsonDocument courseDoc(const char* name = "Invented Course") {
  JsonDocument doc;
  deserializeJson(doc, R"({"v":2,"name":"Invented Course","nines":[{"name":"Out","par":[4,4,4,4,4,4,4,4,4]}]})");
  doc["name"] = name;
  return doc;
}
void write(const char* path, const std::string& text) {
  auto p = golfTestStorageRoot / path;
  std::filesystem::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}
}  // namespace
TEST(CatalogRegions, GoodChunkedAndFinalLine) {
  const std::string input = "# golf-catalog 1 2026-10-24 2 3\n00|East|2|0\n01|West|1|2";
  GolfCatalogRegionsReader reader;
  for (char c : input) reader.feed(&c, 1);
  ASSERT_TRUE(reader.finish());
  ASSERT_EQ(reader.count, 2);
  EXPECT_EQ(reader.courseCount, 3);
  EXPECT_STREQ(reader.regions[1].name, "West");
  EXPECT_EQ(reader.regions[1].firstRecord, 2);
}
TEST(CatalogRegions, SkipsMalformedAndOverlappingLines) {
  GolfCatalogRegionsReader reader;
  const std::string input =
      "# golf-catalog 1 2026-10-24 3 4\ninvalid\n00|East|2|0\n00|Duplicate|1|2\n01|Overlap|2|1\n02|West|2|2\n";
  reader.feed(input.data(), input.size());
  ASSERT_TRUE(reader.finish());
  EXPECT_EQ(reader.count, 2);
}
TEST(CatalogRegions, BadHeadersAndBounds) {
  for (const char* header :
       {"# golf-catalog 2 2026-10-24 1 1\n", "# golf-catalog 1 2026-02-30 1 1\n", "# golf-catalog 1 2026-10-24 65 1\n",
        "# golf-catalog 1 2026-10-24 1 65536\n", "00|East|1|0\n"}) {
    GolfCatalogRegionsReader reader;
    reader.feed(header, strlen(header));
    EXPECT_FALSE(reader.finish());
  }
  GolfCatalogRegionsReader reader;
  std::string input = "# golf-catalog 1 2026-10-24 1 513\n00|East|513|0\n";
  reader.feed(input.data(), input.size());
  EXPECT_FALSE(reader.finish());
}
TEST(CatalogRecords, GoodPaddedName) {
  auto bytes = record(123, "Invented & Club");
  ASSERT_EQ(bytes.size(), 96);
  GolfCatalogRecord r;
  ASSERT_TRUE(golfParseCatalogRecord(bytes.data(), bytes.size(), r));
  EXPECT_EQ(r.id, 123);
  EXPECT_STREQ(r.name, "Invented & Club");
  EXPECT_EQ(r.rating, 46);
  EXPECT_TRUE(r.hasMaps);
  bytes = record(4, "Unrated", "--", 0);
  ASSERT_TRUE(golfParseCatalogRecord(bytes.data(), bytes.size(), r));
  EXPECT_FALSE(r.hasRating);
}
TEST(CatalogRecords, RejectsBadLengthAndFields) {
  auto bytes = record(123, "Invented Club");
  GolfCatalogRecord r;
  EXPECT_FALSE(golfParseCatalogRecord(bytes.data(), 95, r));
  for (size_t offset : {size_t(8), size_t(12), size_t(18), size_t(27), size_t(95)}) {
    auto broken = bytes;
    broken[offset] = '?';
    EXPECT_FALSE(golfParseCatalogRecord(broken.data(), broken.size(), r));
  }
  auto broken = record(1, "Bad", "51");
  EXPECT_FALSE(golfParseCatalogRecord(broken.data(), broken.size(), r));
  broken = record(1, "Bad", "--", 1);
  EXPECT_FALSE(golfParseCatalogRecord(broken.data(), broken.size(), r));
}
TEST(CatalogSort, RatingUnratedLastAndNameTies) {
  // Catalog ordinals already carry case-insensitive name order.
  const uint8_t ratings[] = {0, 46, 49, 46};
  uint16_t ordinals[] = {10, 11, 12, 13};
  golfSortCatalogOrdinals(ordinals, 4, ratings, 10);
  EXPECT_EQ(ordinals[0], 12);
  EXPECT_EQ(ordinals[1], 11);
  EXPECT_EQ(ordinals[2], 13);
  EXPECT_EQ(ordinals[3], 10);
}
TEST(CatalogRecent, DistinctAcrossPlayersNewestFirstAndRefillsMissing) {
  GolfRecentReader reader;
  std::string input = std::string(GOLF_INDEX_HEADER);
  for (const char* name : {"Old", "One", "Two", "Three", "Four", "Five", "Two", "Missing"}) {
    input += std::string("2026-10-24,") + name + ",18,1,Player,72,72,30,20,22,0,0,0,14,0,18,round.json\r\n";
  }
  auto resolver = [](const char* name, void*) { return strcmp(name, "Missing") != 0; };
  for (size_t end = input.size(); end;) {
    const size_t size = std::min(size_t(7), end);
    end -= size;
    reader.feedReverse(input.data() + end, size, resolver);
  }
  reader.finishReverse(resolver);
  ASSERT_EQ(reader.count, 5);
  EXPECT_STREQ(reader.names[0], "Two");
  EXPECT_STREQ(reader.names[4], "One");
}
TEST(CatalogRecent, RejectsMalformedAndPartialRows) {
  GolfRecentReader reader;
  std::string input = std::string(GOLF_INDEX_HEADER) + "bad\n2026-10-24,Partial";
  reader.feedReverse(input.data(), input.size());
  reader.finishReverse();
  EXPECT_EQ(reader.count, 0);
}
TEST(CatalogCourse, MapsValidInvalidAbsentAndFallback) {
  auto doc = courseDoc();
  GolfCourse course{};
  char dir[41];
  ASSERT_TRUE(golfParseCourseJson(doc, course).valid);
  ASSERT_TRUE(golfCourseMapsDir(course, dir, sizeof(dir)));
  EXPECT_STREQ(dir, "invented-course");
  doc["maps"] = "bg-123";
  ASSERT_TRUE(golfParseCourseJson(doc, course).valid);
  EXPECT_STREQ(course.mapsDir, "bg-123");
  ASSERT_TRUE(golfCourseMapsDir(course, dir, sizeof(dir)));
  EXPECT_STREQ(dir, "bg-123");
  for (const char* value : {"", "../escape", "BG-12", "1234567890123456", "bad_name"}) {
    doc["maps"] = value;
    EXPECT_FALSE(golfParseCourseJson(doc, course).valid);
  }
  doc["maps"] = nullptr;
  EXPECT_FALSE(golfParseCourseJson(doc, course).valid);
}
TEST(CatalogFacts, Full) {
  JsonDocument doc;
  ASSERT_FALSE(deserializeJson(
      doc,
      R"({"baigolf":{"id":123,"rating":46,"reviews":1234,"scores":[41,42,43,44,45,46],"region":"Invented Region","holes":27,"par":72,"yards":7160,"designer":"Invented Designer","opened":1995}})"));
  GolfCourseCatalogInfo info;
  ASSERT_TRUE(golfParseCatalogInfo(doc, info));
  EXPECT_EQ(info.id, 123);
  EXPECT_TRUE(info.hasScores);
  EXPECT_EQ(info.scores[5], 46);
  EXPECT_TRUE(info.hasOpened);
  EXPECT_STREQ(info.region, "Invented Region");
}
TEST(CatalogFacts, PartialAndMalformedClearsEverything) {
  JsonDocument doc;
  doc["baigolf"]["id"] = 123;
  GolfCourseCatalogInfo info;
  ASSERT_TRUE(golfParseCatalogInfo(doc, info));
  EXPECT_FALSE(info.hasRating);
  EXPECT_FALSE(info.hasScores);
  for (const char* bad :
       {R"({"id":123,"rating":51,"reviews":2})", R"({"id":123,"scores":[1,2]})", R"({"id":123,"opened":999})",
        R"({"id":123,"rating":46})", R"({"id":123,"region":null})", R"({"id":-1})"}) {
    JsonDocument b;
    deserializeJson(b, bad);
    doc["baigolf"] = b;
    EXPECT_FALSE(golfParseCatalogInfo(doc, info));
    EXPECT_EQ(info.id, 0);
    EXPECT_FALSE(info.hasScores);
  }
  doc.remove("baigolf");
  EXPECT_FALSE(golfParseCatalogInfo(doc, info));
}
class CatalogStore : public ::testing::Test {
 protected:
  void SetUp() override {
    golfTestStorageRoot = std::filesystem::temp_directory_path() / "golf-catalog-synthetic-tests";
    std::filesystem::remove_all(golfTestStorageRoot);
    std::filesystem::create_directories(golfTestStorageRoot);
  }
  void TearDown() override { std::filesystem::remove_all(golfTestStorageRoot); }
  void writeCourse(const char* path, const char* name) {
    auto doc = courseDoc(name);
    std::string json;
    serializeJson(doc, json);
    write(path, json);
  }
};
TEST_F(CatalogStore, LookupOrderOwnerBuiltInCatalogAndSourcePaths) {
  writeCourse("golf/courses/override.json", "Sanyang Golf Club");
  writeCourse("golf/catalog/courses/bg-123.json", "Sanyang Golf Club");
  write("golf/catalog/index.dat", record(123, "Sanyang Golf Club"));
  GolfCourseFile file{};
  GolfCourse course{};
  ASSERT_TRUE(CourseStore::findByName("Sanyang Golf Club", file, course));
  EXPECT_EQ(file.source, GolfCourseSource::Owner);
  EXPECT_EQ(course.nineCount, 1);
  std::filesystem::remove(golfTestStorageRoot / "golf/courses/override.json");
  ASSERT_TRUE(CourseStore::findByName("Sanyang Golf Club", file, course));
  EXPECT_EQ(file.source, GolfCourseSource::BuiltIn);
  EXPECT_EQ(course.nineCount, 2);
  writeCourse("golf/courses/z.json", "Invented Course");
  writeCourse("golf/courses/a.json", "Invented Course");
  writeCourse("golf/catalog/courses/bg-123.json", "Invented Course");
  write("golf/catalog/index.dat", record(123, "Invented Course"));
  ASSERT_TRUE(CourseStore::findByName("Invented Course", file, course));
  EXPECT_EQ(file.source, GolfCourseSource::Owner);
  EXPECT_STREQ(file.filename, "a.json");
  std::filesystem::remove(golfTestStorageRoot / "golf/courses/a.json");
  std::filesystem::remove(golfTestStorageRoot / "golf/courses/z.json");
  ASSERT_TRUE(CourseStore::findByName("Invented Course", file, course));
  EXPECT_EQ(file.source, GolfCourseSource::Catalog);
  EXPECT_STREQ(file.filename, "bg-123.json");
  ASSERT_TRUE(CourseStore::load(file, course));
  EXPECT_STREQ(course.courseName, "Invented Course");
  EXPECT_FALSE(CourseStore::load("../bg-123.json", course));
}
TEST_F(CatalogStore, SkipsBadCatalogRecordsButRejectsWrongLength) {
  writeCourse("golf/catalog/courses/bg-2.json", "Invented Course");
  write("golf/catalog/index.dat", std::string(96, '?') + record(2, "Invented Course"));
  GolfCourseFile file{};
  GolfCourse course{};
  ASSERT_TRUE(CourseStore::findByName("Invented Course", file, course));
  write("golf/catalog/index.dat", record(2, "Invented Course") + "x");
  EXPECT_FALSE(CourseStore::findByName("Invented Course", file, course));
}

TEST(CatalogCourse, MalformedFactsNeverRejectCourse) {
  auto doc = courseDoc();
  doc["baigolf"]["id"] = -1;
  GolfCourse course{};
  ASSERT_TRUE(golfParseCourseJson(doc, course).valid);
  GolfCourseCatalogInfo info;
  EXPECT_FALSE(golfParseCatalogInfo(doc, info));
  EXPECT_EQ(info.id, 0);
}

TEST(CatalogRecords, AcceptsUtf8AndRejectsBrokenSequences) {
  GolfCatalogRecord parsed;
  auto bytes = record(12, "Invented Caf\xc3\xa9");
  EXPECT_TRUE(golfParseCatalogRecord(bytes.data(), bytes.size(), parsed));
  bytes[29] = char(0xc0);
  EXPECT_FALSE(golfParseCatalogRecord(bytes.data(), bytes.size(), parsed));
}

TEST(CatalogRecent, ReverseStopsAtFiveAndResolvesOnce) {
  GolfRecentReader reader;
  std::string input = std::string(GOLF_INDEX_HEADER);
  for (const char* name : {"Old", "One", "Two", "Three", "Four", "Five", "Five"})
    input += std::string("2026-10-24,") + name + ",18,1,Player,72,72,30,20,22,0,0,0,14,0,18,round.json\r\n";
  int calls = 0;
  auto resolver = [](const char*, void* user) {
    ++*static_cast<int*>(user);
    return true;
  };
  reader.feedReverse(input.data(), input.size(), resolver, &calls);
  reader.finishReverse(resolver, &calls);
  EXPECT_EQ(calls, 5);
  EXPECT_EQ(reader.count, 5);
  EXPECT_STREQ(reader.names[0], "Five");
  EXPECT_STREQ(reader.names[4], "One");
}
