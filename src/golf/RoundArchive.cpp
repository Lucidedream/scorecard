#include "RoundArchive.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "CrossPointSettings.h"
#include "GolfCsv.h"
#include "GolfIndexMigrate.h"
#include "GolfJson.h"
#include "GolfPaths.h"
#include "GolfPenalty.h"
#include "GolfRoundDecode.h"
#include "GolfRoundRepairLog.h"
#include "GolfRoundStore.h"
#include "GolfValidate.h"

namespace {

constexpr char ROUNDS_DIRECTORY[] = "/golf/rounds";
constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";
constexpr char INDEX_NEW_PATH[] = "/golf/rounds/index.csv.new";
constexpr char INDEX_BAK_PATH[] = "/golf/rounds/index.csv.bak";

struct ArchiveScratch {
  GolfRound round{};
  GolfValidationResult validation{};
  GolfIndexMigrator indexMigrator{};
  GolfIndexRow indexRow{};
  char csv[GOLF_CSV_ROW_BUFFER_SIZE]{};
  char filename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  char path[sizeof(ROUNDS_DIRECTORY) + GOLF_ROUND_FILENAME_BUFFER_SIZE + 1]{};
};

struct RemoveScratch {
  GolfIndexMigrator indexMigrator{};
  char path[sizeof(ROUNDS_DIRECTORY) + GOLF_ROUND_FILENAME_BUFFER_SIZE + 1]{};
};

constexpr uint32_t ARCHIVE_HASH_OFFSET = 2166136261U;
constexpr uint32_t ARCHIVE_HASH_PRIME = 16777619U;

struct ArchiveDigest {
  uint32_t hash = ARCHIVE_HASH_OFFSET;
  uint32_t bytes = 0;
};

void updateArchiveDigest(ArchiveDigest& digest, const char* data, const size_t size) {
  for (size_t index = 0; index < size; ++index) {
    digest.hash ^= static_cast<uint8_t>(data[index]);
    digest.hash *= ARCHIVE_HASH_PRIME;
  }
  digest.bytes += static_cast<uint32_t>(size);
}

class ArchiveWriter {
 public:
  explicit ArchiveWriter(HalFile& file) : file_(file) {}

  bool write(const char* data, const size_t size) {
    if (file_.write(data, size) != size) return false;
    updateArchiveDigest(digest_, data, size);
    return true;
  }

  ArchiveDigest digest() const { return digest_; }

 private:
  HalFile& file_;
  ArchiveDigest digest_{};
};

struct IndexTransactionContext {
  GolfIndexMigrator* migrator = nullptr;
  GolfIndexRow* indexRow = nullptr;
  char* csv = nullptr;
  size_t csvSize = 0;
  const GolfRound* round = nullptr;
  const char* filename = nullptr;
  uint8_t expectedMask = 0;
};

bool writeText(HalFile& file, const char* text) { return file.write(text, strlen(text)) == strlen(text); }

bool writeText(ArchiveWriter& writer, const char* text) { return writer.write(text, strlen(text)); }

bool archiveStringsValid(const GolfRound& round) {
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

bool migrateSink(const char* data, const size_t size, void* user) {
  auto* file = static_cast<HalFile*>(user);
  return file->write(data, size) == size;
}

enum class IndexStreamStatus : uint8_t { Complete, OpenFailed, ReadFailed, ParseFailed };

IndexStreamStatus runMigrator(const char* path, GolfIndexMigrator& migrator, const GolfIndexMigrateSink sink,
                              void* sinkUser) {
  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) return IndexStreamStatus::OpenFailed;
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) return IndexStreamStatus::ReadFailed;
    if (!migrator.feed(chunk, static_cast<size_t>(bytesRead), sink, sinkUser)) {
      return IndexStreamStatus::ParseFailed;
    }
  }
  return migrator.finish() ? IndexStreamStatus::Complete : IndexStreamStatus::ParseFailed;
}

bool indexExists(const char* path, void*) { return Storage.exists(path); }

