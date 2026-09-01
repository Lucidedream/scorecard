#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "GolfCsv.h"
#include "GolfIndexMigrate.h"

namespace {

struct Sink {
  std::string out;
  size_t allowBytes = SIZE_MAX;
};

bool sink(const char* data, const size_t size, void* user) {
  auto* output = static_cast<Sink*>(user);
  if (output->out.size() + size > output->allowBytes) return false;
  output->out.append(data, size);
  return true;
}

GolfIndexMigrator migrate(const std::string& input, Sink& output) {
  GolfIndexMigrator migrator;
  migrator.reset();
  for (size_t offset = 0; offset < input.size(); offset += 7) {
    const size_t remaining = input.size() - offset;
    if (!migrator.feed(input.data() + offset, remaining < 7 ? remaining : 7, &sink, &output)) break;
  }
  migrator.finish();
  return migrator;
}

GolfIndexMigrator rewriteDelete(const std::string& input, const char* filename, Sink& output) {
  GolfIndexMigrator rewrite;
  EXPECT_TRUE(rewrite.resetForDelete(filename));
  for (size_t offset = 0; offset < input.size(); offset += 7) {
    const size_t remaining = input.size() - offset;
    if (!rewrite.feed(input.data() + offset, remaining < 7 ? remaining : 7, &sink, &output)) break;
  }
  rewrite.finish();
  return rewrite;
}

bool simulatedAtomicDelete(std::string& live, const char* filename, const size_t allowBytes = SIZE_MAX) {
  Sink staged;
  staged.allowBytes = allowBytes;
  const GolfIndexMigrator rewrite = rewriteDelete(live, filename, staged);
  if (rewrite.aborted() || !rewrite.groupValid()) return false;
  live = staged.out;
  return true;
}

std::string v2Row(const char* course, const char* file) {
  return std::string(",") + course + ",18,82,72,33,52,30," + file + "\r\n";
}

std::string v3Row(const char* course, const char* file) {
  return std::string(",") + course + ",18,82,72,33,52,30,2,1," + file + "\r\n";
}

constexpr char LIVE[] = "index.csv";
constexpr char STAGED[] = "index.csv.new";
constexpr char BACKUP[] = "index.csv.bak";

const char* artifactPath(const GolfIndexArtifact artifact) {
  switch (artifact) {
    case GolfIndexArtifact::Live:
      return LIVE;
    case GolfIndexArtifact::Staged:
      return STAGED;
    case GolfIndexArtifact::Backup:
      return BACKUP;
  }
  return "invalid";
}

struct FakeArtifact {
  int generation;
  bool valid;
  bool v4;
};

struct FakeIndexStorage {
  std::map<std::string, FakeArtifact> files;
  std::vector<std::string> operations;
  std::string failedRenameSource;
  std::string failedRemovePath;
  bool stagedProbedBeforeBackupRestore = false;

  static bool exists(const char* path, void* user) {
    auto* self = static_cast<FakeIndexStorage*>(user);
    if (strcmp(path, STAGED) == 0 && self->files.find(BACKUP) != self->files.end()) {
      self->stagedProbedBeforeBackupRestore = true;
    }
    return self->files.find(path) != self->files.end();
  }

  static bool validate(const char* path, const bool requireV4, void* user) {
    auto* self = static_cast<FakeIndexStorage*>(user);
    self->operations.emplace_back(std::string("validate:") + path);
    const auto found = self->files.find(path);
    return found != self->files.end() && found->second.valid && (!requireV4 || found->second.v4);
  }

  static bool remove(const char* path, void* user) {
    auto* self = static_cast<FakeIndexStorage*>(user);
    self->operations.emplace_back(std::string("remove:") + path);
    if (self->failedRemovePath == path) return false;
    return self->files.erase(path) == 1;
  }

  static bool rename(const char* from, const char* to, void* user) {
    auto* self = static_cast<FakeIndexStorage*>(user);
    self->operations.emplace_back(std::string("rename:") + from + ":" + to);
    const auto found = self->files.find(from);
    if (found == self->files.end() || self->files.find(to) != self->files.end() || self->failedRenameSource == from) {
      return false;
    }
    self->files.emplace(to, found->second);
    self->files.erase(found);
    return true;
  }

  static bool removeArtifact(const GolfIndexArtifact artifact, GolfIndexStagePurpose, void* user) {
    return remove(artifactPath(artifact), user);
  }

  static bool renameArtifact(const GolfIndexArtifact from, const GolfIndexArtifact to, GolfIndexStagePurpose,
                             void* user) {
    return rename(artifactPath(from), artifactPath(to), user);
  }

