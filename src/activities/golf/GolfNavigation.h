#pragma once

#include <cstdint>

class ActivityManager;
class Activity;
class GfxRenderer;
class MappedInputManager;
struct GolfNineSelection;
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
constexpr GolfHomeEntryDecision golfDecideHomeEntry(const bool archiveMarkedAtEntry, const bool cleanupSucceeded,
                                                    const bool stateExists, const bool stateLoaded,
                                                    const uint8_t holeCount) {
  if (archiveMarkedAtEntry) return {false, !cleanupSucceeded, false, cleanupSucceeded, false};
  const bool resumable = stateLoaded && (holeCount == 9 || holeCount == 18);
  return {stateExists, false, resumable, true, stateExists && !stateLoaded};
}

enum class GolfPickerMode : uint8_t { Browse, Play };
enum class GolfPickerLevel : uint8_t { Hub, Regions, Courses, MyCourses, Recent };

struct GolfPickerContext {
  GolfPickerMode mode = GolfPickerMode::Play;
  GolfPickerLevel level = GolfPickerLevel::Hub;
  uint8_t region = 0;
  bool byRating = true;
  uint16_t selected = 0;
  uint16_t parentSelected = 0;
  bool skipHub = false;
};

GolfPickerContext golfSetupReturnContext();
void golfSetSetupReturnContext(GolfPickerContext context);
bool openGolfNewRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfPickerParent(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool startGolfCourse(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                     const GolfCourseFile& file, uint8_t nineCount);
void golfSetInfoReturn(const GolfCourseFile& file, uint16_t selected);
GolfHomeEntryDecision golfReadHomeEntry();
constexpr bool golfShowPlayCourse(GolfHomeEntryDecision decision) { return decision.showNew; }

bool openGolfHome(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfNineSelect(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                        const GolfCourseFile& courseFile, GolfNineSelection selection);
bool openGolfPlayerSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput,
                         const GolfCourseFile& courseFile, GolfNineSelection selection);
bool openGolfCourseMap(Activity& parent, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool openGolfScoring(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool resumeGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool finishGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput);
bool flushGolfRoundForSleep();
bool flushGolfRoundIfDirty();
void markGolfRoundDirty();
void markGolfArchiveCleanupPending();
void clearGolfRoundDirty();
bool isGolfRoundDirty();
bool hasGolfRoundSaveFailed();