bool indexValid(const char* path, const bool requireV4, void* user) {
  auto* verifier = static_cast<GolfIndexMigrator*>(user);
  if (verifier == nullptr) {
    LOG_ERR("GOLF", "index recovery verify missing scratch: %s", path);
    return false;
  }
  if (requireV4) {
    verifier->resetForStrictValidation(true);
  } else {
    // v4 is intrinsically strict. A recognized legacy live file remains
    // recoverable so the explicit migration pass can normalize it.
    verifier->reset();
  }
  const IndexStreamStatus stream = runMigrator(path, *verifier, nullptr, nullptr);
  const GolfIndexVersion version = verifier->sourceVersion();
  const bool valid = stream == IndexStreamStatus::Complete && !verifier->aborted() &&
                     (requireV4 ? version == GolfIndexVersion::V4 : version != GolfIndexVersion::Unknown);
  if (!valid) {
    LOG_ERR("GOLF", "index recovery verify failed: %s (stream=%u version=%u)", path,
            static_cast<unsigned>(stream), static_cast<unsigned>(version));
  }
  return valid;
}

bool indexRemove(const char* path, void*) {
  if (Storage.remove(path)) return true;
  LOG_ERR("GOLF", "index recovery remove failed: %s", path);
  return false;
}

bool indexRename(const char* from, const char* to, void*) {
  if (Storage.rename(from, to)) return true;
  LOG_ERR("GOLF", "index recovery rename failed: %s -> %s", from, to);
  return false;
}

const char* indexArtifactPath(const GolfIndexArtifact artifact) {
  switch (artifact) {
    case GolfIndexArtifact::Live:
      return INDEX_PATH;
    case GolfIndexArtifact::Staged:
      return INDEX_NEW_PATH;
    case GolfIndexArtifact::Backup:
      return INDEX_BAK_PATH;
  }
  return "<invalid>";
}

const char* indexPurposeName(const GolfIndexStagePurpose purpose) {
  switch (purpose) {
    case GolfIndexStagePurpose::Migration:
      return "migration";
    case GolfIndexStagePurpose::Append:
      return "append";
    case GolfIndexStagePurpose::Delete:
      return "delete";
  }
  return "unknown";
}

bool transactionRemove(const GolfIndexArtifact artifact, const GolfIndexStagePurpose purpose, void*) {
  const char* path = indexArtifactPath(artifact);
  if (Storage.remove(path)) return true;
  LOG_ERR("GOLF", "index %s remove failed: %s", indexPurposeName(purpose), path);
  return false;
}

bool transactionRename(const GolfIndexArtifact from, const GolfIndexArtifact to,
                       const GolfIndexStagePurpose purpose, void*) {
  const char* fromPath = indexArtifactPath(from);
  const char* toPath = indexArtifactPath(to);
  if (Storage.rename(fromPath, toPath)) return true;
  LOG_ERR("GOLF", "index %s rename failed: %s -> %s", indexPurposeName(purpose), fromPath, toPath);
  return false;
}

bool removeStagedIndex(const char* operation) {
  if (Storage.remove(INDEX_NEW_PATH)) return true;
  LOG_ERR("GOLF", "index %s staging remove failed: %s", operation, INDEX_NEW_PATH);
  return false;
}

enum class IndexRewriteStatus : uint8_t { FailedBeforeCommit, CommittedCleanupPending, Complete };

struct IndexRewriteResult {
  IndexRewriteStatus status = IndexRewriteStatus::FailedBeforeCommit;

  constexpr bool committed() const { return status != IndexRewriteStatus::FailedBeforeCommit; }
};

