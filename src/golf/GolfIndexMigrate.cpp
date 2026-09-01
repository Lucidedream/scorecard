#include "GolfIndexMigrate.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool emit(const GolfIndexMigrateSink sink, void* user, const char* text) {
  return sink == nullptr || sink(text, strlen(text), user);
}

GolfIndexTransactionError stageWriteError(const GolfIndexStagePurpose purpose,
                                          const GolfIndexStageWriteStatus status) {
  if (purpose == GolfIndexStagePurpose::Delete) return GolfIndexTransactionError::InvalidState;
  if (purpose == GolfIndexStagePurpose::Migration) {
    switch (status) {
      case GolfIndexStageWriteStatus::OpenFailed:
        return GolfIndexTransactionError::MigrationStageOpenFailed;
      case GolfIndexStageWriteStatus::ReadFailed:
        return GolfIndexTransactionError::MigrationSourceReadFailed;
      case GolfIndexStageWriteStatus::WriteFailed:
        return GolfIndexTransactionError::MigrationStageWriteFailed;
      case GolfIndexStageWriteStatus::SyncFailed:
        return GolfIndexTransactionError::MigrationStageSyncFailed;
      case GolfIndexStageWriteStatus::Complete:
        break;
    }
  } else {
    switch (status) {
      case GolfIndexStageWriteStatus::OpenFailed:
        return GolfIndexTransactionError::AppendStageOpenFailed;
      case GolfIndexStageWriteStatus::ReadFailed:
        return GolfIndexTransactionError::AppendSourceReadFailed;
      case GolfIndexStageWriteStatus::WriteFailed:
        return GolfIndexTransactionError::AppendStageWriteFailed;
      case GolfIndexStageWriteStatus::SyncFailed:
        return GolfIndexTransactionError::AppendStageSyncFailed;
      case GolfIndexStageWriteStatus::Complete:
        break;
    }
  }
  return GolfIndexTransactionError::InvalidState;
}

GolfIndexTransactionError stageVerifyError(const GolfIndexStagePurpose purpose) {
  switch (purpose) {
    case GolfIndexStagePurpose::Migration:
      return GolfIndexTransactionError::MigrationStageVerifyFailed;
    case GolfIndexStagePurpose::Append:
      return GolfIndexTransactionError::AppendStageVerifyFailed;
    case GolfIndexStagePurpose::Delete:
      return GolfIndexTransactionError::InvalidState;
  }
  return GolfIndexTransactionError::InvalidState;
}

GolfIndexTransactionError publishTransactionError(const GolfIndexStagePurpose purpose,
                                                   const GolfIndexPublishError error) {
  if (purpose == GolfIndexStagePurpose::Delete) return GolfIndexTransactionError::InvalidState;
  switch (error) {
    case GolfIndexPublishError::None:
      return GolfIndexTransactionError::None;
    case GolfIndexPublishError::PreserveFailed:
      return purpose == GolfIndexStagePurpose::Migration ? GolfIndexTransactionError::MigrationPreserveFailed
                                                          : GolfIndexTransactionError::AppendPreserveFailed;
    case GolfIndexPublishError::PublishFailed:
      return purpose == GolfIndexStagePurpose::Migration ? GolfIndexTransactionError::MigrationPublishFailed
                                                          : GolfIndexTransactionError::AppendPublishFailed;
    case GolfIndexPublishError::BackupRemoveFailed:
      return purpose == GolfIndexStagePurpose::Migration
                 ? GolfIndexTransactionError::MigrationBackupRemoveFailed
                 : GolfIndexTransactionError::AppendBackupRemoveFailed;
    case GolfIndexPublishError::InvalidState:
      return GolfIndexTransactionError::InvalidState;
  }
  return GolfIndexTransactionError::InvalidState;
}

struct StageTransactionResult {
  GolfIndexTransactionError error = GolfIndexTransactionError::None;
  bool published = false;
  bool cleanupPending = false;
};

