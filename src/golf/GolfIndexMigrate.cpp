#include "GolfIndexMigrate.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool emit(GolfIndexMigrateSink sink, void* user, const char* text) {
  return sink == nullptr || sink(text, strlen(text), user);
}

}  // namespace

void GolfIndexMigrator::reset() {
  line_[0] = '\0';
  lineNumber_ = 1;
  dataRows_ = 0;
  lineLength_ = 0;
  lineOverflow_ = false;
  aborted_ = false;
  sourceVersion_ = GolfIndexVersion::Unknown;
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
    if (lineLength_ + 1 < sizeof(line_)) {
      line_[lineLength_++] = value;
    } else {
      lineOverflow_ = true;
    }
  }
  return true;
}

bool GolfIndexMigrator::finish() {
  // A trailing line with no newline is an interrupted write. GolfHistoryReader
  // drops it; drop it here too so the row counts still agree.
  lineLength_ = 0;
  lineOverflow_ = false;
  return !aborted_;
}

bool GolfIndexMigrator::acceptLine(const GolfIndexMigrateSink sink, void* user) {
  line_[lineLength_] = '\0';

  if (lineNumber_ == 1) {
    sourceVersion_ = lineOverflow_ ? GolfIndexVersion::Unknown : golfIndexHeaderVersion(line_);
    if (sourceVersion_ == GolfIndexVersion::V2 && !emit(sink, user, GOLF_INDEX_HEADER)) {
      aborted_ = true;
      return false;
    }
    return true;
  }

  // Only a v2 source is rewritten; a v3 source is merely counted for verification.
  if (sourceVersion_ != GolfIndexVersion::V2 && sourceVersion_ != GolfIndexVersion::V3) return true;
  if (lineLength_ == 0) return true;

  GolfIndexRow parsed{};
  if (lineOverflow_ || !golfParseIndexRow(line_, parsed)) return true;

  if (sourceVersion_ == GolfIndexVersion::V2) {
    parsed.penaltiesRecorded = false;  // widen with empty hazards/obs cells
    char row[GOLF_CSV_ROW_BUFFER_SIZE];
    if (!golfFormatIndexRow(parsed, row, sizeof(row)) || !emit(sink, user, row)) {
      aborted_ = true;
      return false;
    }
  }
  ++dataRows_;
  return true;
}

#endif
