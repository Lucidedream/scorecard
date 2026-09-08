#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>
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

std::vector<std::string> teeNames(const GolfCourseTeeSet& set) {
  std::vector<std::string> names;
  for (uint8_t i = 0; i < set.teeCount; ++i) names.emplace_back(set.tees[i].name);
  return names;
}

const GolfCourseTee* findTee(const GolfCourseTeeSet& set, const char* name) {
  for (uint8_t i = 0; i < set.teeCount; ++i) {
    if (std::strcmp(set.tees[i].name, name) == 0) return &set.tees[i];
  }
  return nullptr;
}

// A tee set built straight from a list of names, no SD/resolution involved.
GolfCourseTeeSet teeSetOf(std::initializer_list<const char*> names) {
  GolfCourseTeeSet set{};
  for (const char* name : names) {
    std::snprintf(set.tees[set.teeCount].name, sizeof(set.tees[set.teeCount].name), "%s", name);
    ++set.teeCount;
  }
  return set;
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

// Runs the shared load body (CONTRACTS-V2 §34.4) behind both the new-round course picker
// and the read-only course browser: sort + collapse adjacent same-name runs.
struct DedupedList {
  std::vector<GolfCourseFile> files;
  std::vector<GolfCourse> courses;
  uint8_t primaryIndex[GOLF_MAX_COURSES]{};
  uint8_t rowCount = 0;

  std::vector<std::string> rowNames() const {
    std::vector<std::string> names;
    for (uint8_t row = 0; row < rowCount; ++row) names.emplace_back(courses[primaryIndex[row]].courseName);
    return names;
  }
};

DedupedList sortAndDedup(const std::vector<Entry>& entries) {
  DedupedList out;
  for (const Entry& entry : entries) {
    out.files.push_back(entry.file);
    out.courses.push_back(entry.course);
  }
  out.rowCount = golfSortAndDedupCourses(out.files.data(), out.courses.data(), static_cast<uint8_t>(out.files.size()),
                                         out.primaryIndex);
  return out;
}

TEST(GolfCourseOrder, SanyangFourTeeFilesCollapseToOneRowKeyedOnFirstFile) {
  // Four Sanyang tee files (Black/Blue/White/Red, all "Sanyang Golf Club") plus two
  // unrelated SD courses collapse to three deduped rows.
  const std::vector<Entry> entries{
      sdTeeEntry("sanyang-black.json", "Sanyang Golf Club", "Black"),
      sdTeeEntry("sanyang-blue.json", "Sanyang Golf Club", "Blue"),
      sdTeeEntry("sanyang-white.json", "Sanyang Golf Club", "White"),
      sdTeeEntry("sanyang-red.json", "Sanyang Golf Club", "Red"),
      sdEntry("moganshan.json", "MoganShan Golf Club"),
      sdEntry("pebble.json", "Pebble Beach Golf Links"),
  };

  const DedupedList result = sortAndDedup(entries);

  ASSERT_EQ(result.rowCount, 3);
  EXPECT_EQ(result.rowNames(),
            (std::vector<std::string>{"MoganShan Golf Club", "Pebble Beach Golf Links", "Sanyang Golf Club"}));
  // The Sanyang row's primary is the first Sanyang file in enumerate order (Black); its
  // par/si/holeCount are what a started round or map browse inherits.
  EXPECT_STREQ(result.files[result.primaryIndex[2]].filename, "sanyang-black.json");
  // Every Sanyang tee file is still present in the scratch arrays for tee resolution.
  int sanyangEntries = 0;
  for (const GolfCourse& course : result.courses) {
    if (std::strcmp(course.courseName, "Sanyang Golf Club") == 0) ++sanyangEntries;
  }
  EXPECT_EQ(sanyangEntries, 4);
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
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Blue", "White"}));
  const GolfCourseTee* blue = findTee(result, "Blue");
  const GolfCourseTee* white = findTee(result, "White");
  ASSERT_NE(blue, nullptr);
  ASSERT_NE(white, nullptr);
  EXPECT_TRUE(blue->resolved);
  EXPECT_TRUE(white->resolved);
  EXPECT_TRUE(blue->hasYards);
  EXPECT_TRUE(white->hasYards);
  EXPECT_EQ(blue->yards[0], 380);
  EXPECT_EQ(white->yards[0], 350);
  EXPECT_STREQ(result.primaryFile.filename, "club-blue.json");
}

TEST(GolfResolveAllTees, OneFileResolvesOnlyThatTee) {
  std::vector<Entry> entries;
  entries.push_back(sdTeeEntry("club-blue.json", "Owner Links", "Blue"));
  entries.push_back(sdEntry("unrelated.json", "Some Other Course"));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Owner Links", result));
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Blue"}));
}

TEST(GolfResolveAllTees, SanyangFourSdTeeFilesGatherInFileOrderWithYardages) {
  constexpr uint16_t blackYards[18] = {347, 530, 195, 328, 540, 196, 460, 450, 420,
                                       430, 462, 545, 205, 400, 415, 410, 198, 518};
  constexpr uint16_t blueYards[18] = {325, 510, 144, 290, 510, 170, 427, 430, 390,
                                      400, 395, 520, 175, 365, 375, 385, 165, 490};
  constexpr uint16_t whiteYards[18] = {310, 470, 122, 265, 490, 153, 389, 371, 340,
                                       360, 360, 495, 150, 332, 350, 370, 156, 470};
  constexpr uint16_t redYards[18] = {233, 441, 100, 200, 392, 98,  312, 322, 320,
                                     305, 320, 435, 115, 305, 310, 320, 120, 441};
  std::vector<Entry> entries;
  entries.push_back(sdTeeEntry("sanyang-black.json", "Sanyang Golf Club", "Black", blackYards));
  entries.push_back(sdTeeEntry("sanyang-blue.json", "Sanyang Golf Club", "Blue", blueYards));
  entries.push_back(sdTeeEntry("sanyang-white.json", "Sanyang Golf Club", "White", whiteYards));
  entries.push_back(sdTeeEntry("sanyang-red.json", "Sanyang Golf Club", "Red", redYards));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Sanyang Golf Club", result));
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Black", "Blue", "White", "Red"}));
  EXPECT_EQ(findTee(result, "Black")->yards[0], 347);
  EXPECT_EQ(findTee(result, "Blue")->yards[0], 325);
  EXPECT_EQ(findTee(result, "White")->yards[0], 310);
  EXPECT_EQ(findTee(result, "Red")->yards[0], 233);
  for (uint8_t i = 0; i < result.teeCount; ++i) {
    EXPECT_TRUE(result.tees[i].resolved);
    EXPECT_TRUE(result.tees[i].hasYards);
  }
  EXPECT_STREQ(result.primaryFile.filename, "sanyang-black.json");
}

TEST(GolfResolveAllTees, TeeListIsCappedAtGolfMaxTees) {
  std::vector<Entry> entries;
  const char* names[7] = {"T1", "T2", "T3", "T4", "T5", "T6", "T7"};
  for (int i = 0; i < 7; ++i) {
    char filename[32];
    std::snprintf(filename, sizeof(filename), "many-%d.json", i);
    entries.push_back(sdTeeEntry(filename, "Many Tees", names[i]));
  }

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Many Tees", result));
  EXPECT_EQ(result.teeCount, GOLF_MAX_TEES);
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"T1", "T2", "T3", "T4", "T5", "T6"}));
}

