#include <gtest/gtest.h>

#include <cstdio>
#include <initializer_list>
#include <memory>
#include <utility>

#include "GolfGlanceTees.h"

namespace {

std::unique_ptr<GolfCourse> makeSet(std::initializer_list<std::pair<const char*, bool>> tees) {
  auto storage = std::make_unique<GolfCourse>();
  auto& set = *storage;
  for (const auto& tee : tees) {
    GolfCourseTee& slot = set.tees[set.teeCount++];
    std::snprintf(slot.name, sizeof(slot.name), "%s", tee.first);
    slot.hasYards = tee.second;
  }
  return storage;
}

}  // namespace

TEST(PickGlanceTees, SingleTeeShowsOne) {
  auto storage = makeSet({{"White", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 1);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_EQ(out[1], nullptr);
}

TEST(PickGlanceTees, TwoTeesShowBoth) {
  auto storage = makeSet({{"White", true}, {"Yellow", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_STREQ(out[1]->name, "Yellow");
}

TEST(PickGlanceTees, TwoTeesSkipTheOneWithoutYards) {
  auto storage = makeSet({{"White", false}, {"Yellow", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 1);
  EXPECT_STREQ(out[0]->name, "Yellow");
  EXPECT_EQ(out[1], nullptr);
}

TEST(PickGlanceTees, FourTeesPreferBlueThenRed) {
  auto storage = makeSet({{"Black", true}, {"White", true}, {"Blue", true}, {"Red", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Blue");
  EXPECT_STREQ(out[1]->name, "Red");
}

TEST(PickGlanceTees, MoreThanTwoWithoutBlueOrRedShowsFirstTwo) {
  auto storage = makeSet({{"Black", true}, {"White", true}, {"Yellow", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Black");
  EXPECT_STREQ(out[1]->name, "White");
}

TEST(PickGlanceTees, MoreThanTwoSkipsTeeWithoutYardsWhenToppingUp) {
  auto storage = makeSet({{"Black", false}, {"White", true}, {"Yellow", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "White");
  EXPECT_STREQ(out[1]->name, "Yellow");
}

TEST(PickGlanceTees, BlueWithoutYardsIsNotPickedRedIsKeptThenToppedUp) {
  auto storage = makeSet({{"Black", true}, {"Blue", false}, {"Red", true}, {"White", true}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 2);
  EXPECT_STREQ(out[0]->name, "Red");
  EXPECT_STREQ(out[1]->name, "Black");
}

TEST(PickGlanceTees, MoreThanTwoAllWithoutYardsShowsNone) {
  auto storage = makeSet({{"Black", false}, {"White", false}, {"Yellow", false}});
  const auto& set = *storage;
  const GolfCourseTee* out[2];
  EXPECT_EQ(pickGlanceTees(set, out), 0);
  EXPECT_EQ(out[0], nullptr);
  EXPECT_EQ(out[1], nullptr);
}
