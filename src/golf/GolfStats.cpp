#include "GolfStats.h"

#if defined(CROSSPOINT_GOLF)

#include "GolfPenalty.h"

namespace {

uint8_t holesInRound(const GolfRound& round) {
  return round.holeCount < GolfRound::MAX_HOLES ? round.holeCount : GolfRound::MAX_HOLES;
}

bool isEntered(const GolfRound& round, uint8_t hole) {
  return static_cast<uint16_t>(round.in100[hole]) + round.out100[hole] != 0;
}

}  // namespace

uint8_t golfLongGame(const GolfRound& round, uint8_t hole) {
  if (hole >= holesInRound(round) || !isEntered(round, hole)) {
    return 0;
  }
  return round.out100[hole];
}

uint16_t golfPenaltyTotal(const GolfRound& round) { return golfPenaltyStrokesForRound(round); }

uint16_t golfHoleScore(const GolfRound& round, uint8_t hole) {
  if (hole >= holesInRound(round)) return 0;
  return static_cast<uint16_t>(round.in100[hole]) + round.out100[hole] + golfPenaltyStrokesForHole(round, hole);
}

uint16_t golfScore(const GolfRound& round) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) {
      total += golfHoleScore(round, hole);
    }
  }
  return total;
}

uint16_t golfParTotal(const GolfRound& round) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) total += round.par[hole];
  }
  return total;
}

bool golfHasPar(const GolfRound& round) {
  if (holesInRound(round) == 0) return false;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (round.par[hole] < 3 || round.par[hole] > 6) return false;
  }
  return true;
}

int16_t golfToPar(const GolfRound& round) {
  int16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole) && round.par[hole] != 0) {
      total += static_cast<int16_t>(golfHoleScore(round, hole)) - round.par[hole];
    }
  }
  return total;
}

uint8_t golfThru(const GolfRound& round) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) {
      ++total;
    }
  }
  return total;
}

uint16_t golfPuttsTotal(const GolfRound& round) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) {
      total += round.putts[hole];
    }
  }
  return total;
}

uint16_t golfIn100Total(const GolfRound& round) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) {
      total += round.in100[hole];
    }
  }
  return total;
}

uint16_t golfShortTotal(const GolfRound& round) {
  return static_cast<uint16_t>(golfIn100Total(round) - golfPuttsTotal(round));
}

uint16_t golfLongTotal(const GolfRound& round) {
  uint16_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole)) {
      total += golfLongGame(round, hole);
    }
  }
  return total;
}

uint8_t golfOnePutts(const GolfRound& round) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole) && round.putts[hole] == 1) {
      ++total;
    }
  }
  return total;
}

uint8_t golfThreePutts(const GolfRound& round) {
  uint8_t total = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (isEntered(round, hole) && round.putts[hole] >= 3) {
      ++total;
    }
  }
  return total;
}

uint8_t golfWorstHoles(const GolfRound& round, GolfWorstHole* holes, uint8_t capacity) {
  if (holes == nullptr || capacity == 0) {
    return 0;
  }

  uint8_t count = 0;
  for (uint8_t hole = 0; hole < holesInRound(round); ++hole) {
    if (!isEntered(round, hole) || round.par[hole] == 0) {
      continue;
    }

    GolfWorstHole candidate{hole,
                            static_cast<int16_t>(static_cast<int16_t>(golfHoleScore(round, hole)) - round.par[hole])};
    uint8_t position = count;
    if (count < capacity) {
      ++count;
    } else {
      if (holes[capacity - 1].toPar >= candidate.toPar) {
        continue;
      }
      position = capacity - 1;
    }
    while (position > 0 && holes[position - 1].toPar < candidate.toPar) {
      holes[position] = holes[position - 1];
      --position;
    }
    holes[position] = candidate;
  }
  return count;
}

#endif
