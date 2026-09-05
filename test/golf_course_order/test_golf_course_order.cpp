#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CourseBuiltIns.h"
#include "CourseOrder.h"
#include "CourseStore.h"

namespace {

struct Entry {
  GolfCourseFile file;
  GolfCourse course;
};

Entry builtInEntry(uint8_t index) {
  Entry entry{};
  entry.file = {};
  entry.file.builtInIndex = static_cast<int8_t>(index);
  entry.course = GOLF_BUILT_IN_COURSES[index];
  return entry;
}

// An SD file. builtInIndex == -1 for a file that overrides nothing; otherwise the table
// slot of the built-in it overrides, as CourseStore::enumerate() now carries through.
Entry sdEntry(const char* filename, const char* courseName, int8_t builtInIndex = -1) {
  Entry entry{};
  entry.file = {};
  std::snprintf(entry.file.filename, sizeof(entry.file.filename), "%s", filename);
  entry.file.builtInIndex = builtInIndex;
  std::snprintf(entry.course.courseName, sizeof(entry.course.courseName), "%s", courseName);
  return entry;
}

// An SD tee file: carries a tee label and, optionally, yardages -- the shape
// docs/golf/examples/sanyang-suzhou.json / -white.json actually take.
Entry sdTeeEntry(const char* filename, const char* courseName, const char* tee, const uint16_t* yards = nullptr) {
  Entry entry = sdEntry(filename, courseName);
  std::snprintf(entry.course.tees, sizeof(entry.course.tees), "%s", tee);
  entry.course.holeCount = 18;
  entry.course.hasYards = yards != nullptr;
  if (yards != nullptr) std::memcpy(entry.course.yards, yards, sizeof(entry.course.yards[0]) * entry.course.holeCount);
  return entry;
}

// Mirrors GolfSetupActivity::loadCourses(): a stable insertion sort keyed on
// golfCourseSortsBefore().
void sortEntries(std::vector<Entry>& entries) {
  for (size_t i = 1; i < entries.size(); ++i) {
    const Entry value = entries[i];
    size_t position = i;
    while (position > 0 &&
           golfCourseSortsBefore(value.file, value.course, entries[position - 1].file, entries[position - 1].course)) {
      entries[position] = entries[position - 1];
      --position;
    }
    entries[position] = value;
  }
}

std::vector<std::string> orderedNames(std::vector<Entry> entries) {
  sortEntries(entries);
  std::vector<std::string> names;
  for (const Entry& entry : entries) names.push_back(entry.course.courseName);
  return names;
}

bool resolveAllTees(const std::vector<Entry>& entries, const char* courseName, GolfCourseTeeSet& result) {
  std::vector<GolfCourseFile> files;
  std::vector<GolfCourse> courses;
  for (const Entry& entry : entries) {
    files.push_back(entry.file);
    courses.push_back(entry.course);
  }
  return golfResolveAllTeesFrom(files.data(), courses.data(), static_cast<uint8_t>(files.size()), courseName, result);
}

}  // namespace

TEST(GolfCourseOrder, BuiltInsSortBeforeUnrelatedSdCourses) {
  const Entry builtIn = builtInEntry(SANYANG_BUILT_IN_INDEX);
  const Entry sd = sdEntry("aardvark.json", "Aardvark Links");

  EXPECT_TRUE(golfCourseSortsBefore(builtIn.file, builtIn.course, sd.file, sd.course));
  EXPECT_FALSE(golfCourseSortsBefore(sd.file, sd.course, builtIn.file, builtIn.course));
}

TEST(GolfCourseOrder, BuiltInsKeepTableOrderAndNeverResort) {
  const Entry sanyang = builtInEntry(SANYANG_BUILT_IN_INDEX);
  const Entry moganshan = builtInEntry(MOGANSHAN_BUILT_IN_INDEX);

  EXPECT_TRUE(golfCourseSortsBefore(sanyang.file, sanyang.course, moganshan.file, moganshan.course));
  EXPECT_FALSE(golfCourseSortsBefore(moganshan.file, moganshan.course, sanyang.file, sanyang.course));
}

TEST(GolfCourseOrder, UnrelatedSdCoursesSortAlphabeticallyCaseInsensitive) {
  const Entry lower = sdEntry("b.json", "banff springs");
  const Entry upper = sdEntry("a.json", "Augusta National");

  EXPECT_TRUE(golfCourseSortsBefore(upper.file, upper.course, lower.file, lower.course));
  EXPECT_FALSE(golfCourseSortsBefore(lower.file, lower.course, upper.file, upper.course));
}

TEST(GolfCourseOrder, BuiltInOnlyListStaysInTableOrder) {
  // Fed in reverse; the sort must restore Sanyang, MoganShan, Pebble Beach, Template.
  std::vector<Entry> entries;
  for (int8_t index = GOLF_BUILT_IN_COURSE_COUNT - 1; index >= 0; --index) {
    entries.push_back(builtInEntry(static_cast<uint8_t>(index)));
  }

  EXPECT_EQ(orderedNames(entries),
            (std::vector<std::string>{"Sanyang Golf Club", "MoganShan Gowin", "Pebble Beach", "Template course"}));
}