  GolfIndexRecoveryOps ops() {
    return {this, &FakeIndexStorage::exists, &FakeIndexStorage::validate, &FakeIndexStorage::remove,
            &FakeIndexStorage::rename};
  }

  GolfIndexPublicationOps publicationOps() {
    return {this, &FakeIndexStorage::removeArtifact, &FakeIndexStorage::renameArtifact};
  }
};

GolfIndexRecoveryStatus recover(FakeIndexStorage& storage) {
  return golfRecoverIndexArtifacts(storage.ops(), LIVE, STAGED, BACKUP);
}

const char* purposeName(const GolfIndexStagePurpose purpose) {
  switch (purpose) {
    case GolfIndexStagePurpose::Migration:
      return "migration";
    case GolfIndexStagePurpose::Append:
      return "append";
    case GolfIndexStagePurpose::Delete:
      return "delete";
  }
  return "invalid";
}

const char* artifactName(const GolfIndexArtifact artifact) {
  switch (artifact) {
    case GolfIndexArtifact::Live:
      return "live";
    case GolfIndexArtifact::Staged:
      return "staged";
    case GolfIndexArtifact::Backup:
      return "backup";
  }
  return "invalid";
}

struct FakeIndexTransaction {
  std::vector<std::string> operations;
  GolfIndexStageWriteStatus migrationWrite = GolfIndexStageWriteStatus::Complete;
  GolfIndexStageWriteStatus appendWrite = GolfIndexStageWriteStatus::Complete;
  bool migrationVerify = true;
  bool appendVerify = true;
  bool failRemove = false;
  GolfIndexArtifact failedRemove = GolfIndexArtifact::Staged;
  bool failRename = false;
  GolfIndexArtifact failedRenameFrom = GolfIndexArtifact::Live;
  GolfIndexArtifact failedRenameTo = GolfIndexArtifact::Backup;

  static GolfIndexStageWriteStatus writeStage(const GolfIndexStagePurpose purpose, const GolfIndexLiveState live,
                                              void* user) {
    auto* self = static_cast<FakeIndexTransaction*>(user);
    const GolfIndexStageWriteStatus status = purpose == GolfIndexStagePurpose::Migration
                                                 ? self->migrationWrite
                                                 : self->appendWrite;
    self->operations.emplace_back(std::string("open:") + purposeName(purpose));
    if (status == GolfIndexStageWriteStatus::OpenFailed) return status;
    if (status == GolfIndexStageWriteStatus::ReadFailed) {
      self->operations.emplace_back(std::string("read-fail:") + purposeName(purpose));
      return status;
    }
    if (status == GolfIndexStageWriteStatus::WriteFailed) {
      self->operations.emplace_back(std::string("write-fail:") + purposeName(purpose));
      return status;
    }
    self->operations.emplace_back(std::string("write:") + purposeName(purpose) +
                                  (live.present ? ":with-live" : ":without-live"));
    self->operations.emplace_back(std::string("sync:") + purposeName(purpose) +
                                  (status == GolfIndexStageWriteStatus::SyncFailed ? ":fail" : ":ok"));
    return status;
  }

  static bool verifyStage(const GolfIndexStagePurpose purpose, const uint32_t expectedRows, void* user) {
    auto* self = static_cast<FakeIndexTransaction*>(user);
    self->operations.emplace_back(std::string("verify:") + purposeName(purpose) + ":" +
                                  std::to_string(expectedRows));
    return purpose == GolfIndexStagePurpose::Migration ? self->migrationVerify : self->appendVerify;
  }

  static bool remove(const GolfIndexArtifact artifact, const GolfIndexStagePurpose purpose, void* user) {
    auto* self = static_cast<FakeIndexTransaction*>(user);
    self->operations.emplace_back(std::string("remove:") + artifactName(artifact) + ":" + purposeName(purpose));
    return !self->failRemove || artifact != self->failedRemove;
  }

  static bool rename(const GolfIndexArtifact from, const GolfIndexArtifact to,
                     const GolfIndexStagePurpose purpose, void* user) {
    auto* self = static_cast<FakeIndexTransaction*>(user);
    self->operations.emplace_back(std::string("rename:") + artifactName(from) + ":" + artifactName(to) + ":" +
                                  purposeName(purpose));
    return !self->failRename || from != self->failedRenameFrom || to != self->failedRenameTo;
  }

  GolfIndexTransactionOps ops() {
    return {this, &FakeIndexTransaction::writeStage, &FakeIndexTransaction::verifyStage,
            &FakeIndexTransaction::remove, &FakeIndexTransaction::rename};
  }

