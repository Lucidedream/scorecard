#include <gtest/gtest.h>

#include <cstring>

#include "CourseBuiltIns.h"
#include "CourseOrder.h"
class CourseComposition : public ::testing::Test {
 protected:
  GolfCourse course = GOLF_BUILT_IN_COURSES[0];
  GolfRound round{};
};
TEST(GolfCourseOrder, BuiltinsAndOverridesKeepSlotsThenAlphabetical) {
  GolfCourseSummary rows[6]{};
  const char* names[] = {"zulu", "Template course", "alpha", "Sanyang Golf Club", "MoganShan Gowin", "Pebble Beach"};
  int8_t slots[] = {-1, 3, -1, 0, 1, 2};
  for (int i = 0; i < 6; ++i) {
    strcpy(rows[i].courseName, names[i]);
    rows[i].file.builtInIndex = slots[i];
  }
  strcpy(rows[3].file.filename, "override.json");
  golfSortCourses(rows, 6);
  EXPECT_STREQ(rows[0].file.filename, "override.json");
  for (int i = 0; i < 4; ++i) EXPECT_EQ(rows[i].file.builtInIndex, i);
  EXPECT_STREQ(rows[4].courseName, "alpha");
  EXPECT_STREQ(rows[5].courseName, "zulu");
}
TEST(GolfCourseOrder, NamesCompareWithoutCase) {
  EXPECT_EQ(golfCompareCourseNames("ALPHA", "alpha"), 0);
  EXPECT_LT(golfCompareCourseNames("alpha", "Beta"), 0);
}
TEST(GolfCourseOrder, YardageAcrossEveryNineWinsBeforeNamePriority) {
  GolfCourseTee a{}, b{};
  strcpy(a.name, "White");
  strcpy(b.name, "Black");
  a.hasYards = b.hasYards = true;
  a.yards[3][8] = 999;
  b.yards[0][0] = 998;
  EXPECT_TRUE(golfTeeSortsBefore(a, b));
  EXPECT_FALSE(golfTeeSortsBefore(b, a));
  b.yards[0][0] = 999;
  EXPECT_TRUE(golfTeeSortsBefore(b, a));
  b.hasYards = false;
  EXPECT_TRUE(golfTeeSortsBefore(a, b));
}
TEST(GolfCourseOrder, EqualYardlessUsesConventionalThenLexicalOrder) {
  GolfCourseTee a{}, b{};
  strcpy(a.name, "Gold");
  strcpy(b.name, "Blue");
  EXPECT_TRUE(golfTeeSortsBefore(a, b));
  strcpy(a.name, "Custom A");
  strcpy(b.name, "Custom B");
  EXPECT_TRUE(golfTeeSortsBefore(a, b));
  EXPECT_FALSE(golfTeeSortsBefore(a, a));
}
TEST_F(CourseComposition, DefaultSelectionAndRound) {
  auto s = golfDefaultNineSelection(course);
  EXPECT_EQ(s.front, 0);
  EXPECT_EQ(s.back, 1);
  ASSERT_TRUE(golfComposeRound(course, s, 123, round));
  EXPECT_EQ(round.holeCount, 18);
  EXPECT_EQ(round.dateYmd, 123);
  EXPECT_STREQ(round.courseName, course.courseName);
  EXPECT_STREQ(round.players[0].name, "Noah");
  EXPECT_TRUE(round.hasSi);
  EXPECT_EQ(round.par[9], course.nines[1].par[0]);
  EXPECT_EQ(golfCourseHoleNumber(round, 0), 1);
  EXPECT_EQ(golfCourseHoleNumber(round, 17), 18);
}
TEST_F(CourseComposition, SwappingKeepsCanonicalSiAndMapsImages) {
  ASSERT_TRUE(golfComposeRound(course, {1, 0}, 0, round));
  EXPECT_TRUE(round.hasSi);
  EXPECT_EQ(round.si[0], 6);
  EXPECT_EQ(round.si[9], 15);
  EXPECT_EQ(golfCourseHoleNumber(round, 0), 10);
  EXPECT_EQ(golfCourseHoleNumber(round, 9), 1);
}
TEST_F(CourseComposition, InvalidSelectionLeavesRoundUntouched) {
  round.dateYmd = 456;
  for (auto s : {GolfNineSelection{0, 0}, GolfNineSelection{2, 1}, GolfNineSelection{0, GOLF_NO_NINE}}) {
    EXPECT_FALSE(golfNineSelectionValid(course, s));
    EXPECT_FALSE(golfComposeRound(course, s, 0, round));
    EXPECT_EQ(round.dateYmd, 456);
  }
}
TEST_F(CourseComposition, ThreeNinesWithDuplicateSiDropsSi) {
  course.nineCount = 3;
  course.nines[2] = course.nines[0];
  strcpy(course.nines[2].name, "Third");
  ASSERT_TRUE(golfComposeRound(course, {0, 2}, 0, round));
  EXPECT_FALSE(round.hasSi);
  for (auto si : round.si) EXPECT_EQ(si, 0);
  EXPECT_EQ(golfCourseHoleNumber(round, 9), 19);
}
TEST_F(CourseComposition, OneNineHasNineHoleRoundAndCanonicalSi) {
  course.nineCount = 1;
  for (int h = 0; h < 9; ++h) course.nines[0].si[h] = h + 1;
  auto s = golfDefaultNineSelection(course);
  EXPECT_EQ(s.back, GOLF_NO_NINE);
  ASSERT_TRUE(golfComposeRound(course, s, 0, round));
  EXPECT_EQ(round.holeCount, 9);
  EXPECT_TRUE(round.hasSi);
  EXPECT_EQ(round.nines[1], GOLF_NO_NINE);
  EXPECT_FALSE(golfNineSelectionValid(course, {0, 1}));
}
TEST_F(CourseComposition, ComposeTeeYardsUsesSelectionAndZeroesUnusedHoles) {
  uint16_t yards[GOLF_MAX_HOLES]{};
  ASSERT_TRUE(golfComposeTeeYards(course, 1, {1, 0}, yards));
  EXPECT_EQ(yards[0], 360);
  EXPECT_EQ(yards[9], 310);
  course.nineCount = 1;
  ASSERT_TRUE(golfComposeTeeYards(course, 0, {0, GOLF_NO_NINE}, yards));
  EXPECT_EQ(yards[9], 0);
  course.tees[0].hasYards = false;
  EXPECT_FALSE(golfComposeTeeYards(course, 0, {0, GOLF_NO_NINE}, yards));
  EXPECT_FALSE(golfComposeTeeYards(course, 6, {0, GOLF_NO_NINE}, yards));
}
TEST_F(CourseComposition, DefaultTeeBlueThenWhiteThenFirst) {
  EXPECT_STREQ(golfDefaultTee(course), "Blue");
  strcpy(course.tees[0].name, "Black");
  EXPECT_STREQ(golfDefaultTee(course), "White");
  course.teeCount = 1;
  EXPECT_STREQ(golfDefaultTee(course), "Black");
  course.teeCount = 0;
  EXPECT_STREQ(golfDefaultTee(course), "");
}
TEST_F(CourseComposition, TemplateStaysParFreeAndYardless) {
  course = GOLF_BUILT_IN_COURSES[3];
  ASSERT_TRUE(golfComposeRound(course, {0, 1}, 0, round));
  for (auto p : round.par) EXPECT_EQ(p, 0);
  EXPECT_FALSE(round.hasSi);
  EXPECT_EQ(course.teeCount, 2);
}

TEST_F(CourseComposition, MissingNineFallsBackToRoundHoleNumber) {
  round.nines[0] = GOLF_NO_NINE;
  round.nines[1] = GOLF_NO_NINE;
  EXPECT_EQ(golfCourseHoleNumber(round, 0), 1);
  EXPECT_EQ(golfCourseHoleNumber(round, 8), 9);
  EXPECT_EQ(golfCourseHoleNumber(round, 9), 10);
  EXPECT_EQ(golfCourseHoleNumber(round, 17), 18);
}