IndexRewriteResult commitStagedIndex(const bool hadOriginal, const uint32_t expectedRows, const char* operation,
                                     GolfIndexMigrator& verifier, const char* groupFilename = nullptr,
                                     const uint8_t expectedGroupMask = 0) {
  if (groupFilename != nullptr) {
    if (!verifier.resetForGroupCount(groupFilename)) {
      LOG_ERR("GOLF", "index %s verify setup failed: %s", operation, INDEX_NEW_PATH);
      removeStagedIndex(operation);
      return {};
    }
  } else {
    verifier.resetForStrictValidation(true);
  }
  const IndexStreamStatus stream = runMigrator(INDEX_NEW_PATH, verifier, nullptr, nullptr);
  const bool rowsMatch = verifier.dataRows() == expectedRows;
  const bool groupMatches = groupFilename == nullptr ||
                            (expectedGroupMask == 0
                                 ? verifier.groupRows() == 0
                                 : verifier.groupValid() && verifier.groupSlotMask() == expectedGroupMask);
  if (stream != IndexStreamStatus::Complete || verifier.sourceVersion() != GolfIndexVersion::V4 || !rowsMatch ||
      !groupMatches) {
    LOG_ERR("GOLF", "index %s verify failed: %s (stream=%u rows=%lu expected=%lu)", operation,
            INDEX_NEW_PATH, static_cast<unsigned>(stream), static_cast<unsigned long>(verifier.dataRows()),
            static_cast<unsigned long>(expectedRows));
    removeStagedIndex(operation);
    return {};
  }

  const GolfIndexPublicationOps storage{nullptr, &transactionRemove, &transactionRename};
  const GolfIndexPublishResult published =
      golfPublishStagedIndex(hadOriginal, GolfIndexStagePurpose::Delete, storage);
  if (!published.committed()) {
    LOG_ERR("GOLF", "index %s publication failed before commit: error=%u cleanupPending=%u", operation,
            static_cast<unsigned>(published.error), published.cleanupPending ? 1U : 0U);
    return {};
  }
  if (!published.ok() || published.cleanupPending) {
    LOG_ERR("GOLF", "index %s committed with artifact cleanup pending: error=%u", operation,
            static_cast<unsigned>(published.error));
    return {IndexRewriteStatus::CommittedCleanupPending};
  }
  return {IndexRewriteStatus::Complete};
}

GolfIndexStageWriteStatus copyLiveIndex(HalFile& output) {
  HalFile input;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, input)) {
    LOG_ERR("GOLF", "index append source open failed: %s", INDEX_PATH);
    return GolfIndexStageWriteStatus::ReadFailed;
  }
  char chunk[128];
  while (input.available() > 0) {
    const int bytesRead = input.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      LOG_ERR("GOLF", "index append source read failed: %s", INDEX_PATH);
      return GolfIndexStageWriteStatus::ReadFailed;
    }
    if (output.write(chunk, static_cast<size_t>(bytesRead)) != static_cast<size_t>(bytesRead)) {
      LOG_ERR("GOLF", "index append stage copy write failed: %s", INDEX_NEW_PATH);
      return GolfIndexStageWriteStatus::WriteFailed;
    }
  }
  return GolfIndexStageWriteStatus::Complete;
}

GolfIndexStageWriteStatus writeIndexStage(const GolfIndexStagePurpose purpose, const GolfIndexLiveState live,
                                          void* user) {
  auto* context = static_cast<IndexTransactionContext*>(user);
  if (context == nullptr || context->migrator == nullptr) {
    LOG_ERR("GOLF", "index %s stage missing transaction scratch: %s", indexPurposeName(purpose),
            INDEX_NEW_PATH);
    return GolfIndexStageWriteStatus::WriteFailed;
  }

  HalFile staged = Storage.open(INDEX_NEW_PATH, O_WRONLY | O_CREAT | O_TRUNC);
  if (!staged) {
    LOG_ERR("GOLF", "index %s stage open failed: %s", indexPurposeName(purpose), INDEX_NEW_PATH);
    return GolfIndexStageWriteStatus::OpenFailed;
  }

  if (purpose == GolfIndexStagePurpose::Migration) {
    context->migrator->reset();
    const IndexStreamStatus stream = runMigrator(INDEX_PATH, *context->migrator, &migrateSink, &staged);
    if (stream == IndexStreamStatus::OpenFailed || stream == IndexStreamStatus::ReadFailed) {
      LOG_ERR("GOLF", "index migration source read failed: %s (stream=%u)", INDEX_PATH,
              static_cast<unsigned>(stream));
      return GolfIndexStageWriteStatus::ReadFailed;
    }
    if (stream != IndexStreamStatus::Complete || context->migrator->aborted() ||
        context->migrator->sourceVersion() != live.version || context->migrator->dataRows() != live.rows) {
      LOG_ERR("GOLF", "index migration stage write failed: %s -> %s (stream=%u)", INDEX_PATH,
              INDEX_NEW_PATH, static_cast<unsigned>(stream));
      return GolfIndexStageWriteStatus::WriteFailed;
    }
  } else {
    if (context->round == nullptr || context->filename == nullptr || context->indexRow == nullptr ||
        context->csv == nullptr || context->csvSize == 0) {
      LOG_ERR("GOLF", "index append stage missing row context: %s", INDEX_NEW_PATH);
      return GolfIndexStageWriteStatus::WriteFailed;
    }
    if (live.present) {
      const GolfIndexStageWriteStatus copied = copyLiveIndex(staged);
      if (copied != GolfIndexStageWriteStatus::Complete) return copied;
    } else if (!writeText(staged, GOLF_INDEX_HEADER)) {
      LOG_ERR("GOLF", "index append header write failed: %s", INDEX_NEW_PATH);
      return GolfIndexStageWriteStatus::WriteFailed;
    }

    const GolfIndexGroupWriteResult group =
        golfWriteIndexGroupRows(*context->round, context->filename, *context->indexRow, context->csv,
                                context->csvSize, &migrateSink, &staged);
    if (!group.complete || group.slotMask != context->expectedMask ||
        !golfIndexGroupRowsValid(group.rowCount, group.slotMask)) {
      LOG_ERR("GOLF", "index append group write failed: %s (rows=%u mask=0x%02x)", INDEX_NEW_PATH,
              static_cast<unsigned>(group.rowCount), static_cast<unsigned>(group.slotMask));
      return GolfIndexStageWriteStatus::WriteFailed;
    }
  }

  if (!staged.sync()) {
    LOG_ERR("GOLF", "index %s stage sync failed: %s", indexPurposeName(purpose), INDEX_NEW_PATH);
    return GolfIndexStageWriteStatus::SyncFailed;
  }
  return GolfIndexStageWriteStatus::Complete;
}

