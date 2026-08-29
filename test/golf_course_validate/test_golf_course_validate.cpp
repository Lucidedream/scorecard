#include <gtest/gtest.h>

#include <numeric>

#include "CourseStore.h"
#include "GolfCourseValidate.h"

namespace {

GolfCourse pebbleBeach() {
  GolfCourse course{};
  const uint8_t par[] = {4, 5, 4, 4, 3, 5, 3, 4, 4, 4, 4, 3, 4, 5, 4, 4, 3, 5};
  course.holeCount = 18;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    course.par[hole] = par[hole];
  }
  return course;
}

GolfCourse nineHoleCourse() {
  GolfCourse course{};
  course.holeCount = 9;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    course.par[hole] = 4;
  }
  return course;
}

}  // namespace

TEST(GolfCourseValidate, RealPebbleBeachFixtureValidatesWithoutOptionalArrays) {
  const GolfCourse course = pebbleBeach();
  const GolfCourseValidationResult result = validateGolfCourse(course, {18, 0, 0});

  ASSERT_TRUE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::None);
  EXPECT_FALSE(course.hasYards);
  EXPECT_FALSE(course.hasSi);
  EXPECT_EQ(std::accumulate(course.par, course.par + 9, 0), 36);
  EXPECT_EQ(std::accumulate(course.par + 9, course.par + 18, 0), 36);
}

TEST(GolfCourseValidate, NineHoleCourseRequiresZeroTrailingEntries) {
  GolfCourse course = nineHoleCourse();
  EXPECT_TRUE(validateGolfCourse(course).valid);

  course.par[9] = 4;
  const GolfCourseValidationResult result = validateGolfCourse(course);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::TrailingEntry);
  EXPECT_EQ(result.field, GolfCourseField::Par);
  EXPECT_EQ(result.hole, 9);
}

TEST(GolfCourseValidate, RejectsParBelowThreeAndAboveSix) {
  GolfCourse course = nineHoleCourse();
  course.par[2] = 2;
  EXPECT_EQ(validateGolfCourse(course).error, GolfCourseValidationError::ParRange);
  course.par[2] = 7;
  EXPECT_EQ(validateGolfCourse(course).error, GolfCourseValidationError::ParRange);
}

TEST(GolfCourseValidate, RejectsDuplicateStrokeIndex) {
  GolfCourse course = nineHoleCourse();
  course.hasSi = true;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    course.si[hole] = hole + 1;
  }
  course.si[8] = 8;

  const GolfCourseValidationResult result = validateGolfCourse(course);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::StrokeIndexDuplicate);
  EXPECT_EQ(result.value, 8);
}

TEST(GolfCourseValidate, RejectsMissingStrokeIndex) {
  GolfCourse course = nineHoleCourse();
  course.hasSi = true;
  for (uint8_t hole = 0; hole < course.holeCount - 1; ++hole) {
    course.si[hole] = hole + 1;
  }

  const GolfCourseValidationResult result = validateGolfCourse(course);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::StrokeIndexRange);
  EXPECT_EQ(result.value, 0);
}

TEST(GolfCourseValidate, RejectsArrayLengthDisagreeingWithHoles) {
  const GolfCourse course = nineHoleCourse();
  const GolfCourseValidationResult result = validateGolfCourse(course, {8, 0, 0});

  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::ArrayLength);
  EXPECT_EQ(result.field, GolfCourseField::Par);
  EXPECT_EQ(result.value, 8);
  EXPECT_EQ(result.expected, 9);
}

TEST(GolfCourseValidate, ValidCourseReportsNoFalsePositive) {
  GolfCourse course = nineHoleCourse();
  course.hasYards = true;
  course.hasSi = true;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    course.yards[hole] = 100 + hole;
    course.si[hole] = hole + 1;
  }

  const GolfCourseValidationResult result = validateGolfCourse(course);
  EXPECT_TRUE(result.valid);
  EXPECT_EQ(result.error, GolfCourseValidationError::None);
  EXPECT_EQ(result.field, GolfCourseField::None);
}

TEST(GolfCourseValidate, RejectsYardagesOutsideOneThrough999) {
  GolfCourse course = nineHoleCourse();
  course.hasYards = true;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    course.yards[hole] = 100;
  }
  course.yards[0] = 0;
  EXPECT_EQ(validateGolfCourse(course).error, GolfCourseValidationError::YardageRange);
  course.yards[0] = 1000;
  EXPECT_EQ(validateGolfCourse(course).error, GolfCourseValidationError::YardageRange);
}

TEST(GolfCourseValidate, RejectsUnsupportedHoleCount) {
  GolfCourse course{};
  course.holeCount = 12;
  EXPECT_EQ(validateGolfCourse(course).error, GolfCourseValidationError::HoleCount);
}

TEST(GolfCourseApply, SeedsRoundAndClearsAllCounters) {
  GolfCourse course = nineHoleCourse();
  strcpy(course.courseName, "Pebble Beach");
  strcpy(course.tees, "Blue");
  course.yards[0] = 380;
  course.si[0] = 7;
  GolfRound round{};
  memset(round.strokes, 9, sizeof(round.strokes));
  memset(round.putts, 4, sizeof(round.putts));
  memset(round.in100, 3, sizeof(round.in100));

  CourseStore::applyGolfCourse(course, round, 0x1234);

  EXPECT_STREQ(round.courseName, "Pebble Beach");
  EXPECT_STREQ(round.tees, "Blue");
  EXPECT_EQ(round.dateYmd, 0x1234);
  EXPECT_EQ(round.holeCount, 9);
  EXPECT_EQ(round.currentHole, 0);
  EXPECT_EQ(round.par[0], 4);
  EXPECT_EQ(round.yards[0], 380);
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_EQ(round.strokes[hole], 0);
    EXPECT_EQ(round.putts[hole], 0);
    EXPECT_EQ(round.in100[hole], 0);
  }
}
