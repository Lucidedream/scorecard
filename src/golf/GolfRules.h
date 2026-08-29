#pragma once

#include <cstdint>

#include "GolfRound.h"

enum class GolfField : uint8_t { Putts = 0, In100 = 1, Out100 = 2 };

struct GolfMutationResult {
  bool changed;
  bool carriedIn100;
  bool loweredPutts;
};

GolfMutationResult incrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field);
GolfMutationResult decrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field);
GolfField nextGolfField(GolfField field);
bool seedGolfHoleAtPar(GolfRound& round, uint8_t hole);
