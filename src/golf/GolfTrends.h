#pragma once

#include <cstdint>

#include "GolfHistory.h"

struct GolfTrendStats {
  uint8_t rounds = 0;
  bool showsToPar = false;
  uint16_t best = 0;
  uint16_t worst = 0;
  uint32_t scoringAverageTenths = 0;
  int32_t toParAverageTenths = 0;
  uint32_t puttsAverageTenths = 0;
  uint32_t longAverageTenths = 0;
  uint32_t shortAverageTenths = 0;
  uint32_t puttingAverageTenths = 0;
  uint32_t longPercentTenths = 0;
  uint32_t shortPercentTenths = 0;
  uint32_t puttingPercentTenths = 0;
  uint32_t penaltyPercentTenths = 0;

  // The complete four-bucket mix folds over the 18-hole rounds that actually
  // recorded penalty data. Older rounds are excluded from every mix bucket.
  uint8_t penaltyRounds = 0;
  bool showsPenalties = false;
  uint32_t hazardsAverageTenths = 0;
  uint32_t obsAverageTenths = 0;
  uint32_t penaltyStrokesAverageTenths = 0;

  // FIR and GIR fold independently of the penalty mix: a v4-era round carries a
  // derived GIR but never a fairway record, so each has its own round count and
  // "shows" gate (>= 2 qualifying 18-hole rounds).
  uint32_t firPercentTenths = 0;
  uint32_t girPercentTenths = 0;
  uint8_t firRounds = 0;
  uint8_t girRounds = 0;
  bool showsFir = false;
  bool showsGir = false;

  bool enoughRounds() const { return rounds >= 2; }
  bool enoughMixRounds() const { return penaltyRounds >= 2; }
};

// `history` must have been reset for one stable player slot; no cross-slot
// aggregation is performed here.
GolfTrendStats golfCalculateTrends(const GolfHistoryReader& history);
