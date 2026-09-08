#include <gtest/gtest.h>

#include <cstring>

#include "GolfCareerStats.h"
#include "GolfPenalty.h"
#include "GolfStats.h"

namespace {

// Builds an 18-hole round, slot 0 enabled, every hole par 4 by default.
GolfRound makeRound(const char* course = "Course") {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  round.holeCount = 18;
  golfSetTee(round.players[0], "Blue");
  snprintf(round.courseName, sizeof(round.courseName), "%s", course);
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) round.par[hole] = 4;
  return round;
}

// Marks a hole entered with the given gross strokes (holeScore == strokes) and
// putt count. in100 is always >= 1 so the hole reads as entered.
void playHole(GolfRound& round, const uint8_t hole, const uint8_t strokes, const uint8_t putts = 2) {
  GolfPlayerScore& s = round.players[0].score;
  s.in100[hole] = 1;
  s.out100[hole] = strokes > 1 ? static_cast<uint8_t>(strokes - 1) : 0;
  s.putts[hole] = putts;
}

TEST(GolfCareerStats, HoleCountAndSlotGates) {
  GolfCareerTally tally{};
  GolfRound nine = makeRound();
  nine.holeCount = 9;
  playHole(nine, 0, 4);
  golfFoldCareerRound(nine, 0, tally);
  EXPECT_EQ(tally.rounds, 0u);

  GolfRound round = makeRound();
  playHole(round, 0, 4);
  golfFoldCareerRound(round, 1, tally);  // slot 1 disabled
  EXPECT_EQ(tally.rounds, 0u);

  golfFoldCareerRound(round, 0, tally);
  EXPECT_EQ(tally.rounds, 1u);
}

TEST(GolfCareerStats, DistributionBucketsEachOutcome) {
  GolfCareerTally tally{};
  GolfRound round = makeRound();
  playHole(round, 0, 2);  // -2 eagle+
  playHole(round, 1, 3);  // -1 birdie
  playHole(round, 2, 4);  //  0 par
  playHole(round, 3, 5);  // +1 bogey
  playHole(round, 4, 6);  // +2 double
  playHole(round, 5, 7);  // +3 triple+
  playHole(round, 6, 1);  // -3 also eagle+ bucket
  playHole(round, 7, 8);  // +4 also triple+ bucket
  golfFoldCareerRound(round, 0, tally);

  EXPECT_EQ(tally.dist[0], 2u);
  EXPECT_EQ(tally.dist[1], 1u);
  EXPECT_EQ(tally.dist[2], 1u);
  EXPECT_EQ(tally.dist[3], 1u);
  EXPECT_EQ(tally.dist[4], 1u);
  EXPECT_EQ(tally.dist[5], 2u);
}

TEST(GolfCareerStats, ParTypeSumsAndCounts) {
  GolfCareerTally tally{};
  GolfRound round = makeRound();
  round.par[0] = 3;
  round.par[1] = 3;
  round.par[2] = 3;
  round.par[3] = 4;
  round.par[4] = 4;
  round.par[5] = 5;
  round.par[6] = 6;  // par 6 excluded from the by-par buckets
  playHole(round, 0, 4);
  playHole(round, 1, 3);
  playHole(round, 2, 3);
  playHole(round, 3, 5);
  playHole(round, 4, 4);
  playHole(round, 5, 6);
  playHole(round, 6, 7);
  golfFoldCareerRound(round, 0, tally);

  EXPECT_EQ(tally.parStrokes[0], 10u);  // 4 + 3 + 3
  EXPECT_EQ(tally.parHoles[0], 3u);
  EXPECT_EQ(tally.parStrokes[1], 9u);  // 5 + 4
  EXPECT_EQ(tally.parHoles[1], 2u);
  EXPECT_EQ(tally.parStrokes[2], 6u);  // 6
  EXPECT_EQ(tally.parHoles[2], 1u);
}

TEST(GolfCareerStats, ScrambleAndSandSaveSummedAcrossRounds) {
  GolfCareerTally tally{};

  GolfRound a = makeRound();
  playHole(a, 0, 4, 1);                                 // GIR missed (4-1=3 > par-2), made par -> scramble
  golfSetGreensideBunker(a.players[0].score, 0, true);  // sand-save chance + save
  playHole(a, 1, 5, 2);                                 // GIR missed, over par -> chance only
  golfSetGreensideBunker(a.players[0].score, 1, true);  // sand-save chance, no save
  golfFoldCareerRound(a, 0, tally);

  GolfRound b = makeRound("Other");
  playHole(b, 0, 4, 1);
  golfSetGreensideBunker(b.players[0].score, 0, true);
  golfFoldCareerRound(b, 0, tally);

  EXPECT_EQ(tally.scrambles, 2u);
  EXPECT_EQ(tally.scrambleChances, 3u);
  EXPECT_EQ(tally.sandSaves, 2u);
  EXPECT_EQ(tally.sandSaveChances, 3u);
}

