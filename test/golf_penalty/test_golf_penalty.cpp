#include <gtest/gtest.h>

#include <cstring>

#include "GolfPenalty.h"
#include "GolfRules.h"

namespace {

GolfRound oneHole() {
  GolfRound round{};
  round.holeCount = 1;
  round.par[0] = 4;
  return round;
}

void increment(GolfRound& round, const GolfField field, const uint8_t count) {
  for (uint8_t index = 0; index < count; ++index) ASSERT_TRUE(incrementGolfCounter(round, 0, field).changed);
}

}  // namespace

TEST(GolfPenalty, NibbleRoundTripsEveryFieldAndKind) {
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

TEST(GolfPenalty, AppendThenRemoveRestoresExactRound) {
  for (const GolfField field : {GolfField::Putts, GolfField::In100, GolfField::Out100}) {
    GolfRound round = oneHole();
    round.putts[0] = 2;
    round.in100[0] = 2;
    round.out100[0] = 2;
    const GolfRound before = round;
    ASSERT_EQ(golfAppendPenalty(round, 0, field, GolfPenaltyKind::Hazard), GolfPenaltyMutationStatus::Changed);
    ASSERT_EQ(golfRemoveLatestPenalty(round, 0, field), GolfPenaltyMutationStatus::Changed);
    EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
  }
}

TEST(GolfPenalty, PuttsPenaltyAddsAndRemovesTheContainingIn100Stroke) {
  GolfRound round = oneHole();
  round.putts[0] = 1;
  round.in100[0] = 3;
  const GolfRound before = round;
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Putts, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(round.putts[0], 2);
  EXPECT_EQ(round.in100[0], 4);
  ASSERT_EQ(golfRemoveLatestPenalty(round, 0, GolfField::Putts), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
}

TEST(GolfPenalty, RemoveTakesMostRecentMarkerOnRequestedField) {
  GolfRound round = oneHole();
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::In100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);

  ASSERT_EQ(golfRemoveLatestPenalty(round, 0, GolfField::Out100), GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(round.penaltyCount[0], 2);
  GolfPenaltyEvent first{};
  GolfPenaltyEvent second{};
  ASSERT_TRUE(golfPenaltyEventAt(round, 0, 0, first));
  ASSERT_TRUE(golfPenaltyEventAt(round, 0, 1, second));
  EXPECT_EQ(first.field, GolfField::Out100);
  EXPECT_EQ(first.kind, GolfPenaltyKind::Hazard);
  EXPECT_EQ(second.field, GolfField::In100);
  EXPECT_EQ(second.kind, GolfPenaltyKind::Ob);
  EXPECT_EQ(round.out100[0], 1);
  EXPECT_EQ(round.in100[0], 1);
}

TEST(GolfPenalty, CapRefusesWithoutMutation) {
  GolfRound round = oneHole();
  for (uint8_t index = 0; index < GolfRound::MAX_PENALTIES_PER_HOLE; ++index) {
    ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
              GolfPenaltyMutationStatus::Changed);
  }
  const GolfRound before = round;
  EXPECT_EQ(golfAppendPenalty(round, 0, GolfField::In100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::HoleFull);
  EXPECT_EQ(memcmp(&round, &before, sizeof(round)), 0);
}

TEST(GolfPenalty, StrokeTotalsCoverHazardObMixedAndCap) {
  GolfRound hazard = oneHole();
  ASSERT_EQ(golfAppendPenalty(hazard, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(hazard, 0), 1);

  GolfRound ob = oneHole();
  ASSERT_EQ(golfAppendPenalty(ob, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(ob, 0), 2);

  GolfRound mixed = oneHole();
  ASSERT_EQ(golfAppendPenalty(mixed, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfAppendPenalty(mixed, 0, GolfField::In100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(golfPenaltyStrokesForHole(mixed, 0), 3);
  EXPECT_EQ(golfPenaltyStrokesForRound(mixed), 3);

  GolfRound capped = oneHole();
  for (uint8_t index = 0; index < GolfRound::MAX_PENALTIES_PER_HOLE; ++index) {
    ASSERT_EQ(golfAppendPenalty(capped, 0, GolfField::Out100,
                                index % 2 == 0 ? GolfPenaltyKind::Hazard : GolfPenaltyKind::Ob),
              GolfPenaltyMutationStatus::Changed);
  }
  EXPECT_EQ(golfPenaltyStrokesForHole(capped, 0), 12);
}

TEST(GolfPenalty, WorkedParFourWaterScoresSix) {
  GolfRound round = oneHole();
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  increment(round, GolfField::Out100, 2);
  increment(round, GolfField::Putts, 2);
  EXPECT_EQ(round.out100[0], 3);
  EXPECT_EQ(round.in100[0], 2);
  EXPECT_EQ(golfPenaltyStrokesForHole(round, 0), 1);
  EXPECT_EQ(static_cast<uint16_t>(round.in100[0] + round.out100[0] + golfPenaltyStrokesForHole(round, 0)), 6);
}

TEST(GolfPenalty, WorkedParFourObScoresSeven) {
  GolfRound round = oneHole();
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  increment(round, GolfField::Out100, 2);
  increment(round, GolfField::Putts, 2);
  EXPECT_EQ(static_cast<uint16_t>(round.in100[0] + round.out100[0] + golfPenaltyStrokesForHole(round, 0)), 7);
}

// CONTRACTS-V2 §13.1: a penalty added to an unlogged hole must be applied on top
// of the seeded par preview, never on top of bare stored zeros — otherwise the
// golfer's displayed putts and inside-100 appear wiped the moment the hole flips
// to "entered". The scoring screen orchestrates this by seeding first
// (GolfScoringActivity::ensureHoleSeeded); these tests lock down the composition
// it depends on.
TEST(GolfPenalty, PenaltyOnSeededUnloggedHoleKeepsPuttsAndIn100) {
  GolfRound seeded = oneHole();
  seeded.par[0] = 5;
  ASSERT_TRUE(seedGolfHoleAtPar(seeded, 0));
  ASSERT_EQ(golfAppendPenalty(seeded, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(seeded.putts[0], 2);
  EXPECT_EQ(seeded.in100[0], 2);
  EXPECT_EQ(seeded.out100[0], 4);  // 3 seeded + the shot that was played
  EXPECT_EQ(golfPenaltyStrokesForHole(seeded, 0), 1);

  // Without the seed the same press leaves the hole reading 0 / 0 / 1 — the bug.
  GolfRound unseeded = oneHole();
  unseeded.par[0] = 5;
  ASSERT_EQ(golfAppendPenalty(unseeded, 0, GolfField::Out100, GolfPenaltyKind::Hazard),
            GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(unseeded.putts[0], 0);
  EXPECT_EQ(unseeded.in100[0], 0);
}

TEST(GolfPenalty, RemovePenaltyFromSeededHoleRestoresPreview) {
  GolfRound round = oneHole();
  round.par[0] = 5;
  ASSERT_TRUE(seedGolfHoleAtPar(round, 0));
  const GolfRound afterSeed = round;
  ASSERT_EQ(golfAppendPenalty(round, 0, GolfField::Out100, GolfPenaltyKind::Ob), GolfPenaltyMutationStatus::Changed);
  ASSERT_EQ(golfRemoveLatestPenalty(round, 0, GolfField::Out100), GolfPenaltyMutationStatus::Changed);
  EXPECT_EQ(memcmp(&round, &afterSeed, sizeof(round)), 0);
}

TEST(GolfPenalty, SeedIsIdempotentSoRepeatMutationPathsAreSafe) {
  GolfRound round = oneHole();
  round.par[0] = 4;
  ASSERT_TRUE(seedGolfHoleAtPar(round, 0));
  const GolfRound afterSeed = round;
  // A second seed (a different mutation path re-entering) must be a no-op.
  EXPECT_FALSE(seedGolfHoleAtPar(round, 0));
  EXPECT_EQ(memcmp(&round, &afterSeed, sizeof(round)), 0);
}
