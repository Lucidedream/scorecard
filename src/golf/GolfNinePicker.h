#pragma once

#include "CourseOrder.h"

#if defined(CROSSPOINT_GOLF)
inline GolfNineSelection golfDefaultNineSelection(const GolfCourseSummary& course) {
  return golfDefaultNineSelection(course.nineCount);
}

inline bool golfSwapNines(const GolfCourse& course, GolfNineSelection& selection) {
  if (course.nineCount < 2 || !golfNineSelectionValid(course, selection)) return false;
  const uint8_t front = selection.front;
  selection.front = selection.back;
  selection.back = front;
  return true;
}

inline bool golfChooseNine(const GolfCourse& course, GolfNineSelection& selection, const bool backSlot,
                           const uint8_t nine) {
  if (course.nineCount < 2 || !golfNineSelectionValid(course, selection) || nine >= course.nineCount) return false;
  uint8_t& slot = backSlot ? selection.back : selection.front;
  uint8_t& other = backSlot ? selection.front : selection.back;
  if (nine == other) other = slot;
  slot = nine;
  return true;
}

inline uint16_t golfNinePar(const GolfCourse& course, const uint8_t nine) {
  if (nine >= course.nineCount || nine >= GOLF_MAX_NINES) return 0;
  uint16_t total = 0;
  for (const uint8_t par : course.nines[nine].par) total += par;
  return total;
}

// Loaded tees are longest-first across the whole course.
inline bool golfNineLongestTeeYards(const GolfCourse& course, const uint8_t nine, uint32_t& yards) {
  yards = 0;
  if (nine >= course.nineCount || nine >= GOLF_MAX_NINES) return false;
  for (uint8_t tee = 0; tee < course.teeCount && tee < GOLF_MAX_TEES; ++tee) {
    if (!course.tees[tee].hasYards) continue;
    for (const uint16_t value : course.tees[tee].yards[nine]) yards += value;
    return true;
  }
  return false;
}
#endif
