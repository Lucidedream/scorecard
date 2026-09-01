#include <gtest/gtest.h>

#include "GolfRoundDecode.h"

namespace {

GolfRoundColumnLengths legacyLengths(const uint16_t count, const bool expectYards = false) {
  GolfRoundColumnLengths lengths{};
  lengths.par = count;
  lengths.expectLegacyYards = expectYards;
  lengths.player[0].yards = expectYards ? count : 0;
  lengths.player[0].putts = count;
  lengths.player[0].in100 = count;
  lengths.player[0].out100 = count;
  return lengths;
}

GolfRoundColumnLengths v4Lengths(const uint16_t count) {
  GolfRoundColumnLengths lengths{};
  lengths.par = count;
  lengths.si = count;
  lengths.players = GolfRound::MAX_PLAYERS;
  for (GolfPlayerColumnLengths& player : lengths.player) {
    player = {count, count, count, count, count};
  }
  return lengths;
}

class GolfRoundDecodeTest : public ::testing::Test {
 protected:
  GolfRound round{};
  GolfValidationResult validation{};

  void prepareLegacy(const char* tee = "Blue") {
    golfInitializeLegacyRound(round, tee);
    for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) round.par[hole] = 4;
  }

  void prepareV4() {
    round = {};
    initializeGolfPlayerDefaults(round);
    round.players[0].tee = TeeSelection::Blue;
    for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) round.par[hole] = 4;
  }
};

TEST_F(GolfRoundDecodeTest, RejectsVersionOneAndMissingVersion) {
  prepareLegacy();
  EXPECT_EQ(golfCheckRound(round, 1, 18, 0, 0, legacyLengths(18), validation),
            GolfRoundDecodeStatus::RejectedVersion);
  EXPECT_EQ(golfCheckRound(round, 0, 18, 0, 0, legacyLengths(18), validation),
            GolfRoundDecodeStatus::RejectedVersion);
}

TEST_F(GolfRoundDecodeTest, LegacyTeeMappingUsesExactTokensAndBlueFallback) {
  EXPECT_EQ(golfLegacyTeeSelection("White"), TeeSelection::White);
  EXPECT_EQ(golfLegacyTeeSelection("Blue"), TeeSelection::Blue);
  EXPECT_EQ(golfLegacyTeeSelection("white"), TeeSelection::Blue);
  EXPECT_EQ(golfLegacyTeeSelection("Championship"), TeeSelection::Blue);
  EXPECT_EQ(golfLegacyTeeSelection(nullptr), TeeSelection::Blue);

  TeeSelection parsed = TeeSelection::NotPlay;
  EXPECT_TRUE(golfParseTeeSelection("NotPlay", parsed));
  EXPECT_EQ(parsed, TeeSelection::NotPlay);
  EXPECT_TRUE(golfParseTeeSelection("White", parsed));
  EXPECT_EQ(parsed, TeeSelection::White);
  EXPECT_FALSE(golfParseTeeSelection("white", parsed));
  EXPECT_STREQ(golfTeeSelectionToken(TeeSelection::Blue), "Blue");
}

