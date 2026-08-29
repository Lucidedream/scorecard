#include "GolfRoundFile.h"

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <PersistableStore.h>

#include "GolfJson.h"
#include "GolfPaths.h"
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

  GolfRound loaded{};
  const int version = doc["v"] | 0;
  if (version == 1) {
    LOG_ERR("GOLF", "Rejected v1 round file: in100 semantics changed in v2");
    return false;
  }
  const int holes = doc["holes"] | 0;
  bool validDate = doc["date"].isNull();
  if (!validDate && doc["date"].is<const char*>()) {
    validDate = golfParseDate(doc["date"].as<const char*>(), loaded.dateYmd);
  }
  if (holes < 0 || holes > UINT8_MAX || !validDate ||
      !golfReadJsonString(doc["course"], loaded.courseName, sizeof(loaded.courseName)) ||
      !golfReadJsonString(doc["tees"], loaded.tees, sizeof(loaded.tees))) {
    LOG_ERR("GOLF", "Rejected invalid round metadata: %s", path);
    return false;
  }

  GolfRoundColumnLengths lengths{};
  lengths.expectYards = false;  // completed-round schema omits yards
  if (!golfReadJsonHoleArray(doc["par"], loaded.par, GolfRound::MAX_HOLES, UINT8_MAX, lengths.par) ||
      !golfReadJsonHoleArray(doc["putts"], loaded.putts, GolfRound::MAX_HOLES, 99, lengths.putts) ||
      !golfReadJsonHoleArray(doc["in100"], loaded.in100, GolfRound::MAX_HOLES, 99, lengths.in100) ||
      !golfReadJsonHoleArray(doc["out100"], loaded.out100, GolfRound::MAX_HOLES, 99, lengths.out100)) {
    LOG_ERR("GOLF", "Rejected invalid round arrays: %s", path);
    return false;
  }

  GolfValidationResult validation{};
  const GolfRoundDecodeStatus status = golfCheckRound(loaded, version, holes, /*currentHole=*/0, lengths, validation);
  if (status != GolfRoundDecodeStatus::Ok) {
    LOG_ERR("GOLF", "Rejected round file (decode status %u): %s", static_cast<unsigned>(status), path);
    return false;
  }

  golfLogRoundRepairs(loaded, validation);
  out = loaded;
  return true;
}

#endif
