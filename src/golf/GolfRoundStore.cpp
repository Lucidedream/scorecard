#include "GolfRoundStore.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "GolfPaths.h"
#include "GolfValidate.h"

namespace {

template <typename T>
void addArray(JsonDocument& doc, const char* name, const T* values, uint8_t count) {
  JsonArray array = doc[name].to<JsonArray>();
  for (uint8_t hole = 0; hole < count; ++hole) {
    array.add(values[hole]);
  }
}

bool copyString(JsonVariantConst value, char* output, size_t capacity) {
  if (!value.is<const char*>()) {
    return false;
  }
  const char* source = value.as<const char*>();
  const size_t length = strlen(source);
  if (length >= capacity) {
    return false;
  }
  memcpy(output, source, length + 1);
  return true;
}

template <typename T>
bool readArray(JsonVariantConst value, T* output, uint8_t count, uint16_t maximum) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.isNull() || array.size() != count) {
    return false;
  }
  for (uint8_t hole = 0; hole < count; ++hole) {
    if (!array[hole].is<int>()) {
      return false;
    }
    const int item = array[hole].as<int>();
    if (item < 0 || item > maximum) {
      return false;
    }
    output[hole] = static_cast<T>(item);
  }
  return true;
}

void logRepairs(const GolfRound& round, const GolfValidationResult& result) {
  if (result.currentHoleReset) {
    LOG_ERR("GOLF", "Repaired current hole to 1");
  }
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (result.holePuttsRepaired(hole)) {
      LOG_ERR("GOLF", "Repaired hole %u putts to %u", hole + 1, round.putts[hole]);
    }
    if (result.holeIn100Repaired(hole)) {
      LOG_ERR("GOLF", "Repaired hole %u in100 to %u", hole + 1, round.in100[hole]);
    }
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
  if (!golfFormatDate(round.dateYmd, date, sizeof(date))) {
    date[0] = '\0';
  }
  doc["v"] = 1;
  doc["date"] = date;
  doc["course"] = round.courseName;
  doc["tees"] = round.tees;
  doc["holes"] = round.holeCount;
  doc["currentHole"] = round.currentHole;
  addArray(doc, "par", round.par, round.holeCount);
  addArray(doc, "yards", round.yards, round.holeCount);
  addArray(doc, "strokes", round.strokes, round.holeCount);
  addArray(doc, "putts", round.putts, round.holeCount);
  addArray(doc, "in100", round.in100, round.holeCount);
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
  const int holes = doc["holes"] | 0;
  const int currentHole = doc["currentHole"] | -1;
  const char* date = doc["date"] | "";
  if (version != 1 || holes < 0 || holes > UINT8_MAX || currentHole < 0 || !golfParseDate(date, loaded.dateYmd) ||
      !copyString(doc["course"], loaded.courseName, sizeof(loaded.courseName)) ||
      !copyString(doc["tees"], loaded.tees, sizeof(loaded.tees))) {
    LOG_ERR("GOLF", "Rejected invalid state metadata");
    return false;
  }
  loaded.holeCount = static_cast<uint8_t>(holes);
  if (!validateGolfRound(loaded).valid) {
    LOG_ERR("GOLF", "Rejected unsupported hole count %u", loaded.holeCount);
    return false;
  }
  if (!readArray(doc["par"], loaded.par, loaded.holeCount, UINT8_MAX) ||
      !readArray(doc["yards"], loaded.yards, loaded.holeCount, UINT16_MAX) ||
      !readArray(doc["strokes"], loaded.strokes, loaded.holeCount, UINT8_MAX) ||
      !readArray(doc["putts"], loaded.putts, loaded.holeCount, UINT8_MAX) ||
      !readArray(doc["in100"], loaded.in100, loaded.holeCount, UINT8_MAX)) {
    LOG_ERR("GOLF", "Rejected invalid state arrays");
    return false;
  }
  loaded.currentHole = currentHole >= holes ? loaded.holeCount : static_cast<uint8_t>(currentHole);
  const GolfValidationResult validation = validateGolfRound(loaded);
  logRepairs(loaded, validation);
  round = loaded;
  archiveMarker = loadedMarker;
  return true;
}

#endif
