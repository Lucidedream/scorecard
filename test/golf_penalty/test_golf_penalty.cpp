#include <gtest/gtest.h>

#include <cstring>

#include "GolfPenalty.h"
#include "GolfRules.h"

namespace {

class GolfPenaltyTest : public ::testing::Test {
 protected:
  GolfPlayerScore scores[GolfRound::MAX_PLAYERS]{};

  void increment(GolfPlayerScore& score, const GolfField field, const uint8_t count) {
    for (uint8_t index = 0; index < count; ++index) ASSERT_TRUE(incrementGolfCounter(score, 0, field).changed);
  }
};

TEST_F(GolfPenaltyTest, NibbleRoundTripsEveryFieldAndKind) {
  for (const GolfField field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    for (const GolfPenaltyKind kind : {GolfPenaltyKind::Hazard, GolfPenaltyKind::Ob}) {
      const uint8_t packed = golfPackPenaltyEvent(field, kind);
      EXPECT_EQ(packed & 0x08, 0);
      GolfPenaltyEvent event{};
      ASSERT_TRUE(golfUnpackPenaltyEvent(packed, event));
      EXPECT_EQ(event.field, field);
      EXPECT_EQ(event.kind, kind);
    }
  }
  GolfPenaltyEvent invalid{};
  EXPECT_FALSE(golfUnpackPenaltyEvent(0x03, invalid));
  EXPECT_FALSE(golfUnpackPenaltyEvent(0x08, invalid));
}

TEST_F(GolfPenaltyTest, AppendThenRemoveRestoresExactPlayerScore) {
  for (const GolfField field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    GolfPlayerScore& score = scores[static_cast<uint8_t>(field)];
    score.putts[0] = 2;
    score.in100[0] = 2;
    score.out100[0] = 2;
    const GolfPlayerScore before = score;
    ASSERT_EQ(golfAppendPenalty(score, 0, field, GolfPenaltyKind::Hazard), GolfPenaltyMutationStatus::Changed);
    ASSERT_EQ(golfRemoveLatestPenalty(score, 0, field), GolfPenaltyMutationStatus::Changed);
    EXPECT_EQ(memcmp(&score, &before, sizeof(score)), 0);
  }
}

TEST_F(GolfPenaltyTest, PuttsPenaltyAddsAndRemovesTheContainingIn100Stroke) {
  GolfPlayerScore& score = scores[0];
  score.putts[0] = 1;
  score.in100[0] = 3;
  const GolfPlayerScore before = score;
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Putts, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(score.putts[0], 2);
  EXPECT_EQ(score.in100[0], 4);
  ASSERT_EQ(golfRemoveLatestPenalty(score, 0, GolfField::Putts), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(memcmp(&score, &before, sizeof(score)), 0);
}

TEST_F(GolfPenaltyTest, RemoveTakesMostRecentMarkerOnRequestedField) {
  GolfPlayerScore& score = scores[0];
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::In100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);

  ASSERT_EQ(golfRemoveLatestPenalty(score, 0, GolfField::Out100), GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(score.penaltyCount[0], 2);
  GolfPenaltyEvent first{};
  GolfPenaltyEvent second{};
  ASSERT_TRUE(golfPenaltyEventAt(score, 0, 0, first));
  ASSERT_TRUE(golfPenaltyEventAt(score, 0, 1, second));
  EXPECT_EQ(first.field, GolfField::Out100);
  EXPECT_EQ(first.kind, GolfPenaltyKind::Hazard);
  EXPECT_EQ(second.field, GolfField::In100);
  EXPECT_EQ(second.kind, GolfPenaltyKind::Ob);
  EXPECT_EQ(score.out100[0], 1);
  EXPECT_EQ(score.in100[0], 1);
}

TEST_F(GolfPenaltyTest, CapRefusesWithoutMutation) {
  GolfPlayerScore& score = scores[0];
  for (uint8_t index = 0; index < GolfRound::MAX_PENALTIES_PER_HOLE; ++index) {
    ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
              GolfPenaltyMutationStatus::Changed);
  }
  const GolfPlayerScore before = score;
  EXPECT_EQ(golfAppendPenalty(score, 0, GolfField::In100, GolfPenaltyKind::Ob),
            GolfPenaltyMutationStatus::HoleFull);
  EXPECT_EQ(memcmp(&score, &before, sizeof(score)), 0);
}

TEST_F(GolfPenaltyTest, StrokeTotalsCoverHazardObMixedAndCap) {
  ASSERT_EQ(golfAppendPenalty(scores[0], 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(scores[0], 0), 1);

  ASSERT_EQ(golfAppendPenalty(scores[1], 0, GolfField::Out100, GolfPenaltyKind::Ob),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(scores[1], 0), 2);

  ASSERT_EQ(golfAppendPenalty(scores[2], 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(scores[2], 0, GolfField::In100, GolfPenaltyKind::Ob),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(scores[2], 0), 3);
  EXPECT_EQ(golfPenaltyStrokesForRound(scores[2], 1), 3);

  for (uint8_t index = 0; index < GolfRound::MAX_PENALTIES_PER_HOLE; ++index) {
    ASSERT_EQ(golfAppendPenalty(scores[3], 0, GolfField::Out100,
                                index % 2 == 0 ? GolfPenaltyKind::Hazard : GolfPenaltyKind::Ob),
              GolfPenaltyMutationStatus::Changed);
  }
  EXPECT_EQ(golfPenaltyStrokesForHole(scores[3], 0), 12);
}

TEST_F(GolfPenaltyTest, PenaltyMutationCannotTouchAnotherPlayer) {
  scores[1].putts[0] = 4;
  scores[1].in100[0] = 5;
  const GolfPlayerScore untouched = scores[1];
  ASSERT_EQ(golfAppendPenalty(scores[0], 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(memcmp(&scores[1], &untouched, sizeof(untouched)), 0);
}

TEST_F(GolfPenaltyTest, WorkedParFourWaterScoresSix) {
  GolfPlayerScore& score = scores[0];
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  increment(score, GolfField::Out100, 2);
  increment(score, GolfField::Putts, 2);
  EXPECT_EQ(score.out100[0], 3);
  EXPECT_EQ(score.in100[0], 2);
  EXPECT_EQ(golfPenaltyStrokesForHole(score, 0), 1);
  EXPECT_EQ(static_cast<uint16_t>(score.in100[0] + score.out100[0] + golfPenaltyStrokesForHole(score, 0)), 6);
}

TEST_F(GolfPenaltyTest, WorkedParFourObScoresSeven) {
  GolfPlayerScore& score = scores[0];
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Ob),
            GolfPenaltyMutationStatus::Changed);
  increment(score, GolfField::Out100, 2);
  increment(score, GolfField::Putts, 2);
  EXPECT_EQ(static_cast<uint16_t>(score.in100[0] + score.out100[0] + golfPenaltyStrokesForHole(score, 0)), 7);
}

TEST_F(GolfPenaltyTest, PenaltyOnSeededUnloggedHoleKeepsPuttsAndIn100) {
  GolfPlayerScore& seeded = scores[0];
  ASSERT_TRUE(seedGolfHoleAtPar(seeded, 0, 5));
  ASSERT_EQ(golfAppendPenalty(seeded, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(seeded.putts[0], 2);
  EXPECT_EQ(seeded.in100[0], 2);
  EXPECT_EQ(seeded.out100[0], 4);
  EXPECT_EQ(golfPenaltyStrokesForHole(seeded, 0), 1);

  GolfPlayerScore& unseeded = scores[1];
  ASSERT_EQ(golfAppendPenalty(unseeded, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(unseeded.putts[0], 0);
  EXPECT_EQ(unseeded.in100[0], 0);
}

TEST_F(GolfPenaltyTest, RemovePenaltyFromSeededHoleRestoresPreview) {
  GolfPlayerScore& score = scores[0];
  ASSERT_TRUE(seedGolfHoleAtPar(score, 0, 5));
  const GolfPlayerScore afterSeed = score;
  ASSERT_EQ(golfAppendPenalty(score, 0, GolfField::Out100, GolfPenaltyKind::Ob),
            GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfRemoveLatestPenalty(score, 0, GolfField::Out100), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(memcmp(&score, &afterSeed, sizeof(score)), 0);
}

TEST_F(GolfPenaltyTest, SeedIsIdempotentSoRepeatMutationPathsAreSafe) {
  GolfPlayerScore& score = scores[0];
  ASSERT_TRUE(seedGolfHoleAtPar(score, 0, 4));
  const GolfPlayerScore afterSeed = score;
  EXPECT_FALSE(seedGolfHoleAtPar(score, 0, 4));
  EXPECT_EQ(memcmp(&score, &afterSeed, sizeof(score)), 0);
}

}  // namespace