  GolfIndexPublicationOps publicationOps() {
    return {this, &FakeIndexTransaction::remove, &FakeIndexTransaction::rename};
  }
};

bool traceContains(const std::vector<std::string>& operations, const char* fragment) {
  return std::any_of(operations.begin(), operations.end(), [fragment](const std::string& operation) {
    return operation.find(fragment) != std::string::npos;
  });
}

std::string v4Row(const uint8_t slot, const char* name, const char* file, const uint16_t strokes = 82) {
  GolfIndexRow row{};
  strcpy(row.course, "Course");
  strcpy(row.playerName, name);
  strcpy(row.file, file);
  row.holes = 18;
  row.playerSlot = slot;
  row.strokes = strokes;
  row.par = 72;
  row.putts = 33;
  row.in100 = 52;
  row.out100 = 30;
  row.hazards = 2;
  row.obs = 1;
  row.penaltiesRecorded = true;
  char output[GOLF_CSV_ROW_BUFFER_SIZE];
  EXPECT_TRUE(golfFormatIndexRow(row, output, sizeof(output)));
  return output;
}

}  // namespace

TEST(GolfIndexHeaderVersion, ClassifiesAllHeaders) {
  EXPECT_EQ(golfIndexHeaderVersion(GOLF_INDEX_HEADER_V2), GolfIndexVersion::V2);
  EXPECT_EQ(golfIndexHeaderVersion(GOLF_INDEX_HEADER_V3), GolfIndexVersion::V3);
  EXPECT_EQ(golfIndexHeaderVersion(GOLF_INDEX_HEADER_V4), GolfIndexVersion::V4);
  EXPECT_EQ(golfIndexHeaderVersion("something,else"), GolfIndexVersion::Unknown);
}

TEST(GolfIndexMigrate, V2RowsNormalizeToSlotZeroNoah) {
  const std::string input = std::string(GOLF_INDEX_HEADER_V2) + "\r\n" +
                            v2Row("Pebble", "round-0001.json") + v2Row("Sanyang", "round-0002.json");
  Sink output;
  const GolfIndexMigrator migrator = migrate(input, output);

  EXPECT_EQ(migrator.sourceVersion(), GolfIndexVersion::V2);
  EXPECT_TRUE(migrator.needsMigration());
  EXPECT_FALSE(migrator.aborted());
  EXPECT_EQ(migrator.dataRows(), 2u);
  ASSERT_EQ(output.out.rfind(GOLF_INDEX_HEADER, 0), 0u);

  const size_t firstLine = output.out.find('\n') + 1;
  const size_t firstEnd = output.out.find('\n', firstLine);
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(output.out.substr(firstLine, firstEnd - firstLine + 1).c_str(), parsed));
  EXPECT_EQ(parsed.playerSlot, 0);
  EXPECT_STREQ(parsed.playerName, "Noah");
  EXPECT_FALSE(parsed.penaltiesRecorded);
}

TEST(GolfIndexMigrate, V3RowsNormalizeAndPreservePenaltyRecording) {
  const std::string input = std::string(GOLF_INDEX_HEADER_V3) + "\r\n" + v3Row("Pebble", "round-0001.json");
  Sink output;
  const GolfIndexMigrator migrator = migrate(input, output);
  EXPECT_EQ(migrator.sourceVersion(), GolfIndexVersion::V3);
  EXPECT_TRUE(migrator.needsMigration());

  GolfIndexRow parsed{};
  const size_t firstLine = output.out.find('\n') + 1;
  ASSERT_TRUE(golfParseIndexRow(output.out.substr(firstLine).c_str(), parsed));
  EXPECT_EQ(parsed.playerSlot, 0);
  EXPECT_STREQ(parsed.playerName, "Noah");
  EXPECT_TRUE(parsed.penaltiesRecorded);
  EXPECT_EQ(parsed.hazards, 2);
  EXPECT_EQ(parsed.obs, 1);
}

TEST(GolfIndexMigrate, AlreadyV4FileIsOnlyVerified) {
  const std::string input = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round-0001.json");
  Sink output;
  const GolfIndexMigrator migrator = migrate(input, output);
  EXPECT_EQ(migrator.sourceVersion(), GolfIndexVersion::V4);
  EXPECT_FALSE(migrator.needsMigration());
  EXPECT_EQ(migrator.dataRows(), 1u);
  EXPECT_TRUE(output.out.empty());
}

