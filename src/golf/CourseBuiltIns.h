#pragma once

#include <cstring>

#include "GolfCourse.h"

// Built-in courses list first, in this fixed table order (CONTRACTS-V2 §7); SD-card
// courses that override nothing follow, sorted alphabetically among themselves. An SD
// file that overrides a built-in inherits that built-in's slot (CONTRACTS-V2 §7.1).
inline constexpr uint8_t SANYANG_BUILT_IN_INDEX = 0;
inline constexpr uint8_t MOGANSHAN_BUILT_IN_INDEX = 1;
inline constexpr uint16_t SANYANG_WHITE_YARDS[] = {310, 470, 122, 265, 490, 153, 389, 371, 340,
                                                   360, 360, 495, 150, 332, 350, 370, 156, 470};

inline constexpr GolfCourse GOLF_BUILT_IN_COURSES[] = {
    // Par and yardages come from the owner's scorecard (Blue 3196/3270/6466, White 2910/3043/5953, par
    // 36/36/72); stroke indexes come from his separate prose course guide.
    {"Sanyang Golf Club",
     "Blue",
     18,
     {4, 5, 3, 4, 5, 3, 4, 4, 4, 4, 4, 5, 3, 4, 4, 4, 3, 5},
     {325, 510, 144, 290, 510, 170, 427, 430, 390, 400, 395, 520, 175, 365, 375, 385, 165, 490},
     {15, 7, 13, 11, 9, 17, 3, 1, 5, 6, 12, 18, 16, 10, 14, 2, 4, 8},
     true,
     true},
    // Par and yardages come from the owner's Blue-tee scorecard (out 36/3100, in
    // 36/3132, total 72/6232). Stroke indexes were not supplied, so hasSi is false and
    // the si array is left zeroed rather than guessed.
    {"MoganShan Gowin",
     "Blue",
     18,
     {4, 3, 4, 4, 5, 3, 4, 5, 4, 4, 5, 3, 4, 4, 4, 3, 4, 5},
     {318, 137, 326, 403, 512, 168, 326, 530, 380, 416, 502, 150, 285, 373, 388, 135, 352, 531},
     {},
     true,
     false},
    {"Pebble Beach", "Blue", 18, {4, 5, 4, 4, 3, 5, 3, 4, 4, 4, 4, 3, 4, 5, 4, 4, 3, 5}, {}, {}, false, false},
    {"Template course", "", 18, {}, {}, {}, false, false},
};

inline constexpr uint8_t GOLF_BUILT_IN_COURSE_COUNT = sizeof(GOLF_BUILT_IN_COURSES) / sizeof(GOLF_BUILT_IN_COURSES[0]);

template <typename T>
constexpr uint16_t golfBuiltInSum(const T* values, const uint8_t begin, const uint8_t end) {
  uint16_t total = 0;
  for (uint8_t index = begin; index < end; ++index) total += values[index];
  return total;
}

static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].par, 9, 18) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].par, 0, 18) == 72);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards, 0, 9) == 3196);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards, 9, 18) == 3270);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards, 0, 18) == 6466);
static_assert(golfBuiltInSum(SANYANG_WHITE_YARDS, 0, 9) == 2910);
static_assert(golfBuiltInSum(SANYANG_WHITE_YARDS, 9, 18) == 3043);
static_assert(golfBuiltInSum(SANYANG_WHITE_YARDS, 0, 18) == 5953);

static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].par, 9, 18) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].par, 0, 18) == 72);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].yards, 0, 9) == 3100);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].yards, 9, 18) == 3132);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[MOGANSHAN_BUILT_IN_INDEX].yards, 0, 18) == 6232);

inline bool golfResolveBuiltInTeeYardages(const int8_t builtInIndex, const TeeSelection tee,
                                           const uint16_t*& yards) {
  yards = nullptr;
  if (builtInIndex != SANYANG_BUILT_IN_INDEX) return false;

  if (tee == TeeSelection::Blue) {
    yards = GOLF_BUILT_IN_COURSES[SANYANG_BUILT_IN_INDEX].yards;
  } else if (tee == TeeSelection::White) {
    yards = SANYANG_WHITE_YARDS;
  }
  return yards != nullptr;
}

struct GolfTeeResolution {
  const uint16_t* yards = nullptr;
  bool hasYards = false;
};

// Resolves only tee rows supplied by this exact course source. Built-in-only
// alternates are considered before the course's canonical row; SD courses pass
// allowBuiltInAlternates=false even when they override a built-in table slot.
inline bool golfResolveTeeCourse(const GolfCourse& course, const int8_t builtInIndex,
                                 const bool allowBuiltInAlternates, const TeeSelection tee,
                                 GolfTeeResolution& resolved) {
  resolved = {};
  const char* teeName = tee == TeeSelection::Blue    ? "Blue"
                        : tee == TeeSelection::White ? "White"
                                                     : nullptr;
  if (teeName == nullptr) return false;

  const uint16_t* alternateYards = nullptr;
  const bool exactBuiltIn =
      builtInIndex >= 0 && builtInIndex < static_cast<int8_t>(GOLF_BUILT_IN_COURSE_COUNT) &&
      strcmp(course.courseName, GOLF_BUILT_IN_COURSES[builtInIndex].courseName) == 0;
  if (allowBuiltInAlternates && exactBuiltIn &&
      golfResolveBuiltInTeeYardages(builtInIndex, tee, alternateYards)) {
    resolved.yards = alternateYards;
    resolved.hasYards = true;
    return true;
  }

  // A course with neither a tee label nor yardage supports both domain choices
  // as selection-only values. Yardage data without a label cannot truthfully be
  // assigned to either tee, and noncanonical labels are never guessed.
  if (course.tees[0] == '\0') {
    return !course.hasYards;
  }
  if (strcmp(course.tees, teeName) != 0) return false;
  resolved.yards = course.hasYards ? course.yards : nullptr;
  resolved.hasYards = course.hasYards;
  return true;
}
