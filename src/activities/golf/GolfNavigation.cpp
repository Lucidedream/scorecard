#include "GolfNavigation.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <atomic>
#include <cstdio>

#include "GolfHomeActivity.h"
#include "GolfMessageActivity.h"
#include "GolfPlayerSetupActivity.h"
#include "GolfRoundSummaryActivity.h"
#include "GolfScoringActivity.h"
#include "GolfSetupActivity.h"
#include "activities/ActivityManager.h"
#include "golf/GolfPaths.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfStats.h"
#include "golf/RoundArchive.h"

namespace {
bool roundDirty = false;
std::atomic_bool roundSaveFailed{false};

bool makeSummaryEntry(const GolfRound& round, GolfHistoryEntry& entry) {
  const uint8_t playerSlot =
      round.currentPlayer < GolfRound::MAX_PLAYERS && golfPlayerIsEnabled(round.players[round.currentPlayer])
          ? round.currentPlayer
          : golfFirstEnabledPlayer(round);
  if (playerSlot == GolfRound::NO_PLAYER) return false;

  const GolfPlayer& player = round.players[playerSlot];
  entry = {};
  snprintf(entry.course, sizeof(entry.course), "%s", round.courseName);
  snprintf(entry.playerName, sizeof(entry.playerName), "%s", player.name);
  entry.strokes = golfScore(round, player.score);
  entry.par = golfParTotal(round, player.score);
  entry.putts = golfPuttsTotal(round, player.score);
  entry.in100 = golfIn100Total(round, player.score);
  entry.out100 = golfLongTotal(round, player.score);
  entry.hazards = golfHazardsForRound(player.score, round.holeCount);
  entry.obs = golfObsForRound(player.score, round.holeCount);
  entry.holes = round.holeCount;
  entry.playerSlot = playerSlot;
  entry.penaltiesRecorded = true;
  entry.dateYmd = round.dateYmd;
  return true;
}

template <typename T>
bool replaceGolfActivity(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                         const char* name) {
  auto activity = makeUniqueNoThrow<T>(renderer, mappedInput);
  if (!activity) {
    LOG_ERR("GOLF", "OOM: %s activity", name);
    return false;
  }
  manager.replaceActivity(std::move(activity));
  return true;
}
}  // namespace

bool openGolfHome(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // A committed archive whose marker/state cleanup failed must retain its RAM
  // marker and retry state while the caller leaves the scoring screen.
  if (!GOLF_ROUND_STORE.isArchived()) {
    roundDirty = false;
    roundSaveFailed.store(false, std::memory_order_relaxed);
  }
  return replaceGolfActivity<GolfHomeActivity>(manager, renderer, mappedInput, "home");
}

bool openGolfSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return replaceGolfActivity<GolfSetupActivity>(manager, renderer, mappedInput, "setup");
}

bool openGolfPlayerSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                         const GolfCourseFile& courseFile, const GolfCourse& course) {
  auto activity = makeUniqueNoThrow<GolfPlayerSetupActivity>(renderer, mappedInput, courseFile, course);
  if (!activity) {
    LOG_ERR("GOLF", "OOM: player setup activity");
    return false;
  }
  manager.replaceActivity(std::move(activity));
  return true;
}

bool openGolfScoring(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  if (GOLF_ROUND_STORE.isArchived()) {
    LOG_ERR("GOLF", "Refused to score a committed archive");
    return false;
  }
  roundDirty = false;
  roundSaveFailed.store(false, std::memory_order_relaxed);
  return replaceGolfActivity<GolfScoringActivity>(manager, renderer, mappedInput, "scoring");
}

bool resumeGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // A failed marker/state cleanup leaves the RAM store authoritative. Never
  // reload the older unmarked state file over it or a retry could duplicate the
  // already committed group.
  if (GOLF_ROUND_STORE.isArchived()) {
    if (!GOLF_ROUND_STORE.clear()) LOG_ERR("GOLF", "Could not clear committed archive state");
    return false;
  }
  if (!Storage.exists(GolfRoundStore::getFilePath())) return false;
  if (!GOLF_ROUND_STORE.loadFromFile()) {
    auto message = makeUniqueNoThrow<GolfMessageActivity>(renderer, mappedInput, tr(STR_GOLF_APP_TITLE),
                                                          tr(STR_GOLF_RESUME_ERROR));
    if (!message) {
      LOG_ERR("GOLF", "OOM: resume error activity");
      return false;
    }
    manager.replaceActivity(std::move(message));
    return true;
  }
  if (GOLF_ROUND_STORE.isArchived()) {
    if (!GOLF_ROUND_STORE.clear()) LOG_ERR("GOLF", "Could not clear archived state marker");
    return false;
  }
  const uint8_t holes = GOLF_ROUND_STORE.getRound().holeCount;
  if (holes != 9 && holes != 18) return false;
  return openGolfScoring(manager, renderer, mappedInput);
}

bool finishGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  GolfHistoryEntry summaryEntry{};
  char committedFilename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  const bool hasSummary = makeSummaryEntry(round, summaryEntry);
  const RoundArchiveResult result = RoundArchive::archive(round, committedFilename);
  if (result == RoundArchiveResult::FailedBeforeCommit) return false;

  // The round file and index group are durable. Never leave the user on a
  // mutable scoring surface just because marker deletion needs another try.
  if (result == RoundArchiveResult::CommittedCleanupPending) {
    markGolfArchiveCleanupPending();
  } else {
    clearGolfRoundDirty();
  }
  if (hasSummary) {
    auto summary =
        makeUniqueNoThrow<GolfRoundSummaryActivity>(renderer, mappedInput, summaryEntry, true, committedFilename);
    if (summary) {
      manager.replaceActivity(std::move(summary));
      return true;
    }
    LOG_ERR("GOLF", "OOM: finished round summary");
  }
  openGolfHome(manager, renderer, mappedInput);
  return true;
}

void markGolfRoundDirty() { roundDirty = true; }

void markGolfArchiveCleanupPending() {
  roundDirty = true;
  roundSaveFailed.store(true, std::memory_order_relaxed);
}

void clearGolfRoundDirty() {
  roundDirty = false;
  roundSaveFailed.store(false, std::memory_order_relaxed);
}

bool isGolfRoundDirty() { return roundDirty; }

bool hasGolfRoundSaveFailed() { return roundSaveFailed.load(std::memory_order_relaxed); }

bool flushGolfRoundIfDirty() {
  if (!roundDirty) return true;
  const uint8_t holes = GOLF_ROUND_STORE.getRound().holeCount;
  if (!GOLF_ROUND_STORE.isArchived() && holes != 9 && holes != 18) {
    clearGolfRoundDirty();
    return true;
  }
  if (!GOLF_ROUND_STORE.saveToFile()) {
    roundSaveFailed.store(true, std::memory_order_relaxed);
    return false;
  }
  clearGolfRoundDirty();
  return true;
}

bool flushGolfRoundForSleep() { return flushGolfRoundIfDirty(); }

#endif