TEST(GolfIndexMigrate, V4RejectsLegacyShapeAndMalformedNonemptyRows) {
  const std::string legacyUnderV4 =
      std::string(GOLF_INDEX_HEADER) + v2Row("Legacy", "round-old.json");
  Sink output;
  const GolfIndexMigrator legacy = migrate(legacyUnderV4, output);
  EXPECT_EQ(legacy.sourceVersion(), GolfIndexVersion::V4);
  EXPECT_TRUE(legacy.aborted());
  EXPECT_EQ(legacy.dataRows(), 0u);

  output = {};
  const GolfIndexMigrator malformed = migrate(std::string(GOLF_INDEX_HEADER) + "not,a,v4,row\r\n", output);
  EXPECT_TRUE(malformed.aborted());
}

TEST(GolfIndexMigrate, V4RejectsUnterminatedTailAsIncomplete) {
  Sink output;
  const GolfIndexMigrator migrator =
      migrate(std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round.json") + ",Course,18,0,Noah", output);
  EXPECT_TRUE(migrator.aborted());
  EXPECT_EQ(migrator.dataRows(), 1u);
}

TEST(GolfIndexMigrate, ExplicitLegacyMigrationRetainsMixedV2V3Tolerance) {
  const std::string input = std::string(GOLF_INDEX_HEADER_V2) + "\r\n" +
                            v2Row("Old", "round-old.json") + v3Row("New", "round-new.json");
  Sink output;
  const GolfIndexMigrator migrator = migrate(input, output);
  EXPECT_FALSE(migrator.aborted());
  EXPECT_EQ(migrator.dataRows(), 2u);
  EXPECT_EQ(std::count(output.out.begin(), output.out.end(), '\n'), 3);
}

TEST(GolfIndexMigrate, QuotedLegacyCourseAndV4PlayerNameRoundTrip) {
  const std::string legacy = std::string(GOLF_INDEX_HEADER_V2) + "\r\n" +
                             ",\"Course, \"\"One\"\"\",18,82,72,33,52,30,round-old.json\r\n";
  Sink migrated;
  const GolfIndexMigrator migration = migrate(legacy, migrated);
  ASSERT_FALSE(migration.aborted());
  GolfIndexRow legacyParsed{};
  const size_t firstLine = migrated.out.find('\n') + 1;
  ASSERT_TRUE(golfParseIndexRow(migrated.out.substr(firstLine).c_str(), legacyParsed));
  EXPECT_STREQ(legacyParsed.course, "Course, \"One\"");
  EXPECT_STREQ(legacyParsed.playerName, "Noah");

  GolfIndexRow source{};
  strcpy(source.course, "Course, \"One\"");
  strcpy(source.playerName, "A, \"B\"");
  strcpy(source.file, "round-0001.json");
  source.holes = 18;
  source.playerSlot = 2;
  source.strokes = 80;
  source.par = 72;
  source.penaltiesRecorded = true;
  char encoded[GOLF_CSV_ROW_BUFFER_SIZE];
  ASSERT_TRUE(golfFormatIndexRow(source, encoded, sizeof(encoded)));
  GolfIndexRow parsed{};
  ASSERT_TRUE(golfParseIndexRow(encoded, parsed));
  EXPECT_STREQ(parsed.course, source.course);
  EXPECT_STREQ(parsed.playerName, source.playerName);
}

TEST(GolfIndexMigrate, MalformedRowsAndUnterminatedTailAreDroppedDuringMigration) {
  const std::string input = std::string(GOLF_INDEX_HEADER_V2) + "\r\n" + v2Row("Pebble", "round-0001.json") +
                            "garbage,not,a,row\r\n" + ",Sanyang,18,90";
  Sink output;
  const GolfIndexMigrator migrator = migrate(input, output);
  EXPECT_EQ(migrator.dataRows(), 1u);
  EXPECT_EQ(std::count(output.out.begin(), output.out.end(), '\n'), 2);
}

TEST(GolfIndexMigrate, WriteFailureAbortsWithoutClaimingSuccess) {
  const std::string input = std::string(GOLF_INDEX_HEADER_V3) + "\r\n" + v3Row("Pebble", "round-0001.json");
  Sink output;
  output.allowBytes = sizeof(GOLF_INDEX_HEADER) + 4;
  const GolfIndexMigrator migrator = migrate(input, output);
  EXPECT_TRUE(migrator.aborted());
}

TEST(GolfIndexGroup, ValidatesOneToFourDistinctStableSlots) {
  EXPECT_TRUE(golfIndexGroupRowsValid(1, 0x01));
  EXPECT_TRUE(golfIndexGroupRowsValid(4, 0x0f));
  EXPECT_TRUE(golfIndexGroupRowsValid(2, 0x05));
  EXPECT_FALSE(golfIndexGroupRowsValid(0, 0));
  EXPECT_FALSE(golfIndexGroupRowsValid(2, 0x01));
  EXPECT_FALSE(golfIndexGroupRowsValid(1, 0x11));
  EXPECT_FALSE(golfIndexGroupRowsValid(5, 0x0f));
}

TEST(GolfIndexGroup, CounterFindsExactFilenameAndDistinctSlotMask) {
  const char* target = "round-0002.json";
  const std::string input = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", target) +
                            v4Row(2, "Guest", target) + v4Row(1, "Other", "round-0003.json");
  GolfIndexMigrator counter;
  ASSERT_TRUE(counter.resetForGroupCount(target));
  ASSERT_TRUE(counter.feed(input.data(), input.size(), nullptr, nullptr));
  ASSERT_TRUE(counter.finish());
  EXPECT_EQ(counter.sourceVersion(), GolfIndexVersion::V4);
  EXPECT_EQ(counter.dataRows(), 3u);
  EXPECT_EQ(counter.groupRows(), 2);
  EXPECT_EQ(counter.groupSlotMask(), 0x05);
  EXPECT_TRUE(counter.groupValid());
}

TEST(GolfIndexTransaction, FirstArchivePublishesDirectlyWithoutRenamingAbsentLive) {
  FakeIndexTransaction transaction;
  const GolfIndexTransactionResult result = golfRunIndexTransaction({}, 2, transaction.ops());

  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.appendCommitted);
  EXPECT_TRUE(result.live.present);
  EXPECT_EQ(result.live.version, GolfIndexVersion::V4);
  EXPECT_EQ(result.live.rows, 2u);
  EXPECT_FALSE(traceContains(transaction.operations, "migration"));
  EXPECT_FALSE(traceContains(transaction.operations, "rename:live:backup"));
  const std::vector<std::string> expected{"open:append", "write:append:without-live", "sync:append:ok",
                                          "verify:append:2", "rename:staged:live:append"};
  EXPECT_EQ(transaction.operations, expected);
}

