#include <gtest/gtest.h>

#include <numeric>

#include "CourseBuiltIns.h"
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

TEST(GolfBuiltInCourses, SanyangShipsVerifiedScorecardDataForBothTees) {
  const GolfCourse* sanyang = nullptr;
  int8_t sanyangIndex = -1;
  int8_t index = 0;
  for (const GolfCourse& course : GOLF_BUILT_IN_COURSES) {
    if (strcmp(course.courseName, "Sanyang Golf Club") == 0) {
      sanyang = &course;
      sanyangIndex = index;
      break;
    }
    ++index;
  }

  ASSERT_NE(sanyang, nullptr);
  EXPECT_EQ(sanyang->holeCount, 18);
  EXPECT_TRUE(sanyang->hasYards);
  EXPECT_TRUE(sanyang->hasSi);
  EXPECT_EQ(std::accumulate(sanyang->par, sanyang->par + 9, 0), 36);
  EXPECT_EQ(std::accumulate(sanyang->par + 9, sanyang->par + 18, 0), 36);
  EXPECT_EQ(std::accumulate(sanyang->par, sanyang->par + 18, 0), 72);
  EXPECT_EQ(std::accumulate(sanyang->yards, sanyang->yards + 9, 0), 3196);
  EXPECT_EQ(std::accumulate(sanyang->yards + 9, sanyang->yards + 18, 0), 3270);
  EXPECT_EQ(std::accumulate(sanyang->yards, sanyang->yards + 18, 0), 6466);

  bool seenStrokeIndexes[19]{};
  for (uint8_t hole = 0; hole < sanyang->holeCount; ++hole) {
    ASSERT_GE(sanyang->si[hole], 1);
    ASSERT_LE(sanyang->si[hole], 18);
    EXPECT_FALSE(seenStrokeIndexes[sanyang->si[hole]]);
    seenStrokeIndexes[sanyang->si[hole]] = true;
  }
  for (uint8_t strokeIndex = 1; strokeIndex <= 18; ++strokeIndex) EXPECT_TRUE(seenStrokeIndexes[strokeIndex]);

  EXPECT_TRUE(validateGolfCourse(*sanyang).valid);

  GolfCourse white = *sanyang;
  ASSERT_TRUE(applyBuiltInTeeYardages(sanyangIndex, "White", white));
  EXPECT_EQ(std::accumulate(white.par, white.par + 9, 0), 36);
  EXPECT_EQ(std::accumulate(white.par + 9, white.par + 18, 0), 36);
  EXPECT_EQ(std::accumulate(white.par, white.par + 18, 0), 72);
  EXPECT_EQ(std::accumulate(white.yards, white.yards + 9, 0), 2910);
  EXPECT_EQ(std::accumulate(white.yards + 9, white.yards + 18, 0), 3043);
  EXPECT_EQ(std::accumulate(white.yards, white.yards + 18, 0), 5953);
  EXPECT_TRUE(validateGolfCourse(white).valid);
}

TEST(GolfBuiltInCourses, MoganShanGowinShipsVerifiedBlueTeeCardWithoutStrokeIndexes) {
  const GolfCourse* moganshan = nullptr;
  int8_t moganshanIndex = -1;
  int8_t index = 0;
  for (const GolfCourse& course : GOLF_BUILT_IN_COURSES) {
    if (strcmp(course.courseName, "MoganShan Gowin") == 0) {
      moganshan = &course;
      moganshanIndex = index;
      break;
    }
    ++index;
  }

  ASSERT_NE(moganshan, nullptr);
  EXPECT_EQ(moganshanIndex, MOGANSHAN_BUILT_IN_INDEX);
  EXPECT_EQ(moganshanIndex, 1);  // second, immediately after Sanyang (CONTRACTS-V2 §7)
  EXPECT_EQ(moganshan->holeCount, 18);
  EXPECT_TRUE(moganshan->hasYards);
  EXPECT_FALSE(moganshan->hasSi);

  EXPECT_EQ(std::accumulate(moganshan->par, moganshan->par + 9, 0), 36);
  EXPECT_EQ(std::accumulate(moganshan->par + 9, moganshan->par + 18, 0), 36);
  EXPECT_EQ(std::accumulate(moganshan->par, moganshan->par + 18, 0), 72);
  EXPECT_EQ(std::accumulate(moganshan->yards, moganshan->yards + 9, 0), 3100);
  EXPECT_EQ(std::accumulate(moganshan->yards + 9, moganshan->yards + 18, 0), 3132);
  EXPECT_EQ(std::accumulate(moganshan->yards, moganshan->yards + 18, 0), 6232);

  // Stroke indexes were not supplied, so the array stays zeroed rather than guessed.
  for (uint8_t hole = 0; hole < moganshan->holeCount; ++hole) EXPECT_EQ(moganshan->si[hole], 0);

  EXPECT_TRUE(validateGolfCourse(*moganshan).valid);
}

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
  memset(round.putts, 4, sizeof(round.putts));
  memset(round.in100, 3, sizeof(round.in100));
  memset(round.out100, 9, sizeof(round.out100));

  CourseStore::applyGolfCourse(course, round, 0x1234);

  EXPECT_STREQ(round.courseName, "Pebble Beach");
  EXPECT_STREQ(round.tees, "Blue");
  EXPECT_EQ(round.dateYmd, 0x1234);
  EXPECT_EQ(round.holeCount, 9);
  EXPECT_EQ(round.currentHole, 0);
  EXPECT_EQ(round.par[0], 4);
  EXPECT_EQ(round.yards[0], 380);
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_EQ(round.putts[hole], 0);
    EXPECT_EQ(round.in100[hole], 0);
    EXPECT_EQ(round.out100[hole], 0);
  }
}

TEST(GolfCourseApply, PreservesParFreeTemplate) {
  GolfCourse course{};
  strcpy(course.courseName, "Template course");
  course.holeCount = 18;
  GolfRound round{};

  CourseStore::applyGolfCourse(course, round, 0);

  EXPECT_STREQ(round.courseName, "Template course");
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) EXPECT_EQ(round.par[hole], 0);
}
