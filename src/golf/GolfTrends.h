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

  // Penalty figures fold over the 18-hole rounds that actually recorded penalty
  // data. Rounds played before penalty tracking carry no data and are excluded,
  // never counted as a clean (zero-penalty) round — the same suppression the
  // par figures apply when a round has no par.
  uint8_t penaltyRounds = 0;
  bool showsPenalties = false;
  uint32_t hazardsAverageTenths = 0;
  uint32_t obsAverageTenths = 0;
  uint32_t penaltyStrokesAverageTenths = 0;

  bool enoughRounds() const { return rounds >= 2; }
};

GolfTrendStats golfCalculateTrends(const GolfHistoryReader& history);
