#pragma once

#include <cstdint>

#include "GolfRound.h"

struct GolfValidationResult {
  bool valid;
  bool currentHoleReset;
  uint32_t puttsRepaired;
  uint32_t in100Repaired;

  bool repaired() const { return currentHoleReset || puttsRepaired != 0 || in100Repaired != 0; }
  bool holePuttsRepaired(uint8_t hole) const { return (puttsRepaired & (1UL << hole)) != 0; }
  bool holeIn100Repaired(uint8_t hole) const { return (in100Repaired & (1UL << hole)) != 0; }
};

GolfValidationResult validateGolfRound(GolfRound& round);