TEST_F(GolfRoundDecodeTest, VersionTwoMigratesIntoNoahSlotWithZeroPenalties) {
  prepareLegacy("White");
  round.players[0].score.putts[0] = 2;
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;

  EXPECT_EQ(golfCheckRound(round, 2, 18, 0, 0, legacyLengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_STREQ(round.players[0].name, "Noah");
  EXPECT_EQ(round.players[0].tee, TeeSelection::White);
  for (uint8_t slot = 1; slot < GolfRound::MAX_PLAYERS; ++slot) {
    EXPECT_EQ(round.players[slot].tee, TeeSelection::NotPlay);
  }
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) {
    EXPECT_EQ(round.players[0].score.penaltyCount[hole], 0);
  }
  EXPECT_FALSE(validation.repaired());
}

TEST_F(GolfRoundDecodeTest, VersionThreeRequiresOnePenaltyArrayPerHole) {
  prepareLegacy();
  GolfRoundColumnLengths valid = legacyLengths(18);
  valid.player[0].penalties = 18;
  EXPECT_EQ(golfCheckRound(round, 3, 18, 0, 0, valid, validation), GolfRoundDecodeStatus::Ok);
  valid.player[0].penalties = 17;
  EXPECT_EQ(golfCheckRound(round, 3, 18, 0, 0, valid, validation),
            GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST_F(GolfRoundDecodeTest, V4AcceptsExactlyFourOrderedPlayers) {
  prepareV4();
  round.players[2].tee = TeeSelection::White;
  round.players[2].score.in100[4] = 2;
  round.players[2].score.out100[4] = 3;
  round.currentPlayer = 2;

  EXPECT_EQ(golfCheckRound(round, 4, 18, 4, 2, v4Lengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(round.currentHole, 4);
  EXPECT_EQ(round.currentPlayer, 2);
  EXPECT_EQ(round.players[2].score.out100[4], 3);
}

TEST_F(GolfRoundDecodeTest, V4RejectsPlayerCountAndAnyPerPlayerLengthMismatch) {
  prepareV4();
  GolfRoundColumnLengths lengths = v4Lengths(18);
  lengths.players = 3;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, lengths, validation),
            GolfRoundDecodeStatus::RejectedPlayerCount);
  lengths = v4Lengths(18);
  lengths.player[3].yards = 17;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, lengths, validation),
            GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST_F(GolfRoundDecodeTest, V4RejectsRoundWithNoEnabledPlayers) {
  prepareV4();
  round.players[0].tee = TeeSelection::NotPlay;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation),
            GolfRoundDecodeStatus::RejectedRound);
}

TEST_F(GolfRoundDecodeTest, V4RejectsNonZeroPayloadForDisabledPlayer) {
  prepareV4();
  round.players[3].score.out100[0] = 1;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation),
            GolfRoundDecodeStatus::RejectedDisabledPlayerData);
}

TEST_F(GolfRoundDecodeTest, V4ValidatesSharedStrokeIndexSnapshot) {
  prepareV4();
  round.hasSi = true;
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) round.si[hole] = static_cast<uint8_t>(hole + 1);
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation), GolfRoundDecodeStatus::Ok);
  round.si[1] = round.si[0];
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation),
            GolfRoundDecodeStatus::RejectedSharedData);

  prepareV4();
  round.hasSi = false;
  round.si[0] = 7;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation),
            GolfRoundDecodeStatus::RejectedSharedData);
}

TEST_F(GolfRoundDecodeTest, V4RepairsDisabledCurrentPlayerToFirstEnabled) {
  prepareV4();
  round.players[0].tee = TeeSelection::NotPlay;
  round.players[2].tee = TeeSelection::White;
  EXPECT_EQ(golfCheckRound(round, 4, 18, 3, 0, v4Lengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_TRUE(validation.currentPlayerReset);
  EXPECT_EQ(round.currentPlayer, 2);
}

TEST_F(GolfRoundDecodeTest, RejectsUnsupportedHoleCountAndArrayMismatch) {
  prepareLegacy();
  EXPECT_EQ(golfCheckRound(round, 2, 13, 0, 0, legacyLengths(13), validation),
            GolfRoundDecodeStatus::RejectedHoleCount);
  GolfRoundColumnLengths mismatched = legacyLengths(18);
  mismatched.player[0].putts = 17;
  EXPECT_EQ(golfCheckRound(round, 2, 18, 0, 0, mismatched, validation),
            GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST_F(GolfRoundDecodeTest, RepairsAndReportsOnlyOwningPlayer) {
  prepareV4();
  round.players[1].tee = TeeSelection::White;
  round.players[1].score.putts[3] = 5;
  round.players[1].score.in100[3] = 2;
  round.players[1].score.out100[3] = 1;

  EXPECT_EQ(golfCheckRound(round, 4, 18, 0, 0, v4Lengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_TRUE(validation.players[1].holePuttsRepaired(3));
  EXPECT_FALSE(validation.players[0].repaired());
  EXPECT_EQ(round.players[1].score.putts[3], 2);
}

TEST_F(GolfRoundDecodeTest, LegacyYardsLengthIsRequiredOnlyForState) {
  prepareLegacy();
  GolfRoundColumnLengths archive = legacyLengths(18, false);
  archive.player[0].yards = 3;
  EXPECT_EQ(golfCheckRound(round, 2, 18, 0, 0, archive, validation), GolfRoundDecodeStatus::Ok);

  GolfRoundColumnLengths state = legacyLengths(18, true);
  state.player[0].yards = 3;
  EXPECT_EQ(golfCheckRound(round, 2, 18, 0, 0, state, validation),
            GolfRoundDecodeStatus::RejectedArrayLength);
}

}  // namespace
