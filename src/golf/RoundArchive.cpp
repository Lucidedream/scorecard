#include "RoundArchive.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "GolfCsv.h"
#include "GolfIndexMigrate.h"
#include "GolfPaths.h"
#include "GolfPenalty.h"
#include "GolfRoundStore.h"
#include "GolfStats.h"
#include "GolfValidate.h"

namespace {

constexpr char ROUNDS_DIRECTORY[] = "/golf/rounds";
constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";
constexpr char INDEX_NEW_PATH[] = "/golf/rounds/index.csv.new";
constexpr char INDEX_BAK_PATH[] = "/golf/rounds/index.csv.bak";

bool writeText(HalFile& file, const char* text) { return file.write(text, strlen(text)) == strlen(text); }

bool migrateSink(const char* data, size_t size, void* user) {
  auto* file = static_cast<HalFile*>(user);
  return file->write(data, size) == size;
}

// Streams `path` through `migrator` (already reset). Returns false on an I/O
// error; the migrator's sourceVersion()/dataRows() hold the result otherwise.
bool runMigrator(const char* path, GolfIndexMigrator& migrator, GolfIndexMigrateSink sink, void* sinkUser) {
  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) return false;
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) return false;
    if (!migrator.feed(chunk, static_cast<size_t>(bytesRead), sink, sinkUser)) break;
  }
  migrator.finish();
  return true;
}

// CONTRACTS-V2 §12.7: a one-time v2 -> v3 rewrite of index.csv, staged so a
// failure at any step leaves the original file untouched. The owner's only copy
// of every round he has played is never deleted or truncated before its
// replacement is written and verified.
//
// Failure semantics, step by step:
//   - cannot open the original for read        -> original untouched, return false
//   - cannot create index.csv.new              -> original untouched, return false
//   - write error while streaming the rewrite  -> index.csv.new removed, original untouched
//   - source was not v2 (already v3/unknown)   -> index.csv.new removed, original untouched, return true
//   - verify: new file unreadable / not v3 /
//     row count differs from the original      -> index.csv.new removed, original untouched
//   - rename index.csv -> index.csv.bak fails   -> index.csv.new removed, original still index.csv
//   - rename index.csv.new -> index.csv fails   -> index.csv.bak restored to index.csv (best effort),
//                                                 index.csv.new removed
//   - delete index.csv.bak fails               -> migration already succeeded; the stale .bak is
//                                                 harmless and only logged
bool migrateIndexToV3IfNeeded() {
  if (!Storage.exists(INDEX_PATH)) return true;
  if (Storage.exists(INDEX_NEW_PATH)) Storage.remove(INDEX_NEW_PATH);  // stale from an interrupted run

  HalFile staged = Storage.open(INDEX_NEW_PATH, O_WRONLY | O_CREAT | O_TRUNC);
  if (!staged) {
    LOG_ERR("GOLF", "index.csv migration: cannot create %s; left unchanged", INDEX_NEW_PATH);
    return false;
  }

  GolfIndexMigrator migrator;
  migrator.reset();
  const bool readOk = runMigrator(INDEX_PATH, migrator, &migrateSink, &staged);
  staged.flush();
  staged.close();

  if (!readOk) {
    Storage.remove(INDEX_NEW_PATH);
    LOG_ERR("GOLF", "index.csv migration: cannot read %s; left unchanged", INDEX_PATH);
    return false;
  }
  if (migrator.aborted()) {
    Storage.remove(INDEX_NEW_PATH);
    LOG_ERR("GOLF", "index.csv migration: write to %s failed; %s left unchanged", INDEX_NEW_PATH, INDEX_PATH);
    return false;
  }
  if (!migrator.needsMigration()) {
    Storage.remove(INDEX_NEW_PATH);
    return true;  // already v3, or a header we do not recognise -> nothing to do
  }

  // Verify the staged file with the same parser History reads it with, and only
  // proceed if it holds the v3 header and exactly the rows the rewrite emitted.
  const uint32_t expectedRows = migrator.dataRows();
  migrator.reset();
  const bool verifyOk = runMigrator(INDEX_NEW_PATH, migrator, nullptr, nullptr);
  if (!verifyOk || migrator.sourceVersion() != GolfIndexVersion::V3 || migrator.dataRows() != expectedRows) {
    Storage.remove(INDEX_NEW_PATH);
    LOG_ERR("GOLF", "index.csv migration: verify failed (rows %lu vs %lu); %s left unchanged",
            static_cast<unsigned long>(migrator.dataRows()), static_cast<unsigned long>(expectedRows), INDEX_PATH);
    return false;
  }

  if (Storage.exists(INDEX_BAK_PATH)) Storage.remove(INDEX_BAK_PATH);
  if (!Storage.rename(INDEX_PATH, INDEX_BAK_PATH)) {
    Storage.remove(INDEX_NEW_PATH);
    LOG_ERR("GOLF", "index.csv migration: could not set aside %s; left unchanged", INDEX_PATH);
    return false;
  }
  if (!Storage.rename(INDEX_NEW_PATH, INDEX_PATH)) {
    LOG_ERR("GOLF", "index.csv migration: swap failed; restoring original");
    if (!Storage.rename(INDEX_BAK_PATH, INDEX_PATH)) {
      LOG_ERR("GOLF", "index.csv migration: original preserved as %s", INDEX_BAK_PATH);
    }
    Storage.remove(INDEX_NEW_PATH);
    return false;
  }
  if (!Storage.remove(INDEX_BAK_PATH)) {
    LOG_ERR("GOLF", "index.csv migration: succeeded; stale %s left behind", INDEX_BAK_PATH);
  }
  LOG_INF("GOLF", "index.csv migrated to v3 (%lu rows)", static_cast<unsigned long>(expectedRows));
  return true;
}

