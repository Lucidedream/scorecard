#pragma once

#include <cstdint>

class ActivityManager;
class GfxRenderer;
class MappedInputManager;
struct GolfCourse;
struct GolfCourseFile;

// Front previous/next follows the rendered orientation. Side Up/Down callers
// keep their row semantics and must not pass through this helper.
constexpr int8_t golfFrontNavDelta(const bool directionSwapped, const bool leftButton) {
  return leftButton == directionSwapped ? 1 : -1;
}

struct GolfHomeEntryDecision {
  bool loadState;
  bool cleanupOnly;
  bool showResume;
  bool showNew;
  bool stateError;
};

// A live archive marker is authoritative over state.json. If cleanup fails,
// Home offers only read-only history surfaces and retries cleanup on re-entry.
constexpr GolfHomeEntryDecision golfDecideHomeEntry(const bool archiveMarkedAtEntry,
                                                    const bool cleanupSucceeded, const bool stateExists,
                                                    const bool stateLoaded, const uint8_t holeCount) {
  if (archiveMarkedAtEntry) return {false, !cleanupSucceeded, false, cleanupSucceeded, false};
  const bool resumable = stateLoaded && (holeCount == 9 || holeCount == 18);
  return {stateExists, false, resumable, true, stateExists && !stateLoaded};
}

bool openGolfHome(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfPlayerSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                         const GolfCourseFile& courseFile, const GolfCourse& course);
bool openGolfScoring(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool resumeGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool flushGolfRoundForSleep();
bool flushGolfRoundIfDirty();
void markGolfRoundDirty();
void markGolfArchiveCleanupPending();
void clearGolfRoundDirty();
bool isGolfRoundDirty();
bool hasGolfRoundSaveFailed();
