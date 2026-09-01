#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfCsv.h"

using GolfIndexMigrateSink = bool (*)(const char* data, size_t size, void* user);

struct GolfIndexRecoveryOps {
  void* user;
  bool (*exists)(const char* path, void* user);
  bool (*validate)(const char* path, bool requireV4, void* user);
  bool (*remove)(const char* path, void* user);
  bool (*rename)(const char* from, const char* to, void* user);
};

enum class GolfIndexRecoveryStatus : uint8_t { NoIndex, Ready, Failed };

// Live is authoritative when present; otherwise backup is restored. A lone
// stage is discarded because syntax cannot prove a complete first group.
GolfIndexRecoveryStatus golfRecoverIndexArtifacts(const GolfIndexRecoveryOps& storage, const char* livePath,
                                                   const char* stagedPath, const char* backupPath);

struct GolfIndexLiveState {
  uint32_t rows = 0;
  GolfIndexVersion version = GolfIndexVersion::Unknown;
  bool present = false;
};

enum class GolfIndexStagePurpose : uint8_t { Migration, Append, Delete };
enum class GolfIndexArtifact : uint8_t { Live, Staged, Backup };
enum class GolfIndexStageWriteStatus : uint8_t { Complete, OpenFailed, ReadFailed, WriteFailed, SyncFailed };

enum class GolfIndexPublishError : uint8_t {
  None,
  InvalidState,
  PreserveFailed,
  PublishFailed,
  BackupRemoveFailed,
};

struct GolfIndexPublicationOps {
  void* user;
  bool (*remove)(GolfIndexArtifact artifact, GolfIndexStagePurpose purpose, void* user);
  bool (*rename)(GolfIndexArtifact from, GolfIndexArtifact to, GolfIndexStagePurpose purpose, void* user);
};

struct GolfIndexPublishResult {
  GolfIndexPublishError error = GolfIndexPublishError::None;
  bool published = false;
  bool cleanupPending = false;

  constexpr bool ok() const { return error == GolfIndexPublishError::None; }
  constexpr bool committed() const { return published; }
};

// Publishes a fully written and verified staging file. Once the staged-to-live
// rename succeeds, backup removal is cleanup and cannot undo the commit.
GolfIndexPublishResult golfPublishStagedIndex(bool hadOriginal, GolfIndexStagePurpose purpose,
                                              const GolfIndexPublicationOps& storage);

struct GolfIndexTransactionOps {
  void* user;
  GolfIndexStageWriteStatus (*writeStage)(GolfIndexStagePurpose purpose, GolfIndexLiveState live, void* user);
  bool (*verifyStage)(GolfIndexStagePurpose purpose, uint32_t expectedRows, void* user);
  bool (*remove)(GolfIndexArtifact artifact, GolfIndexStagePurpose purpose, void* user);
  bool (*rename)(GolfIndexArtifact from, GolfIndexArtifact to, GolfIndexStagePurpose purpose, void* user);
};

enum class GolfIndexTransactionError : uint8_t {
  None,
  InvalidState,
  MigrationStageOpenFailed,
  MigrationSourceReadFailed,
  MigrationStageWriteFailed,
  MigrationStageSyncFailed,
  MigrationStageVerifyFailed,
  MigrationPreserveFailed,
  MigrationPublishFailed,
  MigrationBackupRemoveFailed,
  AppendStageOpenFailed,
  AppendSourceReadFailed,
  AppendStageWriteFailed,
  AppendStageSyncFailed,
  AppendStageVerifyFailed,
  AppendPreserveFailed,
  AppendPublishFailed,
  AppendBackupRemoveFailed,
};

struct GolfIndexTransactionResult {
  GolfIndexTransactionError error = GolfIndexTransactionError::None;
  GolfIndexLiveState live{};
  bool appendCommitted = false;
  bool cleanupPending = false;

  constexpr bool ok() const { return error == GolfIndexTransactionError::None; }
};

// With appendRows == 0 this only normalizes a legacy live index. Otherwise it
// migrates if required, then appends one staged group. The supplied live state
// is authoritative: publication never probes for a live file between renames.
GolfIndexTransactionResult golfRunIndexTransaction(GolfIndexLiveState live, uint8_t appendRows,
                                                   const GolfIndexTransactionOps& storage);

bool golfIndexGroupRowsValid(uint8_t rowCount, uint8_t slotMask);

// Streams v2/v3 index rows into normalized v4 rows. It also verifies v4 files,
// counts one filename group, or rewrites an index while removing a whole group.
// Storage is one fixed row buffer; no heap or whole-file buffering is used.
class GolfIndexMigrator {
 public:
  void reset();
  void resetForStrictValidation(bool requireV4);
  bool resetForDelete(const char* filename);
  bool resetForGroupCount(const char* filename);
  bool feed(const char* data, size_t size, GolfIndexMigrateSink sink, void* user);
  bool finish();

  GolfIndexVersion sourceVersion() const { return sourceVersion_; }
  bool needsMigration() const {
    return sourceVersion_ == GolfIndexVersion::V2 || sourceVersion_ == GolfIndexVersion::V3;
  }
  uint32_t dataRows() const { return dataRows_; }
  uint32_t outputRows() const { return outputRows_; }
  uint8_t deletedRows() const { return groupRows_; }
  uint8_t groupRows() const { return groupRows_; }
  uint8_t groupSlotMask() const { return groupSlotMask_; }
  bool groupValid() const { return golfIndexGroupRowsValid(groupRows_, groupSlotMask_) && !duplicateGroupSlot_; }
  bool aborted() const { return aborted_; }

 private:
  char line_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  char output_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  uint32_t lineNumber_ = 1;
  uint32_t dataRows_ = 0;
  uint32_t outputRows_ = 0;
  uint16_t lineLength_ = 0;
  uint8_t groupRows_ = 0;
  uint8_t groupSlotMask_ = 0;
  bool lineOverflow_ = false;
  bool aborted_ = false;
  bool deleting_ = false;
  bool strictValidation_ = false;
  bool requireV4_ = false;
  bool duplicateGroupSlot_ = false;
  // Borrowed for the synchronous feed/finish pass; never retained by storage.
  const char* groupFilename_ = nullptr;
  GolfIndexVersion sourceVersion_ = GolfIndexVersion::Unknown;

  bool acceptLine(GolfIndexMigrateSink sink, void* user);
  bool parseSourceRow(GolfIndexRow& row) const;
  void recordGroupRow(uint8_t playerSlot);
};
