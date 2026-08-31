#include "GolfHistory.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool isIndexHeader(const char* line) { return golfIndexHeaderVersion(line) != GolfIndexVersion::Unknown; }

}  // namespace

void GolfHistoryReader::reset() {
  lineNumber = 1;
  validRows = 0;
  lineLength = 0;
  entryCount = 0;
  nextEntry = 0;
  lineOverflow = false;
}

void GolfHistoryReader::feed(const char* data, const size_t size, const GolfHistoryMalformedCallback malformedCallback,
                             void* callbackUser) {
  if (data == nullptr) return;
  for (size_t index = 0; index < size; ++index) {
    const char value = data[index];
    if (value == '\n') {
      acceptLine(malformedCallback, callbackUser);
      ++lineNumber;
      lineLength = 0;
      lineOverflow = false;
      continue;
    }
    if (value == '\r') continue;
    if (lineLength + 1 < sizeof(line)) {
      line[lineLength++] = value;
    } else {
      lineOverflow = true;
    }
  }
}

void GolfHistoryReader::finish(const GolfHistoryMalformedCallback malformedCallback, void* callbackUser) {
  // index.csv rows are committed with CRLF. An unterminated tail may be a
  // power-loss write, so it is never allowed to replace a complete row.
  if (lineLength > 0 && malformedCallback != nullptr) malformedCallback(lineNumber, callbackUser);
  lineLength = 0;
  lineOverflow = false;
}

void GolfHistoryReader::acceptLine(const GolfHistoryMalformedCallback malformedCallback, void* callbackUser) {
  line[lineLength] = '\0';
  if (lineLength == 0 || (lineNumber == 1 && isIndexHeader(line))) return;

  GolfIndexRow parsed{};
  if (lineOverflow || !golfParseIndexRow(line, parsed)) {
    if (malformedCallback != nullptr) malformedCallback(lineNumber, callbackUser);
    return;
  }

  GolfHistoryEntry& entry = entries[nextEntry];
  memcpy(entry.course, parsed.course, sizeof(entry.course));
  entry.dateYmd = 0;
  golfParseDate(parsed.date, entry.dateYmd);
  entry.strokes = parsed.strokes;
  entry.par = parsed.par;
  entry.putts = parsed.putts;
  entry.in100 = parsed.in100;
  entry.out100 = parsed.out100;
  entry.hazards = parsed.hazards;
  entry.obs = parsed.obs;
  entry.holes = parsed.holes;
  entry.penaltiesRecorded = parsed.penaltiesRecorded;
  nextEntry = static_cast<uint8_t>((nextEntry + 1) % GOLF_HISTORY_CAPACITY);
  if (entryCount < GOLF_HISTORY_CAPACITY) ++entryCount;
  ++validRows;
}

const GolfHistoryEntry& GolfHistoryReader::newest(const uint8_t index) const {
  static constexpr GolfHistoryEntry EMPTY{};
  if (index >= entryCount) return EMPTY;
  const uint8_t newestIndex =
      static_cast<uint8_t>((nextEntry + GOLF_HISTORY_CAPACITY - 1 - index) % GOLF_HISTORY_CAPACITY);
  return entries[newestIndex];
}

bool golfHistoryShowsToPar(const GolfHistoryEntry& entry) { return entry.par != 0; }

void GolfIndexFileLocator::reset(const uint8_t newestIndex, const uint32_t totalValidRows) {
  line_[0] = '\0';
  filename_[0] = '\0';
  validRow_ = 0;
  lineNumber_ = 1;
  lineLength_ = 0;
  lineOverflow_ = false;
  found_ = false;
  active_ = totalValidRows > 0 && newestIndex < totalValidRows;
  targetRow_ = active_ ? totalValidRows - 1 - newestIndex : 0;
}

void GolfIndexFileLocator::feed(const char* data, const size_t size) {
  if (data == nullptr || !active_ || found_) return;
  for (size_t index = 0; index < size; ++index) {
    const char value = data[index];
    if (value == '\n') {
      acceptLine();
      ++lineNumber_;
      lineLength_ = 0;
      lineOverflow_ = false;
      if (found_) return;
      continue;
    }
    if (value == '\r') continue;
    if (lineLength_ + 1 < sizeof(line_)) {
      line_[lineLength_++] = value;
    } else {
      lineOverflow_ = true;
    }
  }
}

bool GolfIndexFileLocator::finish() {
  if (active_ && !found_ && lineLength_ > 0) acceptLine();
  lineLength_ = 0;
  lineOverflow_ = false;
  return found_;
}

void GolfIndexFileLocator::acceptLine() {
  line_[lineLength_] = '\0';
  if (lineLength_ == 0 || (lineNumber_ == 1 && isIndexHeader(line_))) return;

  GolfIndexRow parsed{};
  if (lineOverflow_ || !golfParseIndexRow(line_, parsed)) return;

  if (validRow_ == targetRow_) {
    memcpy(filename_, parsed.file, sizeof(filename_));
    found_ = true;
  }
  ++validRow_;
}

#endif
