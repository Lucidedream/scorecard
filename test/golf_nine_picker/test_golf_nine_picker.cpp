#include <gtest/gtest.h>

#include <memory>

#include "GolfNinePicker.h"

class NinePicker : public testing::Test {
 protected:
  std::unique_ptr<GolfCourse> course = std::make_unique<GolfCourse>();
  void SetUp() override { course->nineCount = 4; }
};

TEST_F(NinePicker, SummaryDefaultsMatchCourseDefaults) {
  GolfCourseSummary summary{};
  for (uint8_t count = 1; count <= GOLF_MAX_NINES; ++count) {
    summary.nineCount = course->nineCount = count;
    const auto actual = golfDefaultNineSelection(summary);
    const auto expected = golfDefaultNineSelection(*course);
    EXPECT_EQ(actual.front, expected.front);
    EXPECT_EQ(actual.back, expected.back);
  }
}

TEST_F(NinePicker, ChoosingEitherSlotSwapsOnCollision) {
  for (bool back : {false, true}) {
    GolfNineSelection selection{0, 2};
    ASSERT_TRUE(golfChooseNine(*course, selection, back, back ? 0 : 2));
    EXPECT_EQ(selection.front, 2);
    EXPECT_EQ(selection.back, 0);
    EXPECT_TRUE(golfNineSelectionValid(*course, selection));
  }
}

TEST_F(NinePicker, ChoosingUnoccupiedNineChangesOnlyChosenSlot) {
  GolfNineSelection selection{0, 1};
  ASSERT_TRUE(golfChooseNine(*course, selection, false, 3));
  EXPECT_EQ(selection.front, 3);
  EXPECT_EQ(selection.back, 1);
  ASSERT_TRUE(golfChooseNine(*course, selection, true, 2));
  EXPECT_EQ(selection.front, 3);
  EXPECT_EQ(selection.back, 2);
}

TEST_F(NinePicker, ChoosingCurrentNineDoesNotChangeSelection) {
  GolfNineSelection selection{2, 3};
  EXPECT_TRUE(golfChooseNine(*course, selection, false, 2));
  EXPECT_TRUE(golfChooseNine(*course, selection, true, 3));
  EXPECT_EQ(selection.front, 2);
  EXPECT_EQ(selection.back, 3);
}

TEST_F(NinePicker, InvalidChoiceOrSelectionLeavesSelectionUntouched) {
  GolfNineSelection selection{0, 1};
  EXPECT_FALSE(golfChooseNine(*course, selection, false, 4));
  EXPECT_EQ(selection.front, 0);
  EXPECT_EQ(selection.back, 1);
  selection = {2, 2};
  EXPECT_FALSE(golfChooseNine(*course, selection, true, 3));
  EXPECT_EQ(selection.front, 2);
  EXPECT_EQ(selection.back, 2);
  course->nineCount = 1;
  selection = {0, GOLF_NO_NINE};
  EXPECT_FALSE(golfChooseNine(*course, selection, true, 0));
  EXPECT_EQ(selection.front, 0);
  EXPECT_EQ(selection.back, GOLF_NO_NINE);
}

TEST_F(NinePicker, AllChoicesKeepDistinctValidNines) {
  for (uint8_t count = 2; count <= GOLF_MAX_NINES; ++count) {
    course->nineCount = count;
    for (uint8_t front = 0; front < count; ++front) {
      for (uint8_t back = 0; back < count; ++back) {
        if (front == back) continue;
        for (bool slot : {false, true}) {
          for (uint8_t nine = 0; nine < count; ++nine) {
            GolfNineSelection selection{front, back};
            ASSERT_TRUE(golfChooseNine(*course, selection, slot, nine));
            EXPECT_TRUE(golfNineSelectionValid(*course, selection));
            EXPECT_EQ(slot ? selection.back : selection.front, nine);
          }
        }
      }
    }
  }
}

TEST_F(NinePicker, SwapIsReversible) {
  GolfNineSelection selection{1, 3};
  ASSERT_TRUE(golfSwapNines(*course, selection));
  EXPECT_EQ(selection.front, 3);
  EXPECT_EQ(selection.back, 1);
  ASSERT_TRUE(golfSwapNines(*course, selection));
  EXPECT_EQ(selection.front, 1);
  EXPECT_EQ(selection.back, 3);
}

TEST_F(NinePicker, ParTotalUsesOnlyChosenNine) {
  for (auto& par : course->nines[2].par) par = 4;
  course->nines[2].par[0] = 5;
  EXPECT_EQ(golfNinePar(*course, 2), 37);
  EXPECT_EQ(golfNinePar(*course, 1), 0);
  EXPECT_EQ(golfNinePar(*course, GOLF_NO_NINE), 0);
}

TEST_F(NinePicker, YardageUsesFirstYardedTeeInCourseOrder) {
  course->teeCount = 3;
  course->tees[1].hasYards = course->tees[2].hasYards = true;
  for (auto& yards : course->tees[1].yards[2]) yards = 300;
  for (auto& yards : course->tees[2].yards[2]) yards = 400;
  uint32_t total = 0;
  ASSERT_TRUE(golfNineLongestTeeYards(*course, 2, total));
  EXPECT_EQ(total, 2700);
  EXPECT_FALSE(golfNineLongestTeeYards(*course, GOLF_NO_NINE, total));
  EXPECT_EQ(total, 0);
  course->tees[1].hasYards = false;
  ASSERT_TRUE(golfNineLongestTeeYards(*course, 2, total));
  EXPECT_EQ(total, 3600);
  for (auto& yards : course->tees[2].yards[2]) yards = UINT16_MAX;
  ASSERT_TRUE(golfNineLongestTeeYards(*course, 2, total));
  EXPECT_EQ(total, 9u * UINT16_MAX);
}

TEST_F(NinePicker, YardlessCourseOmitsYardage) {
  course->teeCount = 2;
  uint32_t total = 99;
  EXPECT_FALSE(golfNineLongestTeeYards(*course, 0, total));
  EXPECT_EQ(total, 0);
}

TEST_F(NinePicker, SwapRejectsInvalidSelectionAndSingleNine) {
  GolfNineSelection selection{1, 1};
  EXPECT_FALSE(golfSwapNines(*course, selection));
  EXPECT_EQ(selection.front, 1);
  EXPECT_EQ(selection.back, 1);
  course->nineCount = 1;
  selection = golfDefaultNineSelection(*course);
  EXPECT_FALSE(golfSwapNines(*course, selection));
  EXPECT_EQ(selection.front, 0);
  EXPECT_EQ(selection.back, GOLF_NO_NINE);
}
