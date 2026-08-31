#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfCsv.h"

// Sink for migrated bytes. Return false to abort the migration (e.g. a write
// error); the migrator stops feeding and reports failure.
using GolfIndexMigrateSink = bool (*)(const char* data, size_t size, void* user);

// Streams an existing index.csv and rewrites a v2 file as v3: the v3 header
// followed by every data row widened with empty hazards/obs cells. Rows that do
// not parse are dropped, exactly as GolfHistoryReader drops them, so a row-count
// check through the same parser still matches.
//
// Fed a file that is already v3, it emits nothing and only counts the data rows
// that parse — this is how the migrated `index.csv.new` is verified before it
// replaces the original.
//
// No heap and no whole-file buffer: one row buffer, fed in chunks.
class GolfIndexMigrator {
 public:
  void reset();
  bool feed(const char* data, size_t size, GolfIndexMigrateSink sink, void* user);
  bool finish();

  GolfIndexVersion sourceVersion() const { return sourceVersion_; }
  // True only when the source carried a v2 header and is worth replacing.
  bool needsMigration() const { return sourceVersion_ == GolfIndexVersion::V2; }
  // Data rows that parsed: rows emitted in v2 mode, rows counted in v3 mode.
  uint32_t dataRows() const { return dataRows_; }
  bool aborted() const { return aborted_; }

 private:
  char line_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  uint32_t lineNumber_ = 1;
  uint32_t dataRows_ = 0;
  uint16_t lineLength_ = 0;
  bool lineOverflow_ = false;
  bool aborted_ = false;
  GolfIndexVersion sourceVersion_ = GolfIndexVersion::Unknown;

  bool acceptLine(GolfIndexMigrateSink sink, void* user);
};
