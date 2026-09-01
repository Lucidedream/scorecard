#include <gtest/gtest.h>

#include <cstring>

#include "GolfNavigation.h"
#include "GolfReviewFormat.h"

namespace {

constexpr char PLAYER_LABEL_FORMAT[] = "P%u %s";
constexpr char EVEN_TEXT[] = "E";
constexpr char POSITIVE_TO_PAR_FORMAT[] = "+%d";
constexpr char NEGATIVE_TO_PAR_FORMAT[] = "%d";
constexpr char ROUND_STATUS_FORMAT[] = "%u (%s)";

GolfRound makeRound() {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  round.holeCount = 18;
  round.players[0].tee = TeeSelection::Blue;
  round.players[1].tee = TeeSelection::White;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  return round;
}

TEST(GolfNavigation, FrontButtonsSwapButSideRowSemanticsRemainIndependent) {
  EXPECT_EQ(golfFrontNavDelta(false, true), -1);
  EXPECT_EQ(golfFrontNavDelta(false, false), 1);
  EXPECT_EQ(golfFrontNavDelta(true, true), 1);
  EXPECT_EQ(golfFrontNavDelta(true, false), -1);
}

TEST(GolfHomeEntryDecision, LiveCommitMarkerWinsOverOldUnmarkedStateAfterMarkerWriteFailure) {
  constexpr bool indexCommitSucceeded = true;
  constexpr bool markerSerializationSucceeded = false;
  constexpr bool oldUnmarkedStateRemains = indexCommitSucceeded && !markerSerializationSucceeded;
  constexpr bool liveArchiveMarker = indexCommitSucceeded;
  constexpr bool cleanupSucceeded = false;

  constexpr GolfHomeEntryDecision decision = golfDecideHomeEntry(
      liveArchiveMarker, cleanupSucceeded, oldUnmarkedStateRemains, false, 18);
  EXPECT_FALSE(decision.loadState);
  EXPECT_TRUE(decision.cleanupOnly);
  EXPECT_FALSE(decision.showResume);
  EXPECT_FALSE(decision.showNew);
  EXPECT_FALSE(decision.stateError);
}

TEST(GolfReviewFormat, PlayerLabelsLeadWithStableSlotAndPreserveDuplicateNames) {
  char first[GOLF_PLAYER_LABEL_CAPACITY]{};
  char third[GOLF_PLAYER_LABEL_CAPACITY]{};
  golfFormatPlayerLabel(0, "Alex", PLAYER_LABEL_FORMAT, first, sizeof(first));
  golfFormatPlayerLabel(2, "Alex", PLAYER_LABEL_FORMAT, third, sizeof(third));

  EXPECT_STREQ(first, "P1 Alex");
  EXPECT_STREQ(third, "P3 Alex");
}

TEST(GolfReviewFormat, PlayerLabelsUseTheProvidedLocalizedFormat) {
  char output[GOLF_PLAYER_LABEL_CAPACITY]{};
  golfFormatPlayerLabel(1, "Alex", "[%u] %s", output, sizeof(output));

  EXPECT_STREQ(output, "[2] Alex");
}

TEST(GolfReviewFormat, PlayerLabelTruncationDoesNotSplitUtf8) {
  char output[6]{};
  golfFormatPlayerLabel(0, "éé", PLAYER_LABEL_FORMAT, output, sizeof(output));

  EXPECT_STREQ(output, "P1 é");
}

TEST(GolfReviewFormat, EmptyOrAsciiWhitespaceOnlyNamesAreNotVisible) {
  EXPECT_FALSE(golfPlayerNameHasVisibleText(""));
  EXPECT_FALSE(golfPlayerNameHasVisibleText(" \t\r\n\f\v"));
  EXPECT_TRUE(golfPlayerNameHasVisibleText(" Alex "));
  EXPECT_TRUE(golfPlayerNameHasVisibleText("李"));
}

TEST(GolfReviewFormat, StatusUsesTheExplicitPlayerScore) {
  GolfRound round = makeRound();
  round.currentPlayer = 0;
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 2;
  round.players[1].score.in100[0] = 2;
  round.players[1].score.out100[0] = 4;

  char status[20]{};
  golfFormatRoundStatus(round, round.players[1].score, EVEN_TEXT, POSITIVE_TO_PAR_FORMAT,
                        NEGATIVE_TO_PAR_FORMAT, ROUND_STATUS_FORMAT, status, sizeof(status));

  EXPECT_STREQ(status, "6 (+2)");
}

TEST(GolfReviewFormat, ParFreeStatusSuppressesToPar) {
  GolfRound round = makeRound();
  memset(round.par, 0, sizeof(round.par));
  round.players[0].score.in100[0] = 2;
  round.players[0].score.out100[0] = 3;

  char status[20]{};
  golfFormatRoundStatus(round, round.players[0].score, EVEN_TEXT, POSITIVE_TO_PAR_FORMAT,
                        NEGATIVE_TO_PAR_FORMAT, ROUND_STATUS_FORMAT, status, sizeof(status));

  EXPECT_STREQ(status, "5");
}

}  // namespace
