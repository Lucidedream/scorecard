#pragma once

#include "GolfRound.h"

class RoundArchive {
 public:
  static bool archive(const GolfRound& round);
  // Removes the index row through a staged verified rewrite before unlinking
  // the round JSON. A failed JSON unlink leaves only an invisible orphan.
  static bool remove(const char* filename);
};
