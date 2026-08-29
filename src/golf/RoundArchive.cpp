#include "RoundArchive.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "GolfCsv.h"
#include "GolfPaths.h"
#include "GolfRoundStore.h"
#include "GolfStats.h"
#include "GolfValidate.h"

namespace {

constexpr char ROUNDS_DIRECTORY[] = "/golf/rounds";
constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

bool writeText(HalFile& file, const char* text) { return file.write(text, strlen(text)) == strlen(text); }

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

bool writeCompletedRound(HalFile& file, const GolfRound& round) {
  char date[GOLF_DATE_BUFFER_SIZE];
  char metadata[32];
  if (!golfFormatDate(round.dateYmd, date, sizeof(date)) || !writeText(file, "{\"v\":1,\"date\":") ||
      !writeJsonString(file, date) || !writeText(file, ",\"course\":") || !writeJsonString(file, round.courseName) ||
      !writeText(file, ",\"tees\":") || !writeJsonString(file, round.tees)) {
    return false;
  }
  snprintf(metadata, sizeof(metadata), ",\"holes\":%u,\"par\":", round.holeCount);
  return writeText(file, metadata) && writeArray(file, round.par, round.holeCount) &&
         writeText(file, ",\"strokes\":") && writeArray(file, round.strokes, round.holeCount) &&
         writeText(file, ",\"putts\":") && writeArray(file, round.putts, round.holeCount) &&
         writeText(file, ",\"in100\":") && writeArray(file, round.in100, round.holeCount) && writeText(file, "}\n");
}

bool appendIndex(const GolfRound& round, const char* filename) {
  char date[GOLF_DATE_BUFFER_SIZE];
  char csv[GOLF_CSV_ROW_BUFFER_SIZE];
  if (!golfFormatDate(round.dateYmd, date, sizeof(date))) {
    return false;
  }
  const int32_t parTotal = static_cast<int32_t>(golfScore(round)) - golfToPar(round);
  const GolfIndexRowView row{date,
                             round.courseName,
                             round.holeCount,
                             golfScore(round),
                             static_cast<uint16_t>(parTotal),
                             golfPuttsTotal(round),
                             golfIn100Total(round),
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
  }
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
  uint16_t sequence = 0;
  do {
    if (!golfRoundFilename(round.dateYmd, round.courseName, sequence, filename, sizeof(filename))) {
      LOG_ERR("GOLF", "Failed to create round filename");
      return false;
    }
    snprintf(path, sizeof(path), "%s/%s", ROUNDS_DIRECTORY, filename);
    sequence = sequence == 0 ? 2 : static_cast<uint16_t>(sequence + 1);
    if (sequence == 0) {
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

bool RoundArchive::lastRoundDate(uint16_t& dateYmd) {
  HalFile index = Storage.open(INDEX_PATH);
  if (!index) return false;

  char line[GOLF_CSV_ROW_BUFFER_SIZE]{};
  size_t length = 0;
  bool found = false;
  while (index.available()) {
    const int next = index.read();
    if (next < 0) break;
    if (next != '\n' && length + 1 < sizeof(line)) {
      line[length++] = static_cast<char>(next);
      continue;
    }
    line[length] = '\0';
    GolfIndexRow row{};
    uint16_t parsedDate = 0;
    if (golfParseIndexRow(line, row) && golfParseDate(row.date, parsedDate)) {
      dateYmd = parsedDate;
      found = true;
    }
    length = 0;
  }
  if (length > 0) {
    line[length] = '\0';
    GolfIndexRow row{};
    uint16_t parsedDate = 0;
    if (golfParseIndexRow(line, row) && golfParseDate(row.date, parsedDate)) {
      dateYmd = parsedDate;
      found = true;
    }
  }
  return found;
}

#endif