StageTransactionResult runStageTransaction(const GolfIndexStagePurpose purpose, const GolfIndexLiveState live,
                                           const uint32_t expectedRows, const GolfIndexTransactionOps& storage) {
  StageTransactionResult result{};
  const GolfIndexStageWriteStatus writeStatus = storage.writeStage(purpose, live, storage.user);
  if (writeStatus != GolfIndexStageWriteStatus::Complete) {
    result.error = stageWriteError(purpose, writeStatus);
    if (writeStatus != GolfIndexStageWriteStatus::OpenFailed &&
        !storage.remove(GolfIndexArtifact::Staged, purpose, storage.user)) {
      result.cleanupPending = true;
    }
    return result;
  }

  if (!storage.verifyStage(purpose, expectedRows, storage.user)) {
    result.error = stageVerifyError(purpose);
    if (!storage.remove(GolfIndexArtifact::Staged, purpose, storage.user)) result.cleanupPending = true;
    return result;
  }

  const GolfIndexPublicationOps publication{storage.user, storage.remove, storage.rename};
  const GolfIndexPublishResult published = golfPublishStagedIndex(live.present, purpose, publication);
  result.error = publishTransactionError(purpose, published.error);
  result.published = published.published;
  result.cleanupPending = published.cleanupPending;
  return result;
}

bool transactionOpsValid(const GolfIndexTransactionOps& storage) {
  return storage.writeStage != nullptr && storage.verifyStage != nullptr && storage.remove != nullptr &&
         storage.rename != nullptr;
}

}  // namespace

GolfIndexPublishResult golfPublishStagedIndex(const bool hadOriginal, const GolfIndexStagePurpose purpose,
                                              const GolfIndexPublicationOps& storage) {
  GolfIndexPublishResult result{};
  if (storage.remove == nullptr || storage.rename == nullptr) {
    result.error = GolfIndexPublishError::InvalidState;
    return result;
  }

  if (hadOriginal &&
      !storage.rename(GolfIndexArtifact::Live, GolfIndexArtifact::Backup, purpose, storage.user)) {
    result.error = GolfIndexPublishError::PreserveFailed;
    if (!storage.remove(GolfIndexArtifact::Staged, purpose, storage.user)) result.cleanupPending = true;
    return result;
  }

  if (!storage.rename(GolfIndexArtifact::Staged, GolfIndexArtifact::Live, purpose, storage.user)) {
    result.error = GolfIndexPublishError::PublishFailed;
    if (hadOriginal) {
      if (!storage.rename(GolfIndexArtifact::Backup, GolfIndexArtifact::Live, purpose, storage.user)) {
        result.cleanupPending = true;
      } else if (!storage.remove(GolfIndexArtifact::Staged, purpose, storage.user)) {
        result.cleanupPending = true;
      }
    } else if (!storage.remove(GolfIndexArtifact::Staged, purpose, storage.user)) {
      result.cleanupPending = true;
    }
    return result;
  }

  result.published = true;
  if (hadOriginal && !storage.remove(GolfIndexArtifact::Backup, purpose, storage.user)) {
    result.error = GolfIndexPublishError::BackupRemoveFailed;
    result.cleanupPending = true;
  }
  return result;
}

GolfIndexTransactionResult golfRunIndexTransaction(GolfIndexLiveState live, const uint8_t appendRows,
                                                   const GolfIndexTransactionOps& storage) {
  GolfIndexTransactionResult result{};
  result.live = live;
  const bool knownVersion = live.version == GolfIndexVersion::V2 || live.version == GolfIndexVersion::V3 ||
                            live.version == GolfIndexVersion::V4;
  if (!transactionOpsValid(storage) || (live.present && !knownVersion) ||
      (!live.present && (live.version != GolfIndexVersion::Unknown || live.rows != 0)) ||
      appendRows > GolfRound::MAX_PLAYERS) {
    result.error = GolfIndexTransactionError::InvalidState;
    return result;
  }

  if (live.version == GolfIndexVersion::V2 || live.version == GolfIndexVersion::V3) {
    const StageTransactionResult migration =
        runStageTransaction(GolfIndexStagePurpose::Migration, live, live.rows, storage);
    result.cleanupPending = migration.cleanupPending;
    if (migration.published) {
      live.version = GolfIndexVersion::V4;
      result.live = live;
    }
    if (migration.error != GolfIndexTransactionError::None) {
      result.error = migration.error;
      return result;
    }
  }

  if (appendRows == 0) return result;
  if ((live.present && live.version != GolfIndexVersion::V4) || UINT32_MAX - live.rows < appendRows) {
    result.error = GolfIndexTransactionError::InvalidState;
    return result;
  }

  const uint32_t expectedRows = live.rows + appendRows;
  const StageTransactionResult append =
      runStageTransaction(GolfIndexStagePurpose::Append, live, expectedRows, storage);
  result.cleanupPending = append.cleanupPending;
  if (append.published) {
    result.appendCommitted = true;
    result.live = {expectedRows, GolfIndexVersion::V4, true};
  }
  result.error = append.error;
  return result;
}

