#pragma once

#include <cstdint>

#include "GolfRound.h"

struct GolfCourse {
  char courseName[40];
  char tees[12];
  uint8_t holeCount;
  uint8_t par[GolfRound::MAX_HOLES];
  uint16_t yards[GolfRound::MAX_HOLES];
  uint8_t si[GolfRound::MAX_HOLES];
  bool hasYards;
  bool hasSi;
};

static_assert(sizeof(GolfCourse) == 128);
