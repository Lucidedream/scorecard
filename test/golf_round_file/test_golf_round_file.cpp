#include <gtest/gtest.h>

#include <cstring>

#include "GolfRoundDecode.h"

// GolfRoundFile's JSON tokenizing (ArduinoJson) and SD I/O are firmware-only, as
// with CourseStore. What is host-testable is the JSON-free core it shares with
// GolfRoundStore::fromJson: golfCheckRound(). It sees the per-hole arrays already
// read into `out`, so these tests populate them directly.

namespace {

GolfRoundColumnLengths lengths(const uint16_t count, const bool expectYards = false) {
  return GolfRoundColumnLengths{count,      count, count, count, expectYards ? count : static_cast<uint16_t>(0),
                                expectYards};
}

GolfRound roundWithHole(const uint8_t hole, const uint8_t putts, const uint8_t in100, const uint8_t out100) {
  GolfRound out{};
  for (uint8_t i = 0; i < 18; ++i) out.par[i] = 4;
  out.putts[hole] = putts;
  out.in100[hole] = in100;
  out.out100[hole] = out100;
  return out;
}

}  // namespace

TEST(GolfRoundFile, RejectsVersionOne) {
  GolfRound out{};
  GolfValidationResult validation{};
  EXPECT_EQ(golfCheckRound(out, 1, 18, 0, lengths(18), validation), GolfRoundDecodeStatus::RejectedVersion);
}

TEST(GolfRoundFile, RejectsMissingVersion) {
  GolfRound out{};
  GolfValidationResult validation{};
  // A missing "v" key decodes to 0 via ArduinoJson's `| 0` default.
  EXPECT_EQ(golfCheckRound(out, 0, 18, 0, lengths(18), validation), GolfRoundDecodeStatus::RejectedVersion);
}

TEST(GolfRoundFile, RejectsArrayLengthMismatch) {
  GolfRound out{};
  GolfValidationResult validation{};
  GolfRoundColumnLengths mismatched = lengths(18);
  mismatched.putts = 17;
  EXPECT_EQ(golfCheckRound(out, 2, 18, 0, mismatched, validation), GolfRoundDecodeStatus::RejectedArrayLength);
}

TEST(GolfRoundFile, RejectsUnsupportedHoleCount) {
  GolfRound out{};
  GolfValidationResult validation{};
  EXPECT_EQ(golfCheckRound(out, 2, 13, 0, lengths(13), validation), GolfRoundDecodeStatus::RejectedHoleCount);
}

TEST(GolfRoundFile, ParsesValidRoundPreservingPerHoleValues) {
  GolfRound out{};
  for (uint8_t hole = 0; hole < 18; ++hole) {
    out.par[hole] = 4;
    out.putts[hole] = 2;
    out.in100[hole] = 3;
    out.out100[hole] = 2;
  }
  out.out100[5] = 4;  // a distinctive hole
  GolfValidationResult validation{};

  EXPECT_EQ(golfCheckRound(out, 2, 18, 0, lengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(out.holeCount, 18);
  EXPECT_FALSE(validation.repaired());
  EXPECT_EQ(out.putts[5], 2);
  EXPECT_EQ(out.in100[5], 3);
  EXPECT_EQ(out.out100[5], 4);
}

TEST(GolfRoundFile, RepairsAndReportsInvalidPuttsRelationship) {
  GolfRound out = roundWithHole(3, /*putts=*/5, /*in100=*/2, /*out100=*/1);
  GolfValidationResult validation{};

  EXPECT_EQ(golfCheckRound(out, 2, 18, 0, lengths(18), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_TRUE(validation.repaired());
  EXPECT_TRUE(validation.holePuttsRepaired(3));
  EXPECT_EQ(out.putts[3], 2);  // pulled down to the in100 floor
}

TEST(GolfRoundFile, ClampsAbsentCurrentHoleToZeroWithoutRepairFlag) {
  GolfRound out{};
  for (uint8_t i = 0; i < 9; ++i) out.par[i] = 4;
  GolfValidationResult validation{};

  EXPECT_EQ(golfCheckRound(out, 2, 9, 0, lengths(9), validation), GolfRoundDecodeStatus::Ok);
  EXPECT_EQ(out.currentHole, 0);
  EXPECT_FALSE(validation.currentHoleReset);
}

TEST(GolfRoundFile, EnforcesYardsLengthOnlyWhenExpected) {
  GolfRound out{};
  for (uint8_t i = 0; i < 18; ++i) out.par[i] = 4;
  GolfValidationResult validation{};

  GolfRoundColumnLengths noYards = lengths(18, /*expectYards=*/false);
  noYards.yards = 3;  // stale value must be ignored for the completed-round schema
  EXPECT_EQ(golfCheckRound(out, 2, 18, 0, noYards, validation), GolfRoundDecodeStatus::Ok);

  GolfRoundColumnLengths withYards = lengths(18, /*expectYards=*/true);
  withYards.yards = 3;
  EXPECT_EQ(golfCheckRound(out, 2, 18, 0, withYards, validation), GolfRoundDecodeStatus::RejectedArrayLength);
}
