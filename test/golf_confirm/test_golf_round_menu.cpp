#include <gtest/gtest.h>

#include "src/activities/golf/GolfRoundMenuRows.h"

TEST(GolfRoundMenuRows, MapIsSecondAndDestructiveActionsKeepTheirIdentity) {
  EXPECT_EQ(golfmenu::ROW_COUNT, 6);
  EXPECT_EQ(golfmenu::rowAt(0), golfmenu::VIEW_CARD);
  EXPECT_EQ(golfmenu::rowAt(1), golfmenu::HOLE_MAP);
  EXPECT_EQ(golfmenu::rowAt(2), golfmenu::ABANDON);
  EXPECT_EQ(golfmenu::rowAt(3), golfmenu::FINISH);
  EXPECT_EQ(golfmenu::rowAt(4), golfmenu::SEND);
  EXPECT_EQ(golfmenu::rowAt(5), golfmenu::TIPS);
}

TEST(GolfRoundMenuRows, InvalidRowsDoNotSelectAnAction) {
  for (int index : {-100, -1, 6, 100}) EXPECT_EQ(golfmenu::rowAt(index), golfmenu::ROW_COUNT);
}