GolfIndexRecoveryStatus golfRecoverIndexArtifacts(const GolfIndexRecoveryOps& storage, const char* livePath,
                                                   const char* stagedPath, const char* backupPath) {
  if (storage.exists == nullptr || storage.validate == nullptr || storage.remove == nullptr ||
      storage.rename == nullptr || livePath == nullptr || stagedPath == nullptr || backupPath == nullptr) {
    return GolfIndexRecoveryStatus::Failed;
  }

  const bool liveExists = storage.exists(livePath, storage.user);
  if (liveExists) {
    if (!storage.validate(livePath, false, storage.user)) return GolfIndexRecoveryStatus::Failed;
    if (storage.exists(stagedPath, storage.user) && !storage.remove(stagedPath, storage.user)) {
      return GolfIndexRecoveryStatus::Failed;
    }
    if (storage.exists(backupPath, storage.user) && !storage.remove(backupPath, storage.user)) {
      return GolfIndexRecoveryStatus::Failed;
    }
    return GolfIndexRecoveryStatus::Ready;
  }

  // A backup is the pre-transaction authority. Restore it before even probing
  // staging so a cut between the two publish renames always rolls back.
  if (storage.exists(backupPath, storage.user)) {
    if (!storage.rename(backupPath, livePath, storage.user)) return GolfIndexRecoveryStatus::Failed;
    if (!storage.validate(livePath, false, storage.user)) return GolfIndexRecoveryStatus::Failed;
    if (storage.exists(stagedPath, storage.user) && !storage.remove(stagedPath, storage.user)) {
      return GolfIndexRecoveryStatus::Failed;
    }
    return GolfIndexRecoveryStatus::Ready;
  }

  // A lone stage has no durable commit authority. A first multi-player append
  // can be interrupted after any complete row and still be syntactically v4,
  // so validation cannot prove that the whole group reached staging.
  if (storage.exists(stagedPath, storage.user)) {
    if (!storage.remove(stagedPath, storage.user)) return GolfIndexRecoveryStatus::Failed;
  }
  return GolfIndexRecoveryStatus::NoIndex;
}

bool golfIndexGroupRowsValid(const uint8_t rowCount, const uint8_t slotMask) {
  const uint8_t validMask = static_cast<uint8_t>((1U << GolfRound::MAX_PLAYERS) - 1);
  if (rowCount == 0 || rowCount > GolfRound::MAX_PLAYERS || (slotMask & ~validMask) != 0) return false;
  uint8_t bits = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((slotMask & (1U << slot)) != 0) ++bits;
  }
  return bits == rowCount;
}

void GolfIndexMigrator::reset() {
  line_[0] = '\0';
  output_[0] = '\0';
  lineNumber_ = 1;
  dataRows_ = 0;
  outputRows_ = 0;
  lineLength_ = 0;
  groupRows_ = 0;
  groupSlotMask_ = 0;
  lineOverflow_ = false;
  aborted_ = false;
  deleting_ = false;
  strictValidation_ = false;
  requireV4_ = false;
  duplicateGroupSlot_ = false;
  groupFilename_ = nullptr;
  sourceVersion_ = GolfIndexVersion::Unknown;
}

void GolfIndexMigrator::resetForStrictValidation(const bool requireV4) {
  reset();
  strictValidation_ = true;
  requireV4_ = requireV4;
}

bool GolfIndexMigrator::resetForDelete(const char* filename) {
  if (!resetForGroupCount(filename)) return false;
  strictValidation_ = false;
  deleting_ = true;
  return true;
}

bool GolfIndexMigrator::resetForGroupCount(const char* filename) {
  resetForStrictValidation(true);
  if (filename == nullptr || filename[0] == '\0') return false;
  groupFilename_ = filename;
  return true;
}

