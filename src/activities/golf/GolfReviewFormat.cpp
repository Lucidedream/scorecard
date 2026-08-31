#include "GolfReviewFormat.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>

#include "GolfStrings.h"
#include "golf/GolfStats.h"

void golfFormatReviewToPar(const int16_t value, char* output, const size_t size) {
  if (value == 0) {
    snprintf(output, size, "%s", GolfStrings::EVEN);
  } else {
    snprintf(output, size, value > 0 ? "+%d" : "%d", value);
  }
}

void golfFormatRoundStatus(const GolfRound& round, char* output, const size_t size) {
  if (!golfHasPar(round)) {
    snprintf(output, size, "%u", golfScore(round));
    return;
  }
  char toPar[8];
  golfFormatReviewToPar(golfToPar(round), toPar, sizeof(toPar));
  snprintf(output, size, "%u (%s)", golfScore(round), toPar);
}

void golfFormatReviewPercent(const uint16_t part, const uint16_t whole, char* output, const size_t size) {
  const uint16_t tenths =
      whole == 0 ? 0 : static_cast<uint16_t>((static_cast<uint32_t>(part) * 1000 + whole / 2) / whole);
  snprintf(output, size, GolfStrings::PERCENT_FORMAT, static_cast<unsigned long>(tenths / 10),
           static_cast<unsigned long>(tenths % 10));
}

#endif
