#pragma once

#include <cstdint>

#include "GolfRound.h"

enum class GolfField : uint8_t { Strokes = 0, Putts = 1, In100 = 2 };

struct GolfMutationResult {
  bool changed;
  bool blocked;
  uint8_t blockingFields;
  bool autoBumpedStrokes;
};

GolfMutationResult incrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field);
GolfMutationResult decrementGolfCounter(GolfRound& round, uint8_t hole, GolfField field);
GolfField nextGolfField(GolfField field);