TEST(GolfIndexTransaction, ValidV4SkipsThrowawayMigrationStage) {
  const GolfIndexLiveState live{7, GolfIndexVersion::V4, true};
  FakeIndexTransaction migrationOnly;
  const GolfIndexTransactionResult unchanged = golfRunIndexTransaction(live, 0, migrationOnly.ops());
  ASSERT_TRUE(unchanged.ok());
  EXPECT_TRUE(migrationOnly.operations.empty());

  FakeIndexTransaction transaction;
  const GolfIndexTransactionResult result = golfRunIndexTransaction(live, 1, transaction.ops());
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.appendCommitted);
  EXPECT_EQ(result.live.rows, 8u);
  EXPECT_FALSE(traceContains(transaction.operations, "migration"));
  const std::vector<std::string> expected{
      "open:append",           "write:append:with-live",      "sync:append:ok",
      "verify:append:8",       "rename:live:backup:append",   "rename:staged:live:append",
      "remove:backup:append",
  };
  EXPECT_EQ(transaction.operations, expected);
}

TEST(GolfIndexTransaction, LegacyMigrationCleansArtifactsBeforeAppend) {
  FakeIndexTransaction transaction;
  const GolfIndexLiveState legacy{3, GolfIndexVersion::V3, true};
  const GolfIndexTransactionResult result = golfRunIndexTransaction(legacy, 2, transaction.ops());

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.appendCommitted);
  EXPECT_EQ(result.live.version, GolfIndexVersion::V4);
  EXPECT_EQ(result.live.rows, 5u);
  const std::vector<std::string> expected{
      "open:migration",
      "write:migration:with-live",
      "sync:migration:ok",
      "verify:migration:3",
      "rename:live:backup:migration",
      "rename:staged:live:migration",
      "remove:backup:migration",
      "open:append",
      "write:append:with-live",
      "sync:append:ok",
      "verify:append:5",
      "rename:live:backup:append",
      "rename:staged:live:append",
      "remove:backup:append",
  };
  EXPECT_EQ(transaction.operations, expected);
}

