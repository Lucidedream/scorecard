// CONTRACTS-V2 §36: per-player round delete. Pure-helper coverage for the two
// GolfRound helpers that RoundArchive::removePlayer() composes -- the archive
// path itself has no host harness and stays device-verified like remove().

#include <gtest/gtest.h>

#include <cstring>

#include "GolfRound.h"

namespace {

// Marks one slot enabled with a distinct tee, per-hole yardage and score so a
// later zeroing is observable and cross-slot bleed would be caught.
void enablePlayer(GolfRound& round, const uint8_t slot, const char* tee, const uint8_t fill) {
  golfSetTee(round.players[slot], tee);
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    round.players[slot].yards[hole] = static_cast<uint16_t>(100 + fill + hole);
    round.players[slot].score.putts[hole] = fill;
    round.players[slot].score.in100[hole] = fill;
  }
}

bool scoreIsZero(const GolfPlayerScore& score) {
  const GolfPlayerScore zero{};
  return std::memcmp(&score, &zero, sizeof(zero)) == 0;
}

bool yardsAreZero(const GolfPlayer& player) {
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    if (player.yards[hole] != 0) return false;
  }
  return true;
}

}  // namespace

TEST(GolfDisablePlayer, MiddleSlotOfThreeIsCleared) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  enablePlayer(round, 0, "Blue", 3);
  enablePlayer(round, 1, "White", 5);
  enablePlayer(round, 2, "Red", 7);
  ASSERT_EQ(golfEnabledPlayerCount(round), 3);

  EXPECT_TRUE(golfDisablePlayer(round, 1));
  EXPECT_EQ(golfEnabledPlayerCount(round), 2);

  EXPECT_FALSE(golfPlayerIsEnabled(round.players[1]));
  EXPECT_TRUE(scoreIsZero(round.players[1].score));
  EXPECT_TRUE(yardsAreZero(round.players[1]));
  // The name is deliberately left untouched.
  EXPECT_STREQ(round.players[1].name, "Player 2");

  // Neighbours are untouched.
  EXPECT_TRUE(golfPlayerIsEnabled(round.players[0]));
  EXPECT_STREQ(round.players[0].tee, "Blue");
  EXPECT_EQ(round.players[0].yards[0], 103);
  EXPECT_EQ(round.players[0].score.putts[0], 3);

  EXPECT_TRUE(golfPlayerIsEnabled(round.players[2]));
  EXPECT_STREQ(round.players[2].tee, "Red");
  EXPECT_EQ(round.players[2].yards[0], 107);
  EXPECT_EQ(round.players[2].score.putts[0], 7);
}

TEST(GolfDisablePlayer, AlreadyDisabledSlotIsRejectedWithoutMutation) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  enablePlayer(round, 0, "Blue", 3);

  GolfRound before = round;
  EXPECT_FALSE(golfDisablePlayer(round, 2));  // slot 2 never enabled
  EXPECT_EQ(std::memcmp(&round, &before, sizeof(round)), 0);
  EXPECT_EQ(golfEnabledPlayerCount(round), 1);
}

TEST(GolfDisablePlayer, OutOfRangeSlotIsRejected) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  enablePlayer(round, 0, "Blue", 3);

  GolfRound before = round;
  EXPECT_FALSE(golfDisablePlayer(round, GolfRound::MAX_PLAYERS));
  EXPECT_FALSE(golfDisablePlayer(round, 200));
  EXPECT_EQ(std::memcmp(&round, &before, sizeof(round)), 0);
}

TEST(GolfEnabledPlayerCount, ZeroedRoundHasNoEnabledPlayers) {
  GolfRound round{};
  EXPECT_EQ(golfEnabledPlayerCount(round), 0);
}

TEST(GolfEnabledPlayerCount, SingleEnabledPlayerCountsOne) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  enablePlayer(round, 0, "Blue", 1);
  EXPECT_EQ(golfEnabledPlayerCount(round), 1);
}
