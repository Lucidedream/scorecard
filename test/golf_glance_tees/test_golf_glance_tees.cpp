#include <gtest/gtest.h>

#include <cstdio>
#include <initializer_list>
#include <utility>

#include "GolfGlanceTees.h"

namespace {

GolfCourseTeeSet makeSet(std::initializer_list<std::pair<const char*, bool>> tees) {
  GolfCourseTeeSet set{};
  for (const auto& tee : tees) {
    GolfCourseTee& slot = set.tees[set.teeCount++];
    std::snprintf(slot.name, sizeof(slot.name), "%s", tee.first);
    slot.resolved = true;
    slot.hasYards = tee.second;
  }
  return set;
}

}  // namespace

TEST(PickGlanceTees, SingleTeeShowsOne) {
  const GolfCourseTeeSet set = makeSet({{"White", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 1);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_EQ(out[1], nullptr);
}

TEST(PickGlanceTees, TwoTeesShowBoth) {
  const GolfCourseTeeSet set = makeSet({{"White", true}, {"Yellow", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_STREQ(out[1]->name, "Yellow");
}

TEST(PickGlanceTees, TwoTeesSkipTheOneWithoutYards) {
  const GolfCourseTeeSet set = makeSet({{"White", false}, {"Yellow", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 1);
  EXPECT_STREQ(out[0]->name, "Yellow");
  EXPECT_EQ(out[1], nullptr);
}

TEST(PickGlanceTees, FourTeesPreferBlueThenRed) {
  const GolfCourseTeeSet set = makeSet({{"Black", true}, {"White", true}, {"Blue", true}, {"Red", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Blue");
  EXPECT_STREQ(out[1]->name, "Red");
}

TEST(PickGlanceTees, MoreThanTwoWithoutBlueOrRedShowsFirstTwo) {
  const GolfCourseTeeSet set = makeSet({{"Black", true}, {"White", true}, {"Yellow", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Black");
  EXPECT_STREQ(out[1]->name, "White");
}

TEST(PickGlanceTees, MoreThanTwoSkipsTeeWithoutYardsWhenToppingUp) {
  const GolfCourseTeeSet set = makeSet({{"Black", false}, {"White", true}, {"Yellow", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_STREQ(out[1]->name, "Yellow");
}

TEST(PickGlanceTees, BlueWithoutYardsIsNotPickedRedIsKeptThenToppedUp) {
  const GolfCourseTeeSet set = makeSet({{"Black", true}, {"Blue", false}, {"Red", true}, {"White", true}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Red");
  EXPECT_STREQ(out[1]->name, "Black");
}

TEST(PickGlanceTees, MoreThanTwoAllWithoutYardsShowsNone) {
  const GolfCourseTeeSet set = makeSet({{"Black", false}, {"White", false}, {"Yellow", false}});
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 0);
  EXPECT_EQ(out[0], nullptr);
  EXPECT_EQ(out[1], nullptr);
}
