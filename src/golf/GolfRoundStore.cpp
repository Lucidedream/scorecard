#include "GolfRoundStore.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include "GolfJson.h"
#include "GolfPaths.h"
#include "GolfRoundDecode.h"
#include "GolfRoundRepairLog.h"
#include "GolfValidate.h"

namespace {

template <typename T>
void addArray(JsonDocument& doc, const char* name, const T* values, uint8_t count) {
  JsonArray array = doc[name].to<JsonArray>();
  for (uint8_t hole = 0; hole < count; ++hole) {
    array.add(values[hole]);
  }
}

}  // namespace

bool GolfRoundStore::saveToFile() const {
  if (!Storage.ensureDirectoryExists("/golf")) {
    LOG_ERR("GOLF", "Failed to create /golf");
    return false;
  }
  return PersistableStore<GolfRoundStore>::saveToFile();
}

bool GolfRoundStore::clear() {
  std::lock_guard<std::mutex> lock(storeMutex);
  if (Storage.exists(getFilePath()) && !Storage.remove(getFilePath())) {
    LOG_ERR("GOLF", "Failed to clear %s", getFilePath());
    return false;
  }
  round = {};
  clearGolfArchiveMarker(archiveMarker);
  return true;
}

bool GolfRoundStore::markArchivedAs(const char* filename) {
  if (!setGolfArchiveMarker(archiveMarker, filename)) {
    LOG_ERR("GOLF", "Invalid archived filename");
    return false;
  }
  return saveToFile();
}

void GolfRoundStore::toJson(JsonDocument& doc) const {
  char date[GOLF_DATE_BUFFER_SIZE];
  doc["v"] = 2;
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    doc["date"] = date;
  } else {
    doc["date"] = nullptr;
  }
  doc["course"] = round.courseName;
  doc["tees"] = round.tees;
  doc["holes"] = round.holeCount;
  doc["currentHole"] = round.currentHole;
  addArray(doc, "par", round.par, round.holeCount);
  addArray(doc, "yards", round.yards, round.holeCount);
  addArray(doc, "putts", round.putts, round.holeCount);
  addArray(doc, "in100", round.in100, round.holeCount);
  addArray(doc, "out100", round.out100, round.holeCount);
  if (isArchived()) {
    doc["archivedAs"] = archivedFilename();
  }
}

bool GolfRoundStore::fromJson(JsonVariantConst doc) {
  GolfRound loaded{};
  GolfArchiveMarker loadedMarker{};
  if (!doc["archivedAs"].isNull()) {
    if (!doc["archivedAs"].is<const char*>() ||
        !setGolfArchiveMarker(loadedMarker, doc["archivedAs"].as<const char*>())) {
      LOG_ERR("GOLF", "Rejected invalid archivedAs marker");
      return false;
    }
    round = {};
    archiveMarker = loadedMarker;
    return true;
  }
  const int version = doc["v"] | 0;
  if (version == 1) {
    LOG_ERR("GOLF", "Rejected v1 state: in100 semantics changed in v2");
    return false;
  }
  const int holes = doc["holes"] | 0;
  const int currentHole = doc["currentHole"] | -1;
  bool validDate = doc["date"].isNull();
  if (!validDate && doc["date"].is<const char*>()) {
    validDate = golfParseDate(doc["date"].as<const char*>(), loaded.dateYmd);
  }
  if (holes < 0 || holes > UINT8_MAX || currentHole < 0 || !validDate ||
      !golfReadJsonString(doc["course"], loaded.courseName, sizeof(loaded.courseName)) ||
      !golfReadJsonString(doc["tees"], loaded.tees, sizeof(loaded.tees))) {
    LOG_ERR("GOLF", "Rejected invalid state metadata");
    return false;
  }
  GolfRoundColumnLengths lengths{};
  lengths.expectYards = true;
  if (!golfReadJsonHoleArray(doc["par"], loaded.par, GolfRound::MAX_HOLES, UINT8_MAX, lengths.par) ||
      !golfReadJsonHoleArray(doc["yards"], loaded.yards, GolfRound::MAX_HOLES, UINT16_MAX, lengths.yards) ||
      !golfReadJsonHoleArray(doc["putts"], loaded.putts, GolfRound::MAX_HOLES, 99, lengths.putts) ||
      !golfReadJsonHoleArray(doc["in100"], loaded.in100, GolfRound::MAX_HOLES, 99, lengths.in100) ||
      !golfReadJsonHoleArray(doc["out100"], loaded.out100, GolfRound::MAX_HOLES, 99, lengths.out100)) {
    LOG_ERR("GOLF", "Rejected invalid state arrays");
    return false;
  }
  GolfValidationResult validation{};
  const GolfRoundDecodeStatus status = golfCheckRound(loaded, version, holes, currentHole, lengths, validation);
  if (status != GolfRoundDecodeStatus::Ok) {
    LOG_ERR("GOLF", "Rejected invalid state: decode status %u", static_cast<unsigned>(status));
    return false;
  }
  golfLogRoundRepairs(loaded, validation);
  round = loaded;
  archiveMarker = loadedMarker;
  return true;
}

#endif
