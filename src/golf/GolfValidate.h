#pragma once

#include <cstdint>

#include "GolfRound.h"

struct GolfValidationResult {
  bool valid;
  bool currentHoleReset;
  uint32_t puttsRepaired;
  uint32_t in100Repaired;
  uint32_t penaltyCountRepaired;
  uint32_t penaltyEventRepaired;
  uint32_t penaltyMarkerRepaired;

  bool repaired() const {
    return currentHoleReset || puttsRepaired != 0 || in100Repaired != 0 || penaltyCountRepaired != 0 ||
           penaltyEventRepaired != 0 || penaltyMarkerRepaired != 0;
  }
  bool holePuttsRepaired(uint8_t hole) const { return (puttsRepaired & (1UL << hole)) != 0; }
  bool holeIn100Repaired(uint8_t hole) const { return (in100Repaired & (1UL << hole)) != 0; }
  bool holePenaltyCountRepaired(uint8_t hole) const { return (penaltyCountRepaired & (1UL << hole)) != 0; }
  bool holePenaltyEventRepaired(uint8_t hole) const { return (penaltyEventRepaired & (1UL << hole)) != 0; }
  bool holePenaltyMarkerRepaired(uint8_t hole) const { return (penaltyMarkerRepaired & (1UL << hole)) != 0; }
};

GolfValidationResult validateGolfRound(GolfRound& round);