bool verifyIndexStage(const GolfIndexStagePurpose purpose, const uint32_t expectedRows, void* user) {
  auto* context = static_cast<IndexTransactionContext*>(user);
  if (context == nullptr || context->migrator == nullptr) {
    LOG_ERR("GOLF", "index %s stage verify missing scratch: %s", indexPurposeName(purpose), INDEX_NEW_PATH);
    return false;
  }

  if (purpose == GolfIndexStagePurpose::Append) {
    if (!context->migrator->resetForGroupCount(context->filename)) {
      LOG_ERR("GOLF", "index append stage verify setup failed: %s", INDEX_NEW_PATH);
      return false;
    }
  } else {
    context->migrator->resetForStrictValidation(true);
  }
  const IndexStreamStatus stream = runMigrator(INDEX_NEW_PATH, *context->migrator, nullptr, nullptr);
  const bool rowsMatch = context->migrator->dataRows() == expectedRows;
  const bool groupMatches = purpose != GolfIndexStagePurpose::Append ||
                            (context->migrator->groupValid() &&
                             context->migrator->groupSlotMask() == context->expectedMask);
  const bool valid = stream == IndexStreamStatus::Complete &&
                     context->migrator->sourceVersion() == GolfIndexVersion::V4 && rowsMatch && groupMatches;
  if (!valid) {
    LOG_ERR("GOLF", "index %s stage verify failed: %s (stream=%u rows=%lu expected=%lu)",
            indexPurposeName(purpose), INDEX_NEW_PATH, static_cast<unsigned>(stream),
            static_cast<unsigned long>(context->migrator->dataRows()), static_cast<unsigned long>(expectedRows));
  }
  return valid;
}

GolfIndexTransactionOps indexTransactionOps(IndexTransactionContext& context) {
  return {&context, &writeIndexStage, &verifyIndexStage, &transactionRemove, &transactionRename};
}

void logIndexTransactionFailure(const GolfIndexTransactionResult& result) {
  LOG_ERR("GOLF", "index transaction failed: error=%u committed=%u cleanupPending=%u",
          static_cast<unsigned>(result.error), result.appendCommitted ? 1U : 0U,
          result.cleanupPending ? 1U : 0U);
}

bool migrateIndexToV4IfNeeded(GolfIndexLiveState& live, GolfIndexMigrator& migrator) {
  const GolfIndexVersion originalVersion = live.version;
  IndexTransactionContext context{};
  context.migrator = &migrator;
  const GolfIndexTransactionResult result = golfRunIndexTransaction(live, 0, indexTransactionOps(context));
  live = result.live;
  if (!result.ok()) {
    logIndexTransactionFailure(result);
    return false;
  }
  if (originalVersion == GolfIndexVersion::V2 || originalVersion == GolfIndexVersion::V3) {
    LOG_INF("GOLF", "index.csv migrated to v4 (%lu rows)", static_cast<unsigned long>(live.rows));
  }
  return true;
}

