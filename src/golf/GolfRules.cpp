#include "GolfRules.h"

#if defined(CROSSPOINT_GOLF)

namespace {

constexpr uint8_t MAX_COUNTER = 99;

GolfMutationResult clamped() { return {false, false, false}; }

GolfMutationResult changed(bool carriedIn100 = false, bool loweredPutts = false) {
  return {true, carriedIn100, loweredPutts};
}

bool isValidHole(const GolfRound& round, uint8_t hole) { return hole < round.holeCount && hole < GolfRound::MAX_HOLES; }

}  // namespace

bool seedGolfHoleAtPar(GolfRound& round, uint8_t hole) {
  if (!isValidHole(round, hole) || round.par[hole] < 3 || round.in100[hole] != 0 || round.out100[hole] != 0) {
    return false;
  }
  round.putts[hole] = 2;
  round.in100[hole] = 2;
  round.out100[hole] = round.par[hole] > 2 ? static_cast<uint8_t>(round.par[hole] - 2) : 0;
  return true;
}

GolfMutationResult incrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field) {
  if (!isValidHole(round, hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Putts: {
      if (round.putts[hole] >= MAX_COUNTER || round.in100[hole] >= MAX_COUNTER) return clamped();
      ++round.putts[hole];
      const bool carried = round.putts[hole] > round.in100[hole];
      if (carried) round.in100[hole] = round.putts[hole];
      return changed(carried);
    }
    case GolfField::In100:
      if (round.in100[hole] >= MAX_COUNTER) return clamped();
      ++round.in100[hole];
      return changed();
    case GolfField::Out100:
      if (round.out100[hole] >= MAX_COUNTER) return clamped();
      ++round.out100[hole];
      return changed();
  }

  return clamped();
}

GolfMutationResult decrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field) {
  if (!isValidHole(round, hole)) {
    return clamped();
  }

  switch (field) {
    case GolfField::Putts:
      if (round.putts[hole] == 0) return clamped();
      --round.putts[hole];
      return changed();
    case GolfField::In100: {
      if (round.in100[hole] == 0) return clamped();
      --round.in100[hole];
      const bool lowered = round.putts[hole] > round.in100[hole];
      if (lowered) round.putts[hole] = round.in100[hole];
      return changed(false, lowered);
    }
    case GolfField::Out100:
      if (round.out100[hole] == 0) return clamped();
      --round.out100[hole];
      return changed();
  }

  return clamped();
}

GolfField nextGolfField(GolfField field) {
  switch (field) {
    case GolfField::Putts:
      return GolfField::In100;
    case GolfField::In100:
      return GolfField::Out100;
    case GolfField::Out100:
      return GolfField::Putts;
  }
  return GolfField::Putts;
}

#endif
