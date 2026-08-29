#pragma once

#include <cstdint>

struct GolfRound {
  static constexpr uint8_t MAX_HOLES = 18;

  char courseName[40];
  char tees[12];
  uint16_t dateYmd;
  uint8_t holeCount;
  uint8_t currentHole;

  uint8_t par[MAX_HOLES];
  uint16_t yards[MAX_HOLES];
  uint8_t putts[MAX_HOLES];
  uint8_t in100[MAX_HOLES];
  uint8_t out100[MAX_HOLES];
};

static_assert(sizeof(GolfRound) == 164);