uint8_t enabledPlayerCount(const uint8_t mask) {
  uint8_t count = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((mask & (1U << slot)) != 0) ++count;
  }
  return count;
}

// Keep staged-index handles in their own bounded frame under LTO.
[[gnu::noinline]] GolfIndexTransactionResult appendIndexGroup(const GolfRound& round, const char* filename,
                                                              ArchiveScratch& scratch,
                                                              const GolfIndexLiveState live) {
  const uint8_t expectedMask = golfEnabledPlayerMask(round);
  const uint8_t expectedRows = enabledPlayerCount(expectedMask);
  if (!golfIndexGroupRowsValid(expectedRows, expectedMask)) {
    GolfIndexTransactionResult invalid{};
    invalid.error = GolfIndexTransactionError::InvalidState;
    invalid.live = live;
    LOG_ERR("GOLF", "index append rejected player group: rows=%u mask=0x%02x", expectedRows, expectedMask);
    return invalid;
  }

  IndexTransactionContext context{};
  context.migrator = &scratch.indexMigrator;
  context.indexRow = &scratch.indexRow;
  context.csv = scratch.csv;
  context.csvSize = sizeof(scratch.csv);
  context.round = &round;
  context.filename = filename;
  context.expectedMask = expectedMask;
  const GolfIndexTransactionResult result =
      golfRunIndexTransaction(live, expectedRows, indexTransactionOps(context));
  if (!result.ok()) logIndexTransactionFailure(result);
  return result;
}

IndexRewriteResult rewriteIndexWithout(const char* filename, GolfIndexMigrator& rewrite,
                                       GolfIndexLiveState& live) {
  if (!live.present) {
    LOG_ERR("GOLF", "index delete has no recovered live index: %s", INDEX_PATH);
    return {};
  }
  if (!migrateIndexToV4IfNeeded(live, rewrite)) return {};
  if (!rewrite.resetForDelete(filename)) {
    LOG_ERR("GOLF", "index delete verify setup failed for filename: %s", filename);
    return {};
  }

  IndexStreamStatus stream = IndexStreamStatus::OpenFailed;
  bool groupAbsent = false;
  bool synced = false;
  {
    HalFile staged = Storage.open(INDEX_NEW_PATH, O_WRONLY | O_CREAT | O_TRUNC);
    if (!staged) {
      LOG_ERR("GOLF", "index delete stage open failed: %s", INDEX_NEW_PATH);
      return {};
    }
    stream = runMigrator(INDEX_PATH, rewrite, &migrateSink, &staged);
    if (stream == IndexStreamStatus::Complete && !rewrite.aborted()) {
      groupAbsent = rewrite.groupRows() == 0;
      if (!groupAbsent && rewrite.groupValid()) {
        synced = staged.sync();
        if (!synced) LOG_ERR("GOLF", "index delete stage sync failed: %s", INDEX_NEW_PATH);
      }
    }
  }
  if (groupAbsent) {
    return {removeStagedIndex("delete retry") ? IndexRewriteStatus::Complete
                                               : IndexRewriteStatus::CommittedCleanupPending};
  }
  if (stream != IndexStreamStatus::Complete || rewrite.aborted() || !rewrite.groupValid() || !synced) {
    LOG_ERR("GOLF", "index delete stage write failed: %s -> %s (stream=%u matched=%u)", INDEX_PATH,
            INDEX_NEW_PATH, static_cast<unsigned>(stream), static_cast<unsigned>(rewrite.deletedRows()));
    removeStagedIndex("delete");
    return {};
  }
  return commitStagedIndex(true, rewrite.outputRows(), "delete", rewrite, filename, 0);
}

bool writeJsonString(ArchiveWriter& file, const char* value) {
  if (value == nullptr || !writeText(file, "\"")) return false;
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
    if (!writeText(file, text)) return false;
  }
  return writeText(file, "\"");
}

template <typename T>
bool writeArray(ArchiveWriter& file, const T* values, const uint8_t count, const bool writeZeros = false) {
  if (!writeText(file, "[")) return false;
  for (uint8_t hole = 0; hole < count; ++hole) {
    char number[8];
    const unsigned value = writeZeros ? 0U : static_cast<unsigned>(values[hole]);
    snprintf(number, sizeof(number), hole == 0 ? "%u" : ",%u", value);
    if (!writeText(file, number)) return false;
  }
  return writeText(file, "]");
}