TEST(GolfIndexTransaction, UndeletableMigrationBackupStopsBeforeAppendAndRetainsV4LiveState) {
  FakeIndexTransaction transaction;
  transaction.failRemove = true;
  transaction.failedRemove = GolfIndexArtifact::Backup;
  const GolfIndexLiveState legacy{3, GolfIndexVersion::V2, true};
  const GolfIndexTransactionResult result = golfRunIndexTransaction(legacy, 1, transaction.ops());

  EXPECT_EQ(result.error, GolfIndexTransactionError::MigrationBackupRemoveFailed);
  EXPECT_FALSE(result.appendCommitted);
  EXPECT_TRUE(result.cleanupPending);
  EXPECT_TRUE(result.live.present);
  EXPECT_EQ(result.live.version, GolfIndexVersion::V4);
  EXPECT_EQ(result.live.rows, 3u);
  EXPECT_FALSE(traceContains(transaction.operations, "open:append"));
  EXPECT_EQ(transaction.operations.back(), "remove:backup:migration");
}

TEST(GolfIndexTransaction, SyncFailureRemovesStageBeforeAnyVerifyOrRename) {
  FakeIndexTransaction transaction;
  transaction.appendWrite = GolfIndexStageWriteStatus::SyncFailed;
  const GolfIndexTransactionResult result = golfRunIndexTransaction({}, 1, transaction.ops());

  EXPECT_EQ(result.error, GolfIndexTransactionError::AppendStageSyncFailed);
  EXPECT_FALSE(result.appendCommitted);
  EXPECT_FALSE(result.cleanupPending);
  EXPECT_TRUE(traceContains(transaction.operations, "sync:append:fail"));
  EXPECT_TRUE(traceContains(transaction.operations, "remove:staged:append"));
  EXPECT_FALSE(traceContains(transaction.operations, "verify:"));
  EXPECT_FALSE(traceContains(transaction.operations, "rename:"));
}

TEST(GolfIndexTransaction, SyncFailureWithUndeletableStageIsReportedAsCleanupPending) {
  FakeIndexTransaction transaction;
  transaction.appendWrite = GolfIndexStageWriteStatus::SyncFailed;
  transaction.failRemove = true;
  transaction.failedRemove = GolfIndexArtifact::Staged;
  const GolfIndexTransactionResult result = golfRunIndexTransaction({}, 1, transaction.ops());

  EXPECT_EQ(result.error, GolfIndexTransactionError::AppendStageSyncFailed);
  EXPECT_FALSE(result.appendCommitted);
  EXPECT_TRUE(result.cleanupPending);
  EXPECT_EQ(transaction.operations.back(), "remove:staged:append");
}

TEST(GolfIndexTransaction, PublishedAppendWithUndeletableBackupIsCommittedButNeedsCleanup) {
  FakeIndexTransaction transaction;
  transaction.failRemove = true;
  transaction.failedRemove = GolfIndexArtifact::Backup;
  const GolfIndexLiveState live{4, GolfIndexVersion::V4, true};
  const GolfIndexTransactionResult result = golfRunIndexTransaction(live, 1, transaction.ops());

  EXPECT_EQ(result.error, GolfIndexTransactionError::AppendBackupRemoveFailed);
  EXPECT_TRUE(result.appendCommitted);
  EXPECT_TRUE(result.cleanupPending);
  EXPECT_EQ(result.live.rows, 5u);
  EXPECT_EQ(transaction.operations.back(), "remove:backup:append");
}

TEST(GolfIndexDeleteTransaction, PublishedDeleteWithUndeletableBackupIsCommittedCleanupPending) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  storage.failedRemovePath = BACKUP;

  const GolfIndexPublishResult result =
      golfPublishStagedIndex(true, GolfIndexStagePurpose::Delete, storage.publicationOps());

  EXPECT_EQ(result.error, GolfIndexPublishError::BackupRemoveFailed);
  EXPECT_TRUE(result.committed());
  EXPECT_TRUE(result.cleanupPending);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 2);
  ASSERT_EQ(storage.files.count(BACKUP), 1u);
  EXPECT_EQ(storage.files.at(BACKUP).generation, 1);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
}

TEST(GolfIndexDeleteTransaction, RecoveryFinishesPublishedDeleteAndAbsentRetryIsIdempotent) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  storage.failedRemovePath = BACKUP;
  ASSERT_TRUE(golfPublishStagedIndex(true, GolfIndexStagePurpose::Delete, storage.publicationOps()).committed());

  storage.failedRemovePath.clear();
  ASSERT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 2);

  const char* deleted = "round-deleted.json";
  const std::string live = std::string(GOLF_INDEX_HEADER) + v4Row(1, "Keep", "round-keep.json");
  Sink retryStage;
  const GolfIndexMigrator retry = rewriteDelete(live, deleted, retryStage);
  EXPECT_FALSE(retry.aborted());
  EXPECT_EQ(retry.groupRows(), 0);
  EXPECT_EQ(retry.groupSlotMask(), 0);
  EXPECT_EQ(retryStage.out, live);
}

