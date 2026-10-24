#include <ArduinoJson.h>
#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "CourseBuiltIns.h"
#include "GolfCourseParse.h"
class GolfCourseParse : public ::testing::Test {
 protected:
  JsonDocument doc;
  GolfCourse course{};
  void SetUp() override {
    doc["v"] = 2;
    doc["name"] = "Test Club";
    auto nines = doc["nines"].to<JsonArray>();
    for (const char* name : {"Out", "In"}) {
      auto nine = nines.add<JsonObject>();
      nine["name"] = name;
      auto par = nine["par"].to<JsonArray>();
      for (int h = 0; h < 9; ++h) par.add(4);
      auto si = nine["si"].to<JsonArray>();
      for (int h = 0; h < 9; ++h) si.add(h + 1);
    }
    auto tees = doc["tees"].to<JsonArray>();
    for (const char* name : {"White", "Blue"}) {
      auto tee = tees.add<JsonObject>();
      tee["name"] = name;
      auto yards = tee["yards"].to<JsonArray>();
      for (int n = 0; n < 2; ++n) {
        auto nine = yards.add<JsonArray>();
        for (int h = 0; h < 9; ++h) nine.add(strcmp(name, "Blue") == 0 ? 400 : 300);
      }
    }
  }
  bool parse() { return golfParseCourseJson(doc.as<JsonVariantConst>(), course).valid; }
  void rejected() {
    strcpy(course.courseName, "Untouched");
    EXPECT_FALSE(parse());
    EXPECT_STREQ(course.courseName, "Untouched");
  }
};
TEST_F(GolfCourseParse, ValidAndLongestFirst) {
  ASSERT_TRUE(parse());
  EXPECT_EQ(course.nineCount, 2);
  EXPECT_EQ(course.teeCount, 2);
  EXPECT_STREQ(course.tees[0].name, "Blue");
  EXPECT_TRUE(validateGolfCourse(course).valid);
}
TEST_F(GolfCourseParse, SynthesizesBlueWhiteWhenTeesAbsentOrEmpty) {
  doc.remove("tees");
  ASSERT_TRUE(parse());
  EXPECT_EQ(course.teeCount, 2);
  EXPECT_STREQ(course.tees[0].name, "Blue");
  EXPECT_FALSE(course.tees[0].hasYards);
  doc["tees"].to<JsonArray>();
  ASSERT_TRUE(parse());
  EXPECT_EQ(course.teeCount, 2);
}
TEST_F(GolfCourseParse, AcceptsYardlessAndOptionalSiAndUnknownFields) {
  doc["tees"][0].as<JsonObject>().remove("yards");
  doc["nines"][0].as<JsonObject>().remove("si");
  doc["ignored"] = true;
  ASSERT_TRUE(parse());
  EXPECT_FALSE(course.nines[0].hasSi);
  EXPECT_FALSE(course.tees[1].hasYards);
}
TEST_F(GolfCourseParse, RejectsVersionOneMissingAndWrongType) {
  for (auto v : {1, 0, 3}) {
    doc["v"] = v;
    rejected();
  }
  doc["v"] = "2";
  rejected();
}
TEST_F(GolfCourseParse, RejectsInvalidCourseNames) {
  for (const char* name : {"", "Bad\nName", "1234567890123456789012345678901234567890"}) {
    doc["name"] = name;
    rejected();
  }
  doc["name"] = 42;
  rejected();
}
TEST_F(GolfCourseParse, RejectsInvalidNineCount) {
  doc["nines"].to<JsonArray>();
  rejected();
  for (int i = 0; i < 5; ++i) doc["nines"].as<JsonArray>().add<JsonObject>();
  rejected();
  doc["nines"] = nullptr;
  rejected();
}
TEST_F(GolfCourseParse, RejectsNineNamesMissingDuplicateLongMultilineOrComma) {
  for (const char* name : {"", "Out", "123456789012", "Bad\nName", "Bad,Name"}) {
    doc["nines"][1]["name"] = name;
    rejected();
  }
  doc["nines"][1].as<JsonObject>().remove("name");
  rejected();
}
TEST_F(GolfCourseParse, RejectsParMissingWrongLengthTypeAndRange) {
  auto par = doc["nines"][0]["par"].as<JsonArray>();
  par[0] = 2;
  rejected();
  par[0] = 7;
  rejected();
  par[0] = "4";
  rejected();
  par[0] = 4;
  par.remove(8);
  rejected();
  par.add(4);
  par.add(4);
  rejected();
  doc["nines"][0].as<JsonObject>().remove("par");
  rejected();
}
TEST_F(GolfCourseParse, RejectsSiLengthTypeRangeAndWithinNineDuplicates) {
  auto si = doc["nines"][0]["si"].as<JsonArray>();
  for (int v : {0, 19, 2}) {
    si[0] = v;
    rejected();
  }
  si[0] = "1";
  rejected();
  si[0] = 1;
  si.remove(8);
  rejected();
  si.add(9);
  si.add(10);
  rejected();
}
TEST_F(GolfCourseParse, SingleNineSiRangeIsNine) {
  doc["nines"].as<JsonArray>().remove(1);
  doc.remove("tees");
  ASSERT_TRUE(parse());
  doc["nines"][0]["si"][0] = 10;
  rejected();
}
TEST_F(GolfCourseParse, RejectsTeeCountAndWrongType) {
  auto tees = doc["tees"].as<JsonArray>();
  for (int i = 0; i < 5; ++i) tees.add<JsonObject>();
  rejected();
  doc["tees"] = nullptr;
  rejected();
}
TEST_F(GolfCourseParse, RejectsTeeNamesMissingDuplicateLongMultilineCommaOrUtf8) {
  for (const char* name : {"", "White", "123456789012", "Bad\nName", "Bad,Name", "\xff"}) {
    doc["tees"][1]["name"] = name;
    rejected();
  }
  doc["tees"][1].as<JsonObject>().remove("name");
  rejected();
}
TEST_F(GolfCourseParse, RejectsYardsShapeTypeAndRange) {
  auto yards = doc["tees"][0]["yards"].as<JsonArray>();
  auto nine = yards[0].as<JsonArray>();
  for (int v : {0, 1000, -1}) {
    nine[0] = v;
    rejected();
  }
  nine[0] = "300";
  rejected();
  nine[0] = 300;
  nine.remove(8);
  rejected();
  nine.add(300);
  nine.add(300);
  rejected();
  nine.remove(9);
  yards.remove(1);
  rejected();
  yards.add<JsonArray>();
  yards.add<JsonArray>();
  rejected();
  doc["tees"][0]["yards"] = nullptr;
  rejected();
}
TEST(GolfCourseExamples, ParsesV2ExamplesAtRuntime) {
  size_t parsed = 0;
  for (const auto& entry : std::filesystem::directory_iterator(GOLF_EXAMPLES_DIRECTORY)) {
    if (entry.path().extension() != ".json") continue;
    std::ifstream file(entry.path());
    std::stringstream text;
    text << file.rdbuf();
    JsonDocument doc;
    ASSERT_FALSE(deserializeJson(doc, text.str())) << entry.path();
    if ((doc["v"] | 0) == 1) {
      std::cout << "SKIP v1 example awaiting conversion: " << entry.path() << '\n';
      continue;
    }
    auto course = std::make_unique<GolfCourse>();
    auto result = golfParseCourseJson(doc.as<JsonVariantConst>(), *course);
    if (entry.path().filename() == "sanyang-suzhou.json") {
      EXPECT_FALSE(result.valid);
      continue;
    }
    EXPECT_TRUE(result.valid) << entry.path() << ": " << golfCourseValidationErrorName(result.error);
    ++parsed;
  }
  std::cout << "Parsed v2 examples: " << parsed << '\n';
}
TEST(GolfCourseBuiltins, ValidExceptParFreeTemplate) {
  for (uint8_t i = 0; i < 3; ++i) EXPECT_TRUE(validateGolfCourse(GOLF_BUILT_IN_COURSES[i]).valid);
  EXPECT_EQ(GOLF_BUILT_IN_COURSES[3].nines[0].par[0], 0);
}