bool writePenalties(ArchiveWriter& file, const GolfPlayerScore& score, const uint8_t holeCount,
                    const bool writeZeros) {
  if (!writeText(file, "[")) return false;
  for (uint8_t hole = 0; hole < holeCount; ++hole) {
    if (!writeText(file, hole == 0 ? "[" : ",[")) return false;
    const uint8_t count = writeZeros ? 0
                                     : (score.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                                            ? score.penaltyCount[hole]
                                            : GolfRound::MAX_PENALTIES_PER_HOLE);
    for (uint8_t index = 0; index < count; ++index) {
      GolfPenaltyEvent event{};
      if (!golfPenaltyEventAt(score, hole, index, event)) continue;
      char pair[12];
      snprintf(pair, sizeof(pair), index == 0 ? "[%u,%u]" : ",[%u,%u]",
               static_cast<unsigned>(event.field), static_cast<unsigned>(event.kind));
      if (!writeText(file, pair)) return false;
    }
    if (!writeText(file, "]")) return false;
  }
  return writeText(file, "]");
}

bool writePlayer(ArchiveWriter& file, const GolfPlayer& player, const uint8_t holeCount) {
  const char* tee = golfTeeSelectionToken(player.tee);
  if (tee == nullptr || !writeText(file, "{\"name\":") || !writeJsonString(file, player.name) ||
      !writeText(file, ",\"tee\":") || !writeJsonString(file, tee) || !writeText(file, ",\"yards\":")) {
    return false;
  }
  const bool disabled = !golfPlayerIsEnabled(player);
  return writeArray(file, player.yards, holeCount, disabled) && writeText(file, ",\"putts\":") &&
         writeArray(file, player.score.putts, holeCount, disabled) && writeText(file, ",\"in100\":") &&
         writeArray(file, player.score.in100, holeCount, disabled) && writeText(file, ",\"out100\":") &&
         writeArray(file, player.score.out100, holeCount, disabled) && writeText(file, ",\"penalties\":") &&
         writePenalties(file, player.score, holeCount, disabled) && writeText(file, "}");
}

// Keep streaming JSON formatter locals out of archive()'s activity-task frame.
[[gnu::noinline]] bool writeCompletedRound(ArchiveWriter& file, const GolfRound& round) {
  char date[GOLF_DATE_BUFFER_SIZE];
  char metadata[48];
  if (!writeText(file, "{\"v\":4,\"date\":")) return false;
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    if (!writeJsonString(file, date)) return false;
  } else if (!writeText(file, "null")) {
    return false;
  }
  if (!writeText(file, ",\"course\":") || !writeJsonString(file, round.courseName)) return false;
  snprintf(metadata, sizeof(metadata), ",\"holes\":%u,\"par\":", static_cast<unsigned>(round.holeCount));
  if (!writeText(file, metadata) || !writeArray(file, round.par, round.holeCount) ||
      !writeText(file, round.hasSi ? ",\"hasSi\":true,\"si\":" : ",\"hasSi\":false,\"si\":") ||
      !writeArray(file, round.si, round.holeCount, !round.hasSi) || !writeText(file, ",\"players\":[")) {
    return false;
  }
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((slot != 0 && !writeText(file, ",")) || !writePlayer(file, round.players[slot], round.holeCount)) return false;
  }
  return writeText(file, "]}\n");
}

[[gnu::noinline]] bool verifyCompletedRound(const char* path, const ArchiveDigest expected) {
  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) {
    LOG_ERR("GOLF", "completed round verify open failed: %s", path);
    return false;
  }

  ArchiveDigest actual{};
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      LOG_ERR("GOLF", "completed round verify read failed: %s", path);
      return false;
    }
    updateArchiveDigest(actual, chunk, static_cast<size_t>(bytesRead));
  }
  if (actual.bytes != expected.bytes || actual.hash != expected.hash) {
    LOG_ERR("GOLF", "completed round verify mismatch: %s (bytes=%lu expected=%lu)", path,
            static_cast<unsigned long>(actual.bytes), static_cast<unsigned long>(expected.bytes));
    return false;
  }
  return true;
}

