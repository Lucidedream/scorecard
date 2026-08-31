#include "GolfTrends.h"

#if defined(CROSSPOINT_GOLF)

#include <limits>

namespace {

int32_t averageTenths(const int32_t sum, const uint8_t count) {
  if (count == 0) return 0;
  return (sum * 10 + (sum < 0 ? -static_cast<int32_t>(count / 2) : count / 2)) / count;
}

void mixPercentTenths(const uint32_t parts[4], uint32_t output[4]) {
  const uint32_t whole = parts[0] + parts[1] + parts[2] + parts[3];
  if (whole == 0) return;
  uint32_t remainders[4]{};
  uint16_t assigned = 0;
  for (uint8_t index = 0; index < 4; ++index) {
    const uint64_t scaled = static_cast<uint64_t>(parts[index]) * 1000;
    output[index] = static_cast<uint32_t>(scaled / whole);
    remainders[index] = static_cast<uint32_t>(scaled % whole);
    assigned = static_cast<uint16_t>(assigned + output[index]);
  }
  // Largest-remainder apportionment makes the displayed shares total exactly
  // 100.0% without floating point or biasing one named bucket.
  while (assigned < 1000) {
    uint8_t largest = 0;
    for (uint8_t index = 1; index < 4; ++index) {
      if (remainders[index] > remainders[largest]) largest = index;
    }
    ++output[largest];
    remainders[largest] = 0;
    ++assigned;
  }
}

}  // namespace

GolfTrendStats golfCalculateTrends(const GolfHistoryReader& history) {
  GolfTrendStats result{};
  uint32_t strokes = 0;
  uint32_t putts = 0;
  uint32_t mixLong = 0;
  uint32_t mixShort = 0;
  uint32_t mixPutting = 0;
  int32_t toPar = 0;
  bool allHavePar = true;
  uint16_t best = std::numeric_limits<uint16_t>::max();
  uint16_t worst = 0;
  uint32_t hazards = 0;
  uint32_t obs = 0;

  for (uint8_t index = 0; index < history.count(); ++index) {
    const GolfHistoryEntry& entry = history.newest(index);
    if (entry.holes != 18) continue;

    ++result.rounds;
    strokes += entry.strokes;
    putts += entry.putts;
    if (entry.par == 0) {
      allHavePar = false;
    } else {
      toPar += static_cast<int32_t>(entry.strokes) - entry.par;
    }
    if (entry.strokes < best) best = entry.strokes;
    if (entry.strokes > worst) worst = entry.strokes;
    if (entry.penaltiesRecorded) {
      ++result.penaltyRounds;
      mixLong += entry.out100;
      mixShort += entry.in100 >= entry.putts ? entry.in100 - entry.putts : 0;
      mixPutting += entry.putts;
      hazards += entry.hazards;
      obs += entry.obs;
    }
  }

  if (result.rounds == 0) return result;
  result.showsToPar = allHavePar;
  result.best = best;
  result.worst = worst;
  result.scoringAverageTenths = averageTenths(strokes, result.rounds);
  result.toParAverageTenths = allHavePar ? averageTenths(toPar, result.rounds) : 0;
  result.puttsAverageTenths = averageTenths(putts, result.rounds);
  result.showsPenalties = result.penaltyRounds >= 2;
  if (result.showsPenalties) {
    const uint32_t penaltyStrokes = hazards + obs * 2;
    result.longAverageTenths = averageTenths(static_cast<int32_t>(mixLong), result.penaltyRounds);
    result.shortAverageTenths = averageTenths(static_cast<int32_t>(mixShort), result.penaltyRounds);
    result.puttingAverageTenths = averageTenths(static_cast<int32_t>(mixPutting), result.penaltyRounds);
    result.hazardsAverageTenths = averageTenths(static_cast<int32_t>(hazards), result.penaltyRounds);
    result.obsAverageTenths = averageTenths(static_cast<int32_t>(obs), result.penaltyRounds);
    // strokes cost per CONTRACTS-V2 §12.2: hazard +1, OB +2.
    result.penaltyStrokesAverageTenths = averageTenths(static_cast<int32_t>(penaltyStrokes), result.penaltyRounds);
    const uint32_t parts[] = {mixLong, mixShort, mixPutting, penaltyStrokes};
    uint32_t percentages[4]{};
    mixPercentTenths(parts, percentages);
    result.longPercentTenths = percentages[0];
    result.shortPercentTenths = percentages[1];
    result.puttingPercentTenths = percentages[2];
    result.penaltyPercentTenths = percentages[3];
  }
  return result;
}

#endif
