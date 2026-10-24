#pragma once

#include <cstdint>

#include "GolfRound.h"

inline constexpr uint8_t GOLF_MAX_TEES = 6;

struct GolfNine {
  char name[GOLF_NINE_NAME_CAPACITY];
  uint8_t par[GOLF_HOLES_PER_NINE];
  uint8_t si[GOLF_HOLES_PER_NINE];
  bool hasSi;
};

struct GolfCourseTee {
  char name[GOLF_TEE_CAPACITY];
  bool hasYards;
  uint16_t yards[GOLF_MAX_NINES][GOLF_HOLES_PER_NINE];  // Indexed by course nine, then hole within the nine.
};

struct GolfCourse {
  char courseName[40];
  uint8_t nineCount;
  uint8_t teeCount;
  GolfNine nines[GOLF_MAX_NINES];
  GolfCourseTee tees[GOLF_MAX_TEES];
  char mapsDir[16]{};
};

struct GolfNineSelection {
  uint8_t front = 0;
  uint8_t back = 1;  // GOLF_NO_NINE for a nine-hole round.
};

static_assert(sizeof(GolfCourse) == 698);
