#include "GolfRoundStore.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "GolfJson.h"
#include "GolfPaths.h"
#include "GolfRoundDecode.h"
#include "GolfRoundRepairLog.h"

namespace {

bool stateStringsValid(const GolfRound& round) {
  if (round.courseName[0] == '\0' || memchr(round.courseName, '\0', sizeof(round.courseName)) == nullptr ||
      strpbrk(round.courseName, "\r\n") != nullptr || !golfJsonValidUtf8(round.courseName)) {
    return false;
  }
  for (const GolfPlayer& player : round.players) {
    if (player.name[0] == '\0' || memchr(player.name, '\0', sizeof(player.name)) == nullptr ||
        strpbrk(player.name, "\r\n") != nullptr || !golfJsonValidUtf8(player.name)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool GolfRoundStore::saveToFile() const {
  if (!isArchived() && !stateStringsValid(round)) {
    LOG_ERR("GOLF", "Refused multiline or invalid golf state text");
    return false;
  }
  if (!Storage.ensureDirectoryExists("/golf")) {
    LOG_ERR("GOLF", "Failed to create /golf");
    return false;
  }
  return PersistableStore<GolfRoundStore>::saveToFile();
}

bool GolfRoundStore::loadFromFile() {
  if (isArchived()) {
    LOG_ERR("GOLF", "Refused to load state over committed archive marker");
    return false;
  }
  return PersistableStore<GolfRoundStore>::loadFromFile();
}

bool GolfRoundStore::clear() {
  std::lock_guard<std::mutex> lock(storeMutex);
  if (Storage.exists(getFilePath()) && !Storage.remove(getFilePath())) {
    LOG_ERR("GOLF", "Failed to clear %s", getFilePath());
    return false;
  }
  round = {};
  initializeGolfPlayerDefaults(round);
  clearGolfArchiveMarker(archiveMarker);
  return true;
}

bool GolfRoundStore::markArchivedAs(const char* filename) {
  if (!setGolfArchiveMarker(archiveMarker, filename)) {
    LOG_ERR("GOLF", "Invalid archived filename");
    return false;
  }
  // Keep the in-memory marker even if serialization or SD I/O fails. The
  // committed score must remain immutable and a retry may perform cleanup only.
  return saveToFile();
}

void GolfRoundStore::toJson(JsonDocument& doc) const {
  if (isArchived()) {
    doc["archivedAs"] = archivedFilename();
    return;
  }

  char date[GOLF_DATE_BUFFER_SIZE];
  doc["v"] = 4;
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    doc["date"] = date;
  } else {
    doc["date"] = nullptr;
  }
  doc["course"] = round.courseName;
  doc["holes"] = round.holeCount;
  doc["currentHole"] = round.currentHole;
  doc["currentPlayer"] = round.currentPlayer;
  golfAddJsonHoleArray(doc, "par", round.par, round.holeCount);
  doc["hasSi"] = round.hasSi;
  golfAddJsonHoleArray(doc, "si", round.si, round.holeCount, !round.hasSi);
  golfAddJsonPlayers(doc, round);
}

bool GolfRoundStore::fromJson(const JsonVariantConst doc) {
  GolfArchiveMarker loadedMarker{};
  // Keep the live commit authority even if a load raced the wrapper check.
  if (isArchived() && doc["archivedAs"].isNull()) {
    LOG_ERR("GOLF", "Rejected unmarked state over committed archive marker");
    return false;
  }
  // The commit marker wins even when every round field is corrupt or allocation
  // fails; resuming an already archived round would duplicate it.
  if (!doc["archivedAs"].isNull()) {
    if (!doc["archivedAs"].is<const char*>() ||
        !setGolfArchiveMarker(loadedMarker, doc["archivedAs"].as<const char*>())) {
      LOG_ERR("GOLF", "Rejected invalid archivedAs marker");
      return false;
    }
    round = {};
    initializeGolfPlayerDefaults(round);
    archiveMarker = loadedMarker;
    return true;
  }

  // Transactional decode needs a second 906-byte round. Heap staging avoids a
  // task-stack overflow and leaves the live member untouched on parse failure.
  auto loaded = makeUniqueNoThrow<GolfRound>();
  if (!loaded) {
    LOG_ERR("GOLF", "OOM: GolfRound state decode (%u bytes)", static_cast<unsigned>(sizeof(GolfRound)));
    return false;
  }
  GolfValidationResult validation{};
  const GolfRoundDecodeStatus status = golfDecodeRoundJson(doc, true, *loaded, validation);
  if (status != GolfRoundDecodeStatus::Ok) {
    if ((doc["v"] | 0) == 1) {
      LOG_ERR("GOLF", "Rejected v1 state: in100 semantics changed in v2");
    } else {
      LOG_ERR("GOLF", "Rejected invalid state: decode status %u", static_cast<unsigned>(status));
    }
    return false;
  }
  golfLogRoundRepairs(*loaded, validation);
  round = *loaded;
  archiveMarker = loadedMarker;
  return true;
}

#endif
