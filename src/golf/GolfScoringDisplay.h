#pragma once

#include <cstdint>

#include "GolfRules.h"
#include "GolfStats.h"

struct GolfScoringHoleDisplay {
  uint8_t counters[3];
  uint16_t score;
  bool seeded;
};

inline GolfScoringHoleDisplay golfScoringHoleDisplay(const GolfRound& round, const GolfPlayerScore& storedScore,
                                                     const uint8_t hole) {
  if (hole >= round.holeCount || hole >= GolfRound::MAX_HOLES) return {};
  GolfPlayerScore displayScore = storedScore;
  const bool seeded = seedGolfHoleAtPar(displayScore, hole, round.par[hole]);
  return {{displayScore.putts[hole], displayScore.in100[hole], displayScore.out100[hole]},
          golfHoleScore(round, displayScore, hole), seeded};
}
