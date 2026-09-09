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
  // Optional output must have GOLF_ROUND_FILENAME_BUFFER_SIZE bytes. Published
  // only after commit, including the cleanup-pending recovery path.
  static RoundArchiveResult archive(const GolfRound& round, char* committedFilename = nullptr);
  // Removes all 1..4 stable-slot rows through a staged verified rewrite, then
  // unlinks the shared JSON. Once rows are absent, retries only clean artifacts.
  static bool remove(const char* filename);
  // Removes one enabled player from a shared multiplayer round: disables the slot
  // in the round file and shrinks its index group to N-1 rows through the same
  // transactional order as an edit. Routes to remove() when the slot is the
  // round's last enabled player (CONTRACTS-V2 §36).
  static bool removePlayer(const char* filename, uint8_t playerSlot);
};