TEST(GolfCourseOrder, BuiltInsFirstThenSdCoursesAlphabetically) {
  std::vector<Entry> entries;
  // Enumerate order: SD files (directory order) first, then non-overridden built-ins.
  entries.push_back(sdEntry("standrews.json", "St Andrews"));
  entries.push_back(sdEntry("augusta.json", "Augusta National"));
  for (uint8_t index = 0; index < GOLF_BUILT_IN_COURSE_COUNT; ++index) entries.push_back(builtInEntry(index));

  EXPECT_EQ(orderedNames(entries), (std::vector<std::string>{"Sanyang Golf Club", "MoganShan Gowin", "Pebble Beach",
                                                             "Template course", "Augusta National", "St Andrews"}));
}

TEST(GolfCourseOrder, SdOverrideLandsInItsBuiltInsSlotNotAmongSdCourses) {
  std::vector<Entry> entries;
  // A corrected Sanyang plus an unrelated SD course, then the other built-ins.
  entries.push_back(sdEntry("Sanyang Golf Club.json", "Sanyang Golf Club", SANYANG_BUILT_IN_INDEX));
  entries.push_back(sdEntry("augusta.json", "Augusta National"));
  entries.push_back(builtInEntry(MOGANSHAN_BUILT_IN_INDEX));
  entries.push_back(builtInEntry(2));  // Pebble Beach
  entries.push_back(builtInEntry(3));  // Template course

  sortEntries(entries);

  const std::vector<std::string> expected{"Sanyang Golf Club", "MoganShan Gowin", "Pebble Beach", "Template course",
                                          "Augusta National"};
  std::vector<std::string> names;
  for (const Entry& entry : entries) names.push_back(entry.course.courseName);
  EXPECT_EQ(names, expected);

  // The course in Sanyang's slot is the SD override, not the flash built-in.
  EXPECT_STREQ(entries[0].file.filename, "Sanyang Golf Club.json");
  EXPECT_EQ(entries[0].file.builtInIndex, SANYANG_BUILT_IN_INDEX);
}

TEST(GolfCourseOrder, SdOverrideOfAMiddleBuiltInKeepsThatPosition) {
  std::vector<Entry> entries;
  entries.push_back(sdEntry("pebble.json", "Pebble Beach", 2));
  entries.push_back(sdEntry("zzz.json", "Zzz Country Club"));
  entries.push_back(builtInEntry(SANYANG_BUILT_IN_INDEX));
  entries.push_back(builtInEntry(MOGANSHAN_BUILT_IN_INDEX));
  entries.push_back(builtInEntry(3));  // Template course

  EXPECT_EQ(orderedNames(entries), (std::vector<std::string>{"Sanyang Golf Club", "MoganShan Gowin", "Pebble Beach",
                                                             "Template course", "Zzz Country Club"}));
}

TEST(GolfResolveAllTees, TwoSdTeeFilesForSameCourseNameResolveBoth) {
  constexpr uint16_t blueYards[18] = {380, 410, 190, 505, 340, 160, 420, 390, 530,
                                      400, 175, 460, 350, 415, 200, 380, 145, 500};
  constexpr uint16_t whiteYards[18] = {350, 390, 165, 480, 315, 140, 395, 365, 505,
                                       375, 150, 435, 325, 390, 175, 355, 120, 475};
  std::vector<Entry> entries;
  entries.push_back(sdTeeEntry("club-blue.json", "Owner Links", "Blue", blueYards));
  entries.push_back(sdTeeEntry("club-white.json", "Owner Links", "White", whiteYards));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Owner Links", result));
  EXPECT_TRUE(result.hasBlue);
  EXPECT_TRUE(result.hasWhite);
  EXPECT_TRUE(result.blue.hasYards);
  EXPECT_TRUE(result.white.hasYards);
  EXPECT_EQ(result.blue.yards[0], 380);
  EXPECT_EQ(result.white.yards[0], 350);
  EXPECT_STREQ(result.primaryFile.filename, "club-blue.json");
}

TEST(GolfResolveAllTees, OneFileResolvesOnlyThatTee) {
  std::vector<Entry> entries;
  entries.push_back(sdTeeEntry("club-blue.json", "Owner Links", "Blue"));
  entries.push_back(sdEntry("unrelated.json", "Some Other Course"));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Owner Links", result));
  EXPECT_TRUE(result.hasBlue);
  EXPECT_FALSE(result.hasWhite);
}

TEST(GolfResolveAllTees, BuiltInSanyangSpecialCaseResolvesBothTeesThroughThisPath) {
  std::vector<Entry> entries;
  entries.push_back(builtInEntry(SANYANG_BUILT_IN_INDEX));
  entries.push_back(builtInEntry(MOGANSHAN_BUILT_IN_INDEX));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Sanyang Golf Club", result));
  EXPECT_TRUE(result.hasBlue);
  EXPECT_TRUE(result.hasWhite);
  EXPECT_EQ(result.blue.yards[0], GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards[0]);
  EXPECT_EQ(result.white.yards[0], SANYANG_WHITE_YARDS[0]);
}

TEST(GolfResolveAllTees, NoMatchingFileReturnsFalse) {
  std::vector<Entry> entries;
  entries.push_back(sdEntry("unrelated.json", "Some Other Course"));

  GolfCourseTeeSet result{};
  EXPECT_FALSE(resolveAllTees(entries, "Nonexistent Course", result));
  EXPECT_FALSE(result.hasBlue);
  EXPECT_FALSE(result.hasWhite);
}
