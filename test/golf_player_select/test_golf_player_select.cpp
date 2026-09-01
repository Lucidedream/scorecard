#include <gtest/gtest.h>

#include "GolfPlayerSelectPolicy.h"

TEST(GolfPlayerSelectPolicy, KeepsStableSlotBits) {
  constexpr uint8_t present = static_cast<uint8_t>((1U << 0) | (1U << 2));

  EXPECT_TRUE(golfPlayerSelectSlotPresent(present, 0));
  EXPECT_FALSE(golfPlayerSelectSlotPresent(present, 1));
  EXPECT_TRUE(golfPlayerSelectSlotPresent(present, 2));
  EXPECT_FALSE(golfPlayerSelectSlotPresent(present, 3));
}

TEST(GolfPlayerSelectPolicy, FirstPresentDoesNotCompactSparseSlots) {
  EXPECT_EQ(golfPlayerSelectFirstPresent(static_cast<uint8_t>(1U << 2)), 2);
  EXPECT_EQ(golfPlayerSelectFirstPresent(static_cast<uint8_t>(1U << 3)), 3);
}

TEST(GolfPlayerSelectPolicy, NextAndPreviousSkipDisabledRowsAndWrap) {
  constexpr uint8_t present = static_cast<uint8_t>((1U << 0) | (1U << 2));

  EXPECT_EQ(golfPlayerSelectNextPresent(present, 0, 1), 2);
  EXPECT_EQ(golfPlayerSelectNextPresent(present, 2, 1), 0);
  EXPECT_EQ(golfPlayerSelectNextPresent(present, 0, -1), 2);
  EXPECT_EQ(golfPlayerSelectNextPresent(present, 2, -1), 0);
}

TEST(GolfPlayerSelectPolicy, AllAbsentRemainsStableAndBackable) {
  EXPECT_EQ(golfPlayerSelectFirstPresent(0), 0);
  EXPECT_EQ(golfPlayerSelectNextPresent(0, 3, 1), 3);
  EXPECT_EQ(golfPlayerSelectNextPresent(0, 3, -1), 3);
  EXPECT_EQ(golfPlayerSelectNextPresent(0, 99, 1), 0);
}
