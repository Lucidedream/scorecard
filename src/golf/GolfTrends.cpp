#include "GolfTrends.h"

#if defined(CROSSPOINT_GOLF)

#include <limits>

namespace {

uint32_t averageTenths(const uint32_t sum, const uint8_t count) {
  return count == 0 ? 0 : (sum * 10 + count / 2) / count;
}

int32_t signedAverageTenths(const int32_t sum, const uint8_t count) {
  if (count == 0) return 0;
  return (sum * 10 + (sum < 0 ? -static_cast<int32_t>(count / 2) : count / 2)) / count;
}

uint32_t percentTenths(const uint32_t part, const uint32_t whole) {
  return whole == 0 ? 0 : (part * 1000 + whole / 2) / whole;
}

}  // namespace

GolfTrendStats golfCalculateTrends(const GolfHistoryReader& history) {
  GolfTrendStats result{};
  uint32_t strokes = 0;
  uint32_t putts = 0;
  uint32_t longGame = 0;
  uint32_t shortGame = 0;
  int32_t toPar = 0;
  bool allHavePar = true;
  uint16_t best = std::numeric_limits<uint16_t>::max();
  uint16_t worst = 0;

  for (uint8_t index = 0; index < history.count(); ++index) {
    const GolfHistoryEntry& entry = history.newest(index);
    if (entry.holes != 18) continue;

    ++result.rounds;
    strokes += entry.strokes;
    putts += entry.putts;
    longGame += entry.out100;
    shortGame += entry.in100 >= entry.putts ? entry.in100 - entry.putts : 0;
    if (entry.par == 0) {
      allHavePar = false;
    } else {
      toPar += static_cast<int32_t>(entry.strokes) - entry.par;
    }
    if (entry.strokes < best) best = entry.strokes;
    if (entry.strokes > worst) worst = entry.strokes;
  }

  if (result.rounds == 0) return result;
  result.showsToPar = allHavePar;
  result.best = best;
  result.worst = worst;
  result.scoringAverageTenths = averageTenths(strokes, result.rounds);
  result.toParAverageTenths = allHavePar ? signedAverageTenths(toPar, result.rounds) : 0;
  result.puttsAverageTenths = averageTenths(putts, result.rounds);
  result.longAverageTenths = averageTenths(longGame, result.rounds);
  result.shortAverageTenths = averageTenths(shortGame, result.rounds);
  result.puttingAverageTenths = result.puttsAverageTenths;
  result.longPercentTenths = percentTenths(longGame, strokes);
  result.shortPercentTenths = percentTenths(shortGame, strokes);
  result.puttingPercentTenths = percentTenths(putts, strokes);
  return result;
}

#endif
