#include <gtest/gtest.h>

#include <cstring>

#include "GolfValidate.h"

namespace {

class GolfValidateTest : public ::testing::Test {
 protected:
  GolfRound round{};

  void SetUp() override {
    initializeGolfPlayerDefaults(round);
    round.holeCount = 18;
    round.players[0].tee = TeeSelection::Blue;
    round.currentPlayer = 0;
  }

  GolfPlayerScore& score(const uint8_t player = 0) { return round.players[player].score; }
};

TEST_F(GolfValidateTest, ClearsPuttsOnUnenteredHole) {
  score().putts[2] = 2;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_EQ(score().putts[2], 0);
  EXPECT_TRUE(result.players[0].holePuttsRepaired(2));
}

TEST_F(GolfValidateTest, ReducesPuttsToInside100Floor) {
  score().putts[4] = 4;
  score().in100[4] = 2;
  score().out100[4] = 3;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_EQ(score().putts[4], 2);
  EXPECT_EQ(score().in100[4], 2);
  EXPECT_EQ(score().out100[4], 3);
  EXPECT_TRUE(result.players[0].holePuttsRepaired(4));
}

TEST_F(GolfValidateTest, AcceptsZeroOutside100OnEnteredHole) {
  score().in100[0] = 1;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.repaired());
}

TEST_F(GolfValidateTest, RejectsUnsupportedHoleCountWithoutScoreMutation) {
  round.holeCount = 12;
  round.currentHole = 15;
  score().out100[0] = 1;
  const GolfPlayerScore before = score();
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(round.currentHole, 15);
  EXPECT_EQ(memcmp(&score(), &before, sizeof(before)), 0);
}

TEST_F(GolfValidateTest, RejectsDraftWithNoEnabledPlayers) {
  round.players[0].tee = TeeSelection::NotPlay;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_FALSE(result.valid);
}

TEST_F(GolfValidateTest, RejectsUnknownTeeValue) {
  round.players[0].tee = static_cast<TeeSelection>(3);
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_FALSE(result.valid);
}

TEST_F(GolfValidateTest, ResetsOutOfRangeCurrentHole) {
  round.currentHole = 18;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.currentHoleReset);
  EXPECT_EQ(round.currentHole, 0);
}

TEST_F(GolfValidateTest, ResetsDisabledCurrentPlayerToFirstEnabledSlot) {
  round.players[0].tee = TeeSelection::NotPlay;
  round.players[2].tee = TeeSelection::White;
  round.currentPlayer = 0;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.currentPlayerReset);
  EXPECT_EQ(round.currentPlayer, 2);
}

TEST_F(GolfValidateTest, LeavesValidRoundUnchanged) {
  round.currentHole = 7;
  score().putts[0] = 2;
  score().in100[0] = 3;
  score().out100[0] = 2;
  const GolfPlayerScore before = score();
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_FALSE(result.repaired());
  EXPECT_EQ(memcmp(&score(), &before, sizeof(before)), 0);
  EXPECT_EQ(round.currentHole, 7);
  EXPECT_EQ(round.currentPlayer, 0);
}

TEST_F(GolfValidateTest, RepairsAndReportsCorruptPenaltyRecords) {
  score(2).out100[0] = 1;
  score(2).penaltyCount[0] = 10;
  score(2).penaltyEvents[0][0] = 0x32;
  score(2).penaltyEvents[0][1] = 0x22;

  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.valid);
  EXPECT_TRUE(result.repaired());
  EXPECT_TRUE(result.players[2].holePenaltyCountRepaired(0));
  EXPECT_TRUE(result.players[2].holePenaltyEventRepaired(0));
  EXPECT_TRUE(result.players[2].holePenaltyMarkerRepaired(0));
  EXPECT_FALSE(result.players[0].repaired());
  EXPECT_EQ(score(2).penaltyCount[0], 1);
  EXPECT_EQ(score(2).penaltyEvents[0][0] & 0x0f, 2);
}

TEST_F(GolfValidateTest, RemovesPenaltyOnlyHoleMarkers) {
  score().penaltyCount[0] = 1;
  score().penaltyEvents[0][0] = 2;
  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.players[0].holePenaltyMarkerRepaired(0));
  EXPECT_EQ(score().penaltyCount[0], 0);
}

TEST_F(GolfValidateTest, RepairsEachPlayersScoreIndependently) {
  score(0).putts[0] = 2;
  score(1).putts[0] = 3;
  score(1).in100[0] = 1;
  score(1).out100[0] = 1;

  const GolfValidationResult result = validateGolfRound(round);
  EXPECT_TRUE(result.players[0].holePuttsRepaired(0));
  EXPECT_TRUE(result.players[1].holePuttsRepaired(0));
  EXPECT_EQ(score(0).putts[0], 0);
  EXPECT_EQ(score(1).putts[0], 1);
}

}  // namespace
