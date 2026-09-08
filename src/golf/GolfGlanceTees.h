#pragma once

#include <cstdint>
#include <cstring>

#include "CourseStore.h"

// CONTRACTS-V2 §34.1: the course-map hole band lists at most two tees on its
// yardage glance line. This picks which tees GolfCourseMapBrowserActivity shows:
//
//   * teeCount <= 2  -> every tee in set order that has yardage (0, 1 or 2).
//   * teeCount  > 2  -> exactly two: the tee named "Blue" then the tee named
//                       "Red" when each is present with yardage (case-sensitive
//                       name match, Blue first), then topped up from the first
//                       remaining tees in set order that have yardage until two
//                       are shown.
//
// Tees without yardage are never shown. Writes the picks to out[0..return) and
// nulls the remainder; the return value is 0, 1 or 2.
inline uint8_t pickGlanceTees(const GolfCourseTeeSet& teeSet, const GolfCourseTee* out[2]) {
  out[0] = nullptr;
  out[1] = nullptr;
  uint8_t count = 0;

  if (teeSet.teeCount <= 2) {
    for (uint8_t i = 0; i < teeSet.teeCount && count < 2; ++i) {
      if (teeSet.tees[i].hasYards) out[count++] = &teeSet.tees[i];
    }
    return count;
  }

  static constexpr const char* PRIORITY_NAMES[] = {"Blue", "Red"};
  for (const char* const name : PRIORITY_NAMES) {
    for (uint8_t i = 0; i < teeSet.teeCount; ++i) {
      const GolfCourseTee& tee = teeSet.tees[i];
      if (tee.hasYards && strcmp(tee.name, name) == 0) {
        out[count++] = &tee;
        break;
      }
    }
  }

  for (uint8_t i = 0; i < teeSet.teeCount && count < 2; ++i) {
    const GolfCourseTee& tee = teeSet.tees[i];
    if (!tee.hasYards) continue;
    if (count == 1 && out[0] == &tee) continue;
    out[count++] = &tee;
  }
  return count;
}