TEST(GolfResolveAllTees, BuiltInSanyangYieldsBluePlusWhiteFromFlash) {
  std::vector<Entry> entries;
  entries.push_back(builtInEntry(SANYANG_BUILT_IN_INDEX));
  entries.push_back(builtInEntry(MOGANSHAN_BUILT_IN_INDEX));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Sanyang Golf Club", result));
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Blue", "White"}));
  EXPECT_EQ(findTee(result, "Blue")->yards[0], GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards[0]);
  EXPECT_EQ(findTee(result, "White")->yards[0], SANYANG_WHITE_YARDS[0]);
}

TEST(GolfResolveAllTees, BuiltInMoganShanYieldsOnlyBlue) {
  std::vector<Entry> entries;
  entries.push_back(builtInEntry(MOGANSHAN_BUILT_IN_INDEX));

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "MoganShan Gowin", result));
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Blue"}));
  EXPECT_TRUE(findTee(result, "Blue")->hasYards);
}

TEST(GolfResolveAllTees, LabelLessCourseKeepsSelectionOnlyBlueWhite) {
  std::vector<Entry> entries;
  entries.push_back(builtInEntry(3));  // Template course: no tee label, no yardage

  GolfCourseTeeSet result{};
  ASSERT_TRUE(resolveAllTees(entries, "Template course", result));
  EXPECT_EQ(teeNames(result), (std::vector<std::string>{"Blue", "White"}));
  EXPECT_TRUE(findTee(result, "Blue")->resolved);
  EXPECT_FALSE(findTee(result, "Blue")->hasYards);
}

TEST(GolfResolveAllTees, NoMatchingFileReturnsFalse) {
  std::vector<Entry> entries;
  entries.push_back(sdEntry("unrelated.json", "Some Other Course"));

  GolfCourseTeeSet result{};
  EXPECT_FALSE(resolveAllTees(entries, "Nonexistent Course", result));
  EXPECT_EQ(result.teeCount, 0);
}

// CONTRACTS-V2 §32.7: the tee a fresh round pre-selects for player 1.
TEST(GolfDefaultTeeForSet, PrefersBlueOverEverythingElse) {
  EXPECT_STREQ(golfDefaultTeeForSet(teeSetOf({"Black", "Blue", "White", "Red"})), "Blue");
}

TEST(GolfDefaultTeeForSet, FallsBackToWhiteWhenNoBlue) {
  EXPECT_STREQ(golfDefaultTeeForSet(teeSetOf({"Black", "White", "Red"})), "White");
}

TEST(GolfDefaultTeeForSet, FallsBackToFirstTeeWhenNeitherBlueNorWhite) {
  EXPECT_STREQ(golfDefaultTeeForSet(teeSetOf({"Black", "Red"})), "Black");
}

TEST(GolfDefaultTeeForSet, SingleBlueTeeSet) { EXPECT_STREQ(golfDefaultTeeForSet(teeSetOf({"Blue"})), "Blue"); }

TEST(GolfDefaultTeeForSet, EmptySetYieldsEmptyString) { EXPECT_STREQ(golfDefaultTeeForSet(GolfCourseTeeSet{}), ""); }

TEST(GolfDefaultTeeForSet, FindsBlueEvenWhenNotFirst) {
  EXPECT_STREQ(golfDefaultTeeForSet(teeSetOf({"Black", "White", "Blue", "Red"})), "Blue");
}