TEST(GolfIndexDeleteTransaction, PreserveFailureRemainsBeforeCommit) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  storage.failedRenameSource = LIVE;

  const GolfIndexPublishResult result =
      golfPublishStagedIndex(true, GolfIndexStagePurpose::Delete, storage.publicationOps());

  EXPECT_EQ(result.error, GolfIndexPublishError::PreserveFailed);
  EXPECT_FALSE(result.committed());
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 1);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
}

TEST(GolfIndexTransaction, StaleBackupRecoveryFailurePreventsAppendUntilCleanupSucceeds) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{2, true, true});
  storage.files.emplace(BACKUP, FakeArtifact{1, true, true});
  storage.failedRemovePath = BACKUP;
  FakeIndexTransaction transaction;

  ASSERT_EQ(recover(storage), GolfIndexRecoveryStatus::Failed);
  EXPECT_TRUE(transaction.operations.empty());
  EXPECT_EQ(storage.files.count(BACKUP), 1u);

  storage.failedRemovePath.clear();
  ASSERT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  const GolfIndexLiveState live{4, GolfIndexVersion::V4, true};
  const GolfIndexTransactionResult result = golfRunIndexTransaction(live, 1, transaction.ops());
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.appendCommitted);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
  EXPECT_EQ(transaction.operations.front(), "open:append");
}

TEST(GolfIndexDelete, RemovesAllRowsForOneGroupAndPreservesOthers) {
  const char* target = "round-0002.json";
  const std::string input = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round-0001.json") +
                            v4Row(0, "Noah", target) + v4Row(2, "Guest", target) +
                            v4Row(3, "Fourth", target) + v4Row(1, "B", "round-0003.json");
  Sink staged;
  const GolfIndexMigrator rewrite = rewriteDelete(input, target, staged);
  ASSERT_FALSE(rewrite.aborted());
  EXPECT_TRUE(rewrite.groupValid());
  EXPECT_EQ(rewrite.deletedRows(), 3);
  EXPECT_EQ(rewrite.groupSlotMask(), 0x0d);
  EXPECT_EQ(rewrite.outputRows(), 2u);
  EXPECT_EQ(staged.out.find(target), std::string::npos);
  EXPECT_NE(staged.out.find("round-0001.json"), std::string::npos);
  EXPECT_NE(staged.out.find("round-0003.json"), std::string::npos);
}

TEST(GolfIndexDelete, DuplicateSlotOrMissingGroupNeverBecomesLive) {
  std::string duplicate = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round.json") +
                          v4Row(0, "Duplicate", "round.json");
  const std::string before = duplicate;
  EXPECT_FALSE(simulatedAtomicDelete(duplicate, "round.json"));
  EXPECT_EQ(duplicate, before);

  std::string missing = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "other.json");
  const std::string missingBefore = missing;
  EXPECT_FALSE(simulatedAtomicDelete(missing, "round.json"));
  EXPECT_EQ(missing, missingBefore);
}

TEST(GolfIndexDelete, FailedStagedWriteLeavesLiveIndexByteIdentical) {
  std::string live = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round.json") +
                     v4Row(2, "Guest", "round.json") + v4Row(1, "Keep", "other.json");
  const std::string before = live;
  EXPECT_FALSE(simulatedAtomicDelete(live, "round.json", sizeof(GOLF_INDEX_HEADER) + 4));
  EXPECT_EQ(live, before);
}

TEST(GolfIndexRecovery, PowerCutBeforeFirstRenameKeepsValidatedLiveAndDropsStaging) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 1);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
}

TEST(GolfIndexRecovery, SecondIndependentReadRecoversNewBackupAndStagingArtifacts) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{1, true, true});
  ASSERT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);

  storage.files.erase(LIVE);
  storage.files.emplace(BACKUP, FakeArtifact{2, true, true});
  storage.files.emplace(STAGED, FakeArtifact{3, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 2);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
  EXPECT_FALSE(storage.stagedProbedBeforeBackupRestore);
}