bool removeCompletedRound(const char* path, const char* phase) {
  if (Storage.remove(path)) return true;
  LOG_ERR("GOLF", "completed round %s remove failed: %s", phase, path);
  return false;
}

// Keep directory iteration handles in their own bounded frame under LTO.
[[gnu::noinline]] uint16_t nextRoundSequence() {
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

bool recoverIndexState(GolfIndexMigrator& scratch, GolfIndexLiveState& live) {
  live = {};
  const GolfIndexRecoveryOps ops{&scratch, &indexExists, &indexValid, &indexRemove, &indexRename};
  const GolfIndexRecoveryStatus status =
      golfRecoverIndexArtifacts(ops, INDEX_PATH, INDEX_NEW_PATH, INDEX_BAK_PATH);
  if (status == GolfIndexRecoveryStatus::Failed) {
    LOG_ERR("GOLF", "index recovery failed: live=%s staged=%s backup=%s", INDEX_PATH, INDEX_NEW_PATH,
            INDEX_BAK_PATH);
    return false;
  }
  if (status == GolfIndexRecoveryStatus::NoIndex) return true;

  const GolfIndexVersion version = scratch.sourceVersion();
  if (version == GolfIndexVersion::Unknown) {
    LOG_ERR("GOLF", "index recovery produced unknown live version: %s", INDEX_PATH);
    return false;
  }
  live = {scratch.dataRows(), version, true};
  return true;
}

}  // namespace

bool RoundArchive::recoverIndex(GolfIndexMigrator& scratch) {
  GolfIndexLiveState live{};
  return recoverIndexState(scratch, live);
}

