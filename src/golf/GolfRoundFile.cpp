#include "GolfRoundFile.h"

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <PersistableStore.h>

#include "GolfJson.h"
#include "GolfRoundDecode.h"
#include "GolfRoundRepairLog.h"

bool loadGolfRoundFile(const char* path, GolfRound& out) {
  if (!Storage.exists(path)) {
    LOG_INF("GOLF", "Round file absent, using CSV summary: %s", path);
    return false;
  }

  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(path, doc)) {
    LOG_ERR("GOLF", "Round file unreadable or malformed: %s", path);
    return false;
  }

  // A 906-byte transactional staging value is unsafe on the activity task
  // stack. One checked heap allocation keeps `out` unchanged on every failure.
  auto loaded = makeUniqueNoThrow<GolfRound>();
  if (!loaded) {
    LOG_ERR("GOLF", "OOM: GolfRound file decode (%u bytes)", static_cast<unsigned>(sizeof(GolfRound)));
    return false;
  }
  GolfValidationResult validation{};
  const GolfRoundDecodeStatus status = golfDecodeRoundJson(doc.as<JsonVariantConst>(), false, *loaded, validation);
  if (status != GolfRoundDecodeStatus::Ok) {
    if ((doc["v"] | 0) == 1) {
      LOG_ERR("GOLF", "Rejected v1 round file: in100 semantics changed in v2");
    } else {
      LOG_ERR("GOLF", "Rejected round file (decode status %u): %s", static_cast<unsigned>(status), path);
    }
    return false;
  }

  golfLogRoundRepairs(*loaded, validation);
  out = *loaded;
  return true;
}

#endif
