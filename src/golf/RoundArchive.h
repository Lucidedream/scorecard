#pragma once

#include <cstdint>

#include "GolfRound.h"

class GolfIndexMigrator;

enum class RoundArchiveResult : uint8_t {
  FailedBeforeCommit,
  CommittedCleanupPending,
  Complete,
};

constexpr bool golfArchiveCommitted(const RoundArchiveResult result) {
  return result != RoundArchiveResult::FailedBeforeCommit;
}

class RoundArchive {
 public:
  // Repairs interrupted index publication before any caller reads or mutates
  // it. The caller retains reusable scratch across independent reads.
  static bool recoverIndex(GolfIndexMigrator& scratch);
  static RoundArchiveResult archive(const GolfRound& round);
  // Removes all 1..4 stable-slot rows through a staged verified rewrite, then
  // unlinks the shared JSON. Once rows are absent, retries only clean artifacts.
  static bool remove(const char* filename);
};