bool GolfIndexMigrator::feed(const char* data, const size_t size, const GolfIndexMigrateSink sink, void* user) {
  if (data == nullptr || aborted_) return !aborted_;
  for (size_t index = 0; index < size; ++index) {
    const char value = data[index];
    if (value == '\n') {
      if (!acceptLine(sink, user)) return false;
      ++lineNumber_;
      lineLength_ = 0;
      lineOverflow_ = false;
      continue;
    }
    if (value == '\r') continue;
    if (static_cast<size_t>(lineLength_) + 1 < sizeof(line_)) {
      line_[lineLength_++] = value;
    } else {
      lineOverflow_ = true;
    }
  }
  return true;
}

bool GolfIndexMigrator::finish() {
  // Legacy migration alone may drop an interrupted tail. A v4 reader or any
  // strict verifier must reject it because the final record was not committed.
  if (lineLength_ > 0 && (deleting_ || strictValidation_ || sourceVersion_ == GolfIndexVersion::V4)) {
    aborted_ = true;
  }
  lineLength_ = 0;
  lineOverflow_ = false;
  return !aborted_;
}

void GolfIndexMigrator::recordGroupRow(const uint8_t playerSlot) {
  if (groupRows_ < UINT8_MAX) ++groupRows_;
  const uint8_t bit = static_cast<uint8_t>(1U << playerSlot);
  if ((groupSlotMask_ & bit) != 0) duplicateGroupSlot_ = true;
  groupSlotMask_ |= bit;
}

bool GolfIndexMigrator::parseSourceRow(GolfIndexRow& row) const {
  if (sourceVersion_ == GolfIndexVersion::V4) {
    return golfParseIndexRow(line_, GolfIndexVersion::V4, row);
  }
  if (sourceVersion_ != GolfIndexVersion::V2 && sourceVersion_ != GolfIndexVersion::V3) return false;

  // Only explicit legacy migration accepts the historical mixed 9/11-column
  // shape. Each attempt still parses against a known schema; v4 is never tried.
  if (golfParseIndexRow(line_, sourceVersion_, row)) return true;
  const GolfIndexVersion alternate =
      sourceVersion_ == GolfIndexVersion::V2 ? GolfIndexVersion::V3 : GolfIndexVersion::V2;
  return !strictValidation_ && golfParseIndexRow(line_, alternate, row);
}

bool GolfIndexMigrator::acceptLine(const GolfIndexMigrateSink sink, void* user) {
  line_[lineLength_] = '\0';

  if (lineNumber_ == 1) {
    sourceVersion_ = lineOverflow_ ? GolfIndexVersion::Unknown : golfIndexHeaderVersion(line_);
    if ((deleting_ || strictValidation_) &&
        (sourceVersion_ == GolfIndexVersion::Unknown ||
         (requireV4_ && sourceVersion_ != GolfIndexVersion::V4))) {
      aborted_ = true;
      return false;
    }
    if (!strictValidation_ && (deleting_ || needsMigration()) && !emit(sink, user, GOLF_INDEX_HEADER)) {
      aborted_ = true;
      return false;
    }
    return true;
  }

  if (sourceVersion_ == GolfIndexVersion::Unknown || lineLength_ == 0) return true;
  GolfIndexRow parsed{};
  if (lineOverflow_ || !parseSourceRow(parsed)) {
    if (deleting_ || strictValidation_ || sourceVersion_ == GolfIndexVersion::V4) {
      aborted_ = true;
      return false;
    }
    return true;
  }

  ++dataRows_;
  const bool inGroup = groupFilename_ != nullptr && strcmp(parsed.file, groupFilename_) == 0;
  if (inGroup) {
    recordGroupRow(parsed.playerSlot);
    if (deleting_) return true;
  }

  if (deleting_ || (!strictValidation_ && needsMigration())) {
    if (sourceVersion_ != GolfIndexVersion::V4) {
      parsed.playerSlot = 0;
      memcpy(parsed.playerName, GOLF_DEFAULT_PLAYER_NAMES[0], sizeof(parsed.playerName));
    }
    if (!golfFormatIndexRow(parsed, output_, sizeof(output_)) || !emit(sink, user, output_)) {
      aborted_ = true;
      return false;
    }
    ++outputRows_;
  }
  return true;
}

#endif
