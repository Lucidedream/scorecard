#include "GolfNavigation.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "GolfHomeActivity.h"
#include "GolfMessageActivity.h"
#include "GolfScoringActivity.h"
#include "GolfSetupActivity.h"
#include "GolfStrings.h"
#include "activities/ActivityManager.h"
#include "golf/GolfRoundStore.h"

namespace {
bool roundDirty = false;

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
  roundDirty = false;
  return replaceGolfActivity<GolfHomeActivity>(manager, renderer, mappedInput, "home");
}

bool openGolfSetup(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return replaceGolfActivity<GolfSetupActivity>(manager, renderer, mappedInput, "setup");
}

bool openGolfScoring(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  roundDirty = false;
  return replaceGolfActivity<GolfScoringActivity>(manager, renderer, mappedInput, "scoring");
}

bool resumeGolfRound(ActivityManager& manager, GfxRenderer& renderer, MappedInputManager& mappedInput) {
  if (!Storage.exists(GolfRoundStore::getFilePath())) return false;
  if (!GOLF_ROUND_STORE.loadFromFile()) {
    auto message = makeUniqueNoThrow<GolfMessageActivity>(renderer, mappedInput, GolfStrings::APP_TITLE,
                                                          GolfStrings::RESUME_ERROR);
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

void markGolfRoundDirty() { roundDirty = true; }

void clearGolfRoundDirty() { roundDirty = false; }

bool isGolfRoundDirty() { return roundDirty; }

bool flushGolfRoundIfDirty() {
  if (!roundDirty) return true;
  const uint8_t holes = GOLF_ROUND_STORE.getRound().holeCount;
  if (holes != 9 && holes != 18) {
    roundDirty = false;
    return true;
  }
  if (!GOLF_ROUND_STORE.saveToFile()) return false;
  roundDirty = false;
  return true;
}

bool flushGolfRoundForSleep() { return flushGolfRoundIfDirty(); }

#endif