bool writeJsonString(HalFile& file, const char* value) {
  if (!writeText(file, "\"")) {
    return false;
  }
  for (const auto* current = reinterpret_cast<const unsigned char*>(value); *current != 0; ++current) {
    char escaped[7];
    const char* text = escaped;
    switch (*current) {
      case '"':
        text = "\\\"";
        break;
      case '\\':
        text = "\\\\";
        break;
      case '\b':
        text = "\\b";
        break;
      case '\f':
        text = "\\f";
        break;
      case '\n':
        text = "\\n";
        break;
      case '\r':
        text = "\\r";
        break;
      case '\t':
        text = "\\t";
        break;
      default:
        if (*current < 0x20) {
          snprintf(escaped, sizeof(escaped), "\\u%04x", *current);
        } else {
          escaped[0] = static_cast<char>(*current);
          escaped[1] = '\0';
        }
        break;
    }
    if (!writeText(file, text)) {
      return false;
    }
  }
  return writeText(file, "\"");
}

template <typename T>
bool writeArray(HalFile& file, const T* values, uint8_t count) {
  if (!writeText(file, "[")) {
    return false;
  }
  for (uint8_t hole = 0; hole < count; ++hole) {
    char number[7];
    snprintf(number, sizeof(number), hole == 0 ? "%u" : ",%u", values[hole]);
    if (!writeText(file, number)) {
      return false;
    }
  }
  return writeText(file, "]");
}

bool writePenalties(HalFile& file, const GolfRound& round) {
  if (!writeText(file, "[")) return false;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (!writeText(file, hole == 0 ? "[" : ",[")) return false;
    const uint8_t count = round.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                              ? round.penaltyCount[hole]
                              : GolfRound::MAX_PENALTIES_PER_HOLE;
    for (uint8_t index = 0; index < count; ++index) {
      GolfPenaltyEvent event{};
      if (!golfPenaltyEventAt(round, hole, index, event)) continue;
      char pair[12];
      snprintf(pair, sizeof(pair), index == 0 ? "[%u,%u]" : ",[%u,%u]", static_cast<uint8_t>(event.field),
               static_cast<uint8_t>(event.kind));
      if (!writeText(file, pair)) return false;
    }
    if (!writeText(file, "]")) return false;
  }
  return writeText(file, "]");
}

bool writeCompletedRound(HalFile& file, const GolfRound& round) {
  char date[GOLF_DATE_BUFFER_SIZE];
  char metadata[32];
  if (!writeText(file, "{\"v\":3,\"date\":")) return false;
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    if (!writeJsonString(file, date)) return false;
  } else if (!writeText(file, "null")) {
    return false;
  }
  if (!writeText(file, ",\"course\":") || !writeJsonString(file, round.courseName) || !writeText(file, ",\"tees\":") ||
      !writeJsonString(file, round.tees)) {
    return false;
  }
  snprintf(metadata, sizeof(metadata), ",\"holes\":%u,\"par\":", round.holeCount);
  return writeText(file, metadata) && writeArray(file, round.par, round.holeCount) && writeText(file, ",\"putts\":") &&
         writeArray(file, round.putts, round.holeCount) && writeText(file, ",\"in100\":") &&
         writeArray(file, round.in100, round.holeCount) && writeText(file, ",\"out100\":") &&
         writeArray(file, round.out100, round.holeCount) && writeText(file, ",\"penalties\":") &&
         writePenalties(file, round) && writeText(file, "}\n");
}