TEST(GolfIndexRecovery, PowerCutAfterLiveToBackupRestoresBackupBeforeTouchingStaging) {
  FakeIndexStorage storage;
  storage.files.emplace(BACKUP, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 1);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
  EXPECT_FALSE(storage.stagedProbedBeforeBackupRestore);
  ASSERT_GE(storage.operations.size(), 3u);
  EXPECT_EQ(storage.operations[0], "rename:index.csv.bak:index.csv");
  EXPECT_EQ(storage.operations[1], "validate:index.csv");
  EXPECT_EQ(storage.operations[2], "remove:index.csv.new");
}

TEST(GolfIndexRecovery, PowerCutAfterStagedToLiveKeepsNewLiveAndDropsBackup) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{2, true, true});
  storage.files.emplace(BACKUP, FakeArtifact{1, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 2);
  EXPECT_EQ(storage.files.count(BACKUP), 0u);
}

TEST(GolfIndexRecovery, PowerCutAfterBackupDeleteLeavesCompleteLiveUntouched) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{2, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Ready);
  ASSERT_EQ(storage.files.count(LIVE), 1u);
  EXPECT_EQ(storage.files.at(LIVE).generation, 2);
}

TEST(GolfIndexRecovery, LoneSyntacticallyValidV4StageIsDiscardedInsteadOfPublished) {
  FakeIndexStorage storage;
  // This can be a clean row-boundary cut after only part of a first group.
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});

  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::NoIndex);
  EXPECT_EQ(storage.files.count(LIVE), 0u);
  EXPECT_EQ(storage.files.count(STAGED), 0u);
  const std::vector<std::string> expected{"remove:index.csv.new"};
  EXPECT_EQ(storage.operations, expected);
}

TEST(GolfIndexRecovery, EveryLoneStageIsDiscardedWithoutUsingItsSyntaxAsAuthority) {
  FakeIndexStorage legacy;
  legacy.files.emplace(STAGED, FakeArtifact{1, true, false});
  EXPECT_EQ(recover(legacy), GolfIndexRecoveryStatus::NoIndex);
  EXPECT_EQ(legacy.files.count(STAGED), 0u);
  EXPECT_FALSE(traceContains(legacy.operations, "validate:"));

  FakeIndexStorage malformed;
  malformed.files.emplace(STAGED, FakeArtifact{2, false, true});
  EXPECT_EQ(recover(malformed), GolfIndexRecoveryStatus::NoIndex);
  EXPECT_EQ(malformed.files.count(STAGED), 0u);
  EXPECT_FALSE(traceContains(malformed.operations, "validate:"));
}

TEST(GolfIndexRecovery, UndeletableLoneStageFailsRecoveryAndRemainsUnpublished) {
  FakeIndexStorage storage;
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  storage.failedRemovePath = STAGED;

  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Failed);
  EXPECT_EQ(storage.files.count(LIVE), 0u);
  EXPECT_EQ(storage.files.count(STAGED), 1u);
  const std::vector<std::string> expected{"remove:index.csv.new"};
  EXPECT_EQ(storage.operations, expected);
}

TEST(GolfIndexRecovery, FailedBackupRestoreNeverTouchesRecoverableStaging) {
  FakeIndexStorage storage;
  storage.files.emplace(BACKUP, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{2, true, true});
  storage.failedRenameSource = BACKUP;
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Failed);
  EXPECT_EQ(storage.files.count(BACKUP), 1u);
  EXPECT_EQ(storage.files.count(STAGED), 1u);
  EXPECT_FALSE(storage.stagedProbedBeforeBackupRestore);
  ASSERT_EQ(storage.operations.size(), 1u);
  EXPECT_EQ(storage.operations[0], "rename:index.csv.bak:index.csv");
}

TEST(GolfIndexRecovery, InvalidLivePreservesEveryArtifactForManualRecovery) {
  FakeIndexStorage storage;
  storage.files.emplace(LIVE, FakeArtifact{2, false, true});
  storage.files.emplace(BACKUP, FakeArtifact{1, true, true});
  storage.files.emplace(STAGED, FakeArtifact{3, true, true});
  EXPECT_EQ(recover(storage), GolfIndexRecoveryStatus::Failed);
  EXPECT_EQ(storage.files.size(), 3u);
  ASSERT_EQ(storage.operations.size(), 1u);
  EXPECT_EQ(storage.operations[0], "validate:index.csv");
}

TEST(GolfIndexDelete, DeletingOnlyGroupLeavesValidHeaderOnlyIndex) {
  std::string live = std::string(GOLF_INDEX_HEADER) + v4Row(0, "Noah", "round.json") +
                     v4Row(2, "Guest", "round.json");
  ASSERT_TRUE(simulatedAtomicDelete(live, "round.json"));
  EXPECT_EQ(live, GOLF_INDEX_HEADER);
}