RoundArchiveResult RoundArchive::archive(const GolfRound& source) {
  if (GOLF_ROUND_STORE.isArchived()) {
    return GOLF_ROUND_STORE.clear() ? RoundArchiveResult::Complete
                                    : RoundArchiveResult::CommittedCleanupPending;
  }

  // Validation, index migration, row formatting, and path construction share
  // one checked operation allocation rather than consuming the activity stack.
  auto scratch = makeUniqueNoThrow<ArchiveScratch>();
  if (!scratch) {
    LOG_ERR("GOLF", "OOM: round archive scratch (%u bytes)", static_cast<unsigned>(sizeof(ArchiveScratch)));
    return RoundArchiveResult::FailedBeforeCommit;
  }
  GolfIndexLiveState liveIndex{};
  if (!recoverIndexState(scratch->indexMigrator, liveIndex)) {
    return RoundArchiveResult::FailedBeforeCommit;
  }

  scratch->round = source;
  scratch->round.dateYmd = 0;
  const int16_t utcOffsetMinutes = static_cast<int16_t>(static_cast<int16_t>(SETTINGS.clockUtcOffsetQ) - 48) * 15;
  golfDateFromTimestamp(static_cast<int64_t>(time(nullptr)), utcOffsetMinutes, scratch->round.dateYmd);
  scratch->validation = validateGolfRound(scratch->round);
  if (!scratch->validation.valid || !archiveStringsValid(scratch->round)) {
    LOG_ERR("GOLF", "Refused to archive invalid round");
    return RoundArchiveResult::FailedBeforeCommit;
  }
  golfLogRoundRepairs(scratch->round, scratch->validation);
  if (!Storage.ensureDirectoryExists(ROUNDS_DIRECTORY)) {
    LOG_ERR("GOLF", "Failed to create %s", ROUNDS_DIRECTORY);
    return RoundArchiveResult::FailedBeforeCommit;
  }

  const uint16_t roundSequence = nextRoundSequence();
  if (roundSequence == 0) {
    LOG_ERR("GOLF", "Exhausted round sequence numbers");
    return RoundArchiveResult::FailedBeforeCommit;
  }
  uint16_t collisionSuffix = 0;
  do {
    if (!golfRoundFilename(roundSequence, scratch->round.courseName, collisionSuffix, scratch->filename,
                           sizeof(scratch->filename))) {
      LOG_ERR("GOLF", "Failed to create round filename");
      return RoundArchiveResult::FailedBeforeCommit;
    }
    snprintf(scratch->path, sizeof(scratch->path), "%s/%s", ROUNDS_DIRECTORY, scratch->filename);
    collisionSuffix = collisionSuffix == 0 ? 2 : static_cast<uint16_t>(collisionSuffix + 1);
    if (collisionSuffix == 0) {
      LOG_ERR("GOLF", "Exhausted round filename suffixes");
      return RoundArchiveResult::FailedBeforeCommit;
    }
  } while (Storage.exists(scratch->path));

  ArchiveDigest expectedDigest{};
  bool archiveOpened = false;
  bool archiveWritten = false;
  bool archiveSynced = false;
  {
    HalFile file = Storage.open(scratch->path, O_WRONLY | O_CREAT | O_TRUNC);
    archiveOpened = static_cast<bool>(file);
    if (!archiveOpened) {
      LOG_ERR("GOLF", "completed round open failed: %s", scratch->path);
    } else {
      ArchiveWriter writer(file);
      archiveWritten = writeCompletedRound(writer, scratch->round);
      if (!archiveWritten) {
        LOG_ERR("GOLF", "completed round write failed: %s", scratch->path);
      } else {
        expectedDigest = writer.digest();
        archiveSynced = file.sync();
        if (!archiveSynced) LOG_ERR("GOLF", "completed round sync failed: %s", scratch->path);
      }
    }
  }
  if (!archiveOpened || !archiveWritten || !archiveSynced) {
    if (archiveOpened) removeCompletedRound(scratch->path, "write rollback");
    return RoundArchiveResult::FailedBeforeCommit;
  }
  if (!verifyCompletedRound(scratch->path, expectedDigest)) {
    removeCompletedRound(scratch->path, "verify rollback");
    return RoundArchiveResult::FailedBeforeCommit;
  }

  const GolfIndexTransactionResult indexResult =
      appendIndexGroup(scratch->round, scratch->filename, *scratch, liveIndex);
  if (!indexResult.appendCommitted) {
    if (indexResult.cleanupPending) {
      LOG_ERR("GOLF", "completed round retained while index cleanup is pending: %s", scratch->path);
    } else {
      removeCompletedRound(scratch->path, "index rollback");
    }
    return RoundArchiveResult::FailedBeforeCommit;
  }

  // The group JSON and all index rows are now authoritative. markArchivedAs()
  // sets the RAM marker before trying persistence, making scorer mutations
  // illegal even when state cleanup cannot finish in this attempt.
  if (!GOLF_ROUND_STORE.markArchivedAs(scratch->filename)) {
    LOG_ERR("GOLF", "Round committed as %s but marker persistence needs cleanup", scratch->filename);
    return RoundArchiveResult::CommittedCleanupPending;
  }
  if (!GOLF_ROUND_STORE.clear()) {
    LOG_ERR("GOLF", "Round committed as %s but state cleanup remains", scratch->filename);
    return RoundArchiveResult::CommittedCleanupPending;
  }
  if (!indexResult.ok() || indexResult.cleanupPending) {
    LOG_ERR("GOLF", "Round committed as %s but index artifact cleanup remains", scratch->filename);
    return RoundArchiveResult::CommittedCleanupPending;
  }
  return RoundArchiveResult::Complete;
}

bool RoundArchive::remove(const char* filename) {
  auto scratch = makeUniqueNoThrow<RemoveScratch>();
  if (!scratch) {
    LOG_ERR("GOLF", "OOM: round removal scratch (%u bytes)", static_cast<unsigned>(sizeof(RemoveScratch)));
    return false;
  }
  GolfIndexLiveState liveIndex{};
  if (!recoverIndexState(scratch->indexMigrator, liveIndex)) return false;
  if (filename == nullptr || filename[0] == '\0' || strchr(filename, '/') != nullptr ||
      strchr(filename, '\\') != nullptr) {
    LOG_ERR("GOLF", "index delete rejected invalid round filename");
    return false;
  }
  const IndexRewriteResult rewrite = rewriteIndexWithout(filename, scratch->indexMigrator, liveIndex);
  if (!rewrite.committed()) return false;

  snprintf(scratch->path, sizeof(scratch->path), "%s/%s", ROUNDS_DIRECTORY, filename);
  if (Storage.exists(scratch->path) && !Storage.remove(scratch->path)) {
    LOG_ERR("GOLF", "completed round delete remove failed after index commit: %s", scratch->path);
  }
  if (rewrite.status == IndexRewriteStatus::CommittedCleanupPending) {
    LOG_ERR("GOLF", "completed round delete committed with index cleanup pending: %s", filename);
  }
  return true;
}

#endif