bool appendIndex(const GolfRound& round, const char* filename) {
  // A v2 index must become v3 before a v3 row is appended, or the header would
  // no longer describe the rows (CONTRACTS-V2 §12.7). A failed migration is a
  // no-op and the mixed-row reader tolerates the result, so the append still
  // proceeds either way.
  if (!migrateIndexToV3IfNeeded()) {
    LOG_ERR("GOLF", "index.csv left at v2; appending a v3 row anyway (reader tolerates mixed rows)");
  }

  char date[GOLF_DATE_BUFFER_SIZE];
  char csv[GOLF_CSV_ROW_BUFFER_SIZE];
  date[0] = '\0';
  golfFormatDate(round.dateYmd, date, sizeof(date));
  const GolfIndexRowView row{date,
                             round.courseName,
                             round.holeCount,
                             golfScore(round),
                             golfParTotal(round),
                             golfPuttsTotal(round),
                             golfIn100Total(round),
                             golfLongTotal(round),
                             golfHazardsForRound(round),
                             golfObsForRound(round),
                             true,
                             filename};
  if (!golfFormatIndexRow(row, csv, sizeof(csv))) {
    LOG_ERR("GOLF", "Failed to format history row");
    return false;
  }

  const bool writeHeader = !Storage.exists(INDEX_PATH);
  HalFile index = Storage.open(INDEX_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (!index || (writeHeader && !writeText(index, GOLF_INDEX_HEADER)) || !writeText(index, csv)) {
    LOG_ERR("GOLF", "Failed to append %s", INDEX_PATH);
    return false;
  }
  index.flush();
  return true;
}

void logArchiveRepairs(const GolfRound& round, const GolfValidationResult& validation) {
  if (validation.currentHoleReset) {
    LOG_ERR("GOLF", "Archive repaired current hole to 1");
  }
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    if (validation.holePuttsRepaired(hole)) {
      LOG_ERR("GOLF", "Archive repaired hole %u putts to %u", hole + 1, round.putts[hole]);
    }
    if (validation.holeIn100Repaired(hole)) {
      LOG_ERR("GOLF", "Archive repaired hole %u in100 to %u", hole + 1, round.in100[hole]);
    }
    if (validation.holePenaltyCountRepaired(hole)) {
      LOG_ERR("GOLF", "Archive repaired hole %u penalty count to %u", hole + 1, round.penaltyCount[hole]);
    }
    if (validation.holePenaltyEventRepaired(hole)) {
      LOG_ERR("GOLF", "Archive removed invalid penalty event on hole %u", hole + 1);
    }
    if (validation.holePenaltyMarkerRepaired(hole)) {
      LOG_ERR("GOLF", "Archive removed penalty marker exceeding shots on hole %u", hole + 1);
    }
  }
}

uint16_t nextRoundSequence() {
  HalFile directory = Storage.open(ROUNDS_DIRECTORY);
  if (!directory || !directory.isDirectory()) return 1;
  uint16_t highest = 0;
  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    if (entry.isDirectory()) continue;
    char filename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
    if (entry.getName(filename, sizeof(filename)) == 0 || strncmp(filename, "round-", 6) != 0) continue;
    uint16_t sequence = 0;
    bool valid = true;
    for (uint8_t digit = 0; digit < 4; ++digit) {
      const char value = filename[6 + digit];
      if (value < '0' || value > '9') {
        valid = false;
        break;
      }
      sequence = static_cast<uint16_t>(sequence * 10 + value - '0');
    }
    if (valid && filename[10] == '-' && sequence > highest) highest = sequence;
  }
  return highest >= 9999 ? 0 : static_cast<uint16_t>(highest + 1);
}

}  // namespace

bool RoundArchive::archive(const GolfRound& source) {
  if (GOLF_ROUND_STORE.isArchived()) {
    return GOLF_ROUND_STORE.clear();
  }
  GolfRound round = source;
  const GolfValidationResult validation = validateGolfRound(round);
  if (!validation.valid) {
    LOG_ERR("GOLF", "Refused to archive unsupported hole count %u", round.holeCount);
    return false;
  }
  logArchiveRepairs(round, validation);
  if (!Storage.ensureDirectoryExists(ROUNDS_DIRECTORY)) {
    LOG_ERR("GOLF", "Failed to create %s", ROUNDS_DIRECTORY);
    return false;
  }

  char filename[GOLF_ROUND_FILENAME_BUFFER_SIZE];
  char path[sizeof(ROUNDS_DIRECTORY) + GOLF_ROUND_FILENAME_BUFFER_SIZE + 1];
  const uint16_t roundSequence = nextRoundSequence();
  if (roundSequence == 0) {
    LOG_ERR("GOLF", "Exhausted round sequence numbers");
    return false;
  }
  uint16_t collisionSuffix = 0;
  do {
    if (!golfRoundFilename(roundSequence, round.courseName, collisionSuffix, filename, sizeof(filename))) {
      LOG_ERR("GOLF", "Failed to create round filename");
      return false;
    }
    snprintf(path, sizeof(path), "%s/%s", ROUNDS_DIRECTORY, filename);
    collisionSuffix = collisionSuffix == 0 ? 2 : static_cast<uint16_t>(collisionSuffix + 1);
    if (collisionSuffix == 0) {
      LOG_ERR("GOLF", "Exhausted round filename suffixes");
      return false;
    }
  } while (Storage.exists(path));

  HalFile file = Storage.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file || !writeCompletedRound(file, round)) {
    LOG_ERR("GOLF", "Failed to write %s", path);
    file.close();
    Storage.remove(path);
    return false;
  }
  file.flush();
  file.close();
  if (!appendIndex(round, filename)) {
    Storage.remove(path);
    return false;
  }
  if (!GOLF_ROUND_STORE.markArchivedAs(filename)) {
    LOG_ERR("GOLF", "Round archived as %s but commit marker write failed", filename);
    return false;
  }
  return GOLF_ROUND_STORE.clear();
}

#endif