TEST(GolfCareerStats, BogeyFreeRunBreaksOnNotEnteredHole) {
  GolfCareerTally tally{};
  GolfRound round = makeRound();
  playHole(round, 0, 4);
  playHole(round, 1, 4);
  playHole(round, 2, 4);
  // hole 3 left not entered -> breaks the run
  playHole(round, 4, 4);
  playHole(round, 5, 4);
  playHole(round, 6, 5);  // bogey ends the second run
  golfFoldCareerRound(round, 0, tally);
  EXPECT_EQ(tally.longestBogeyFreeRun, 3u);

  GolfCareerTally filled{};
  GolfRound whole = makeRound();
  for (uint8_t hole = 0; hole < 6; ++hole) playHole(whole, hole, 4);
  playHole(whole, 6, 5);
  golfFoldCareerRound(whole, 0, filled);
  EXPECT_EQ(filled.longestBogeyFreeRun, 6u);
}

TEST(GolfCareerStats, MostParsKeepsTheMax) {
  GolfCareerTally tally{};

  GolfRound r1 = makeRound();
  for (uint8_t hole = 0; hole < 5; ++hole) playHole(r1, hole, 4);
  golfFoldCareerRound(r1, 0, tally);
  EXPECT_EQ(tally.mostPars, 5u);

  GolfRound r2 = makeRound();
  for (uint8_t hole = 0; hole < 8; ++hole) playHole(r2, hole, 4);
  golfFoldCareerRound(r2, 0, tally);
  EXPECT_EQ(tally.mostPars, 8u);

  GolfRound r3 = makeRound();
  for (uint8_t hole = 0; hole < 3; ++hole) playHole(r3, hole, 4);
  golfFoldCareerRound(r3, 0, tally);
  EXPECT_EQ(tally.mostPars, 8u);
}

TEST(GolfCareerStats, LowestRoundAndFewestPuttsRecords) {
  GolfCareerTally tally{};

  GolfRound alpha = makeRound("Alpha");
  for (uint8_t hole = 0; hole < 18; ++hole) playHole(alpha, hole, 5, 2);  // gross 90, 36 putts
  golfFoldCareerRound(alpha, 0, tally);

  GolfRound bravo = makeRound("Bravo");
  for (uint8_t hole = 0; hole < 18; ++hole) playHole(bravo, hole, 4, 1);  // gross 72, 18 putts
  golfFoldCareerRound(bravo, 0, tally);

  // A partial round must not set a record even though its gross is tiny.
  GolfRound partial = makeRound("Partial");
  for (uint8_t hole = 0; hole < 6; ++hole) playHole(partial, hole, 2, 0);
  golfFoldCareerRound(partial, 0, tally);

  EXPECT_EQ(tally.lowestRound, 72u);
  EXPECT_STREQ(tally.lowestCourse, "Bravo");
  EXPECT_EQ(tally.fewestPutts, 18u);
  EXPECT_EQ(tally.rounds, 3u);
}

TEST(GolfCareerStats, ParFreeRoundCountsForScoreAndRecordsButNotToPar) {
  GolfCareerTally tally{};

  GolfRound withPar = makeRound("WithPar");
  for (uint8_t hole = 0; hole < 18; ++hole) playHole(withPar, hole, 5, 2);  // gross 90, +18 to par
  golfFoldCareerRound(withPar, 0, tally);

  GolfRound parFree = makeRound("ParFree");
  for (uint8_t hole = 0; hole < 18; ++hole) {
    parFree.par[hole] = 0;
    playHole(parFree, hole, 4, 1);  // gross 72
  }
  golfFoldCareerRound(parFree, 0, tally);

  EXPECT_EQ(tally.rounds, 2u);
  EXPECT_EQ(tally.strokes, 90u + 72u);
  EXPECT_EQ(tally.parRounds, 1u);     // only the par round
  EXPECT_EQ(tally.toParTotal, 18);    // par-free round contributes nothing
  EXPECT_EQ(tally.lowestRound, 72u);  // par-free gross still a record
  EXPECT_STREQ(tally.lowestCourse, "ParFree");
}

TEST(GolfCareerStats, PuttsPerGirAndThreePutt) {
  GolfCareerTally tally{};
  GolfRound round = makeRound();
  playHole(round, 0, 4, 2);  // GIR (4-2=2 == par-2), 2 putts
  playHole(round, 1, 3, 1);  // GIR (3-1=2), 1 putt
  playHole(round, 2, 6, 3);  // not GIR, 3-putt
  golfFoldCareerRound(round, 0, tally);

  EXPECT_EQ(tally.girHoles, 2u);
  EXPECT_EQ(tally.puttsOnGir, 3u);
  EXPECT_EQ(tally.threePuttHoles, 1u);
  EXPECT_EQ(tally.holesPlayed, 3u);
}

}  // namespace
