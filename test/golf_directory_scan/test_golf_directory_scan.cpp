#include <gtest/gtest.h>

#include "GolfDirectoryScan.h"

namespace {

TEST(GolfDirectoryScan, RejectsAppleDoubleSidecarFilenames) {
  EXPECT_TRUE(golfIsHiddenSidecarFilename("._Slope Strategy.txt"));
  EXPECT_TRUE(golfIsHiddenSidecarFilename("._SomeCourse.json"));
  EXPECT_TRUE(golfIsHiddenSidecarFilename("._round-0001-20260101.json"));
  EXPECT_TRUE(golfIsHiddenSidecarFilename(".DS_Store"));
}

TEST(GolfDirectoryScan, AcceptsOrdinaryFilenames) {
  EXPECT_FALSE(golfIsHiddenSidecarFilename("Slope Strategy.txt"));
  EXPECT_FALSE(golfIsHiddenSidecarFilename("SomeCourse.json"));
  EXPECT_FALSE(golfIsHiddenSidecarFilename("round-0001-20260101.json"));
}

TEST(GolfDirectoryScan, HandlesEmptyAndNullFilenames) {
  EXPECT_FALSE(golfIsHiddenSidecarFilename(""));
  EXPECT_FALSE(golfIsHiddenSidecarFilename(nullptr));
}

}  // namespace
