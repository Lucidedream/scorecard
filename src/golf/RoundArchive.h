#pragma once

#include "GolfRound.h"

class RoundArchive {
 public:
  static bool archive(const GolfRound& round);
};
