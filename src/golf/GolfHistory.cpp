#include "GolfHistory.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool isIndexHeader(const char* line) { return golfIndexHeaderVersion(line) != GolfIndexVersion::Unknown; }

}  // namespace

bool GolfHistoryReader::reset(const uint8_t playerSlot) {
  lineNumber = 1;
  validRows = 0;
  lineLength = 0;
  entryCount = 0;
  nextEntry = 0;
  lineOverflow = false;
  playerSlot_ = playerSlot < GolfRound::MAX_PLAYERS ? playerSlot : GolfRound::NO_PLAYER;
  return playerSlot_ != GolfRound::NO_PLAYER;
}

void GolfHistoryReader::feed(const char* data, const size_t size, const GolfHistoryMalformedCallback malformedCallback,
                             void* callbackUser) {
  if (data == nullptr || playerSlot_ == GolfRound::NO_PLAYER) return;
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
    if (static_cast<size_t>(lineLength) + 1 < sizeof(line)) {
      line[lineLength++] = value;
    } else {
      lineOverflow = true;
    }
  }
}

void GolfHistoryReader::finish(const GolfHistoryMalformedCallback malformedCallback, void* callbackUser) {
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
  // Filtering happens before both the 50-entry ring and valid-row ordinal.
  if (parsed.playerSlot != playerSlot_) return;

  GolfHistoryEntry& entry = entries[nextEntry];
  memcpy(entry.course, parsed.course, sizeof(entry.course));
  memcpy(entry.playerName, parsed.playerName, sizeof(entry.playerName));
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
  entry.playerSlot = parsed.playerSlot;
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

bool GolfIndexFileLocator::reset(const uint8_t playerSlot, const uint8_t newestIndex,
                                 const uint32_t totalFilteredRows) {
  line_[0] = '\0';
  filename_[0] = '\0';
  validRow_ = 0;
  lineNumber_ = 1;
  lineLength_ = 0;
  lineOverflow_ = false;
  found_ = false;
  playerSlot_ = playerSlot < GolfRound::MAX_PLAYERS ? playerSlot : GolfRound::NO_PLAYER;
  active_ = playerSlot_ != GolfRound::NO_PLAYER && totalFilteredRows > 0 && newestIndex < totalFilteredRows;
  targetRow_ = active_ ? totalFilteredRows - 1 - newestIndex : 0;
  return active_;
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
    if (static_cast<size_t>(lineLength_) + 1 < sizeof(line_)) {
      line_[lineLength_++] = value;
    } else {
      lineOverflow_ = true;
    }
  }
}

bool GolfIndexFileLocator::finish() {
  // Match GolfHistoryReader: an unterminated tail is not a committed row.
  lineLength_ = 0;
  lineOverflow_ = false;
  return found_;
}

void GolfIndexFileLocator::acceptLine() {
  line_[lineLength_] = '\0';
  if (lineLength_ == 0 || (lineNumber_ == 1 && isIndexHeader(line_))) return;

  GolfIndexRow parsed{};
  if (lineOverflow_ || !golfParseIndexRow(line_, parsed) || parsed.playerSlot != playerSlot_) return;

  if (validRow_ == targetRow_) {
    memcpy(filename_, parsed.file, sizeof(filename_));
    found_ = true;
  }
  ++validRow_;
}

void GolfPlayerNamesReader::reset() {
  memcpy(names_, GOLF_DEFAULT_PLAYER_NAMES, sizeof(names_));
  memset(present_, 0, sizeof(present_));
  line_[0] = '\0';
  lineLength_ = 0;
  lineNumber_ = 1;
  lineOverflow_ = false;
}

void GolfPlayerNamesReader::feed(const char* data, const size_t size) {
  if (data == nullptr) return;
  for (size_t index = 0; index < size; ++index) {
    const char value = data[index];
    if (value == '\n') {
      acceptLine();
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
}

void GolfPlayerNamesReader::finish() {
  lineLength_ = 0;
  lineOverflow_ = false;
}

const char* GolfPlayerNamesReader::name(const uint8_t playerSlot) const {
  return playerSlot < GolfRound::MAX_PLAYERS ? names_[playerSlot] : "";
}

bool GolfPlayerNamesReader::present(const uint8_t playerSlot) const {
  return playerSlot < GolfRound::MAX_PLAYERS && present_[playerSlot];
}

uint8_t GolfPlayerNamesReader::firstPresent() const {
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (present_[slot]) return slot;
  }
  return GolfRound::NO_PLAYER;
}

void GolfPlayerNamesReader::acceptLine() {
  line_[lineLength_] = '\0';
  if (lineLength_ == 0 || (lineNumber_ == 1 && isIndexHeader(line_))) return;
  GolfIndexRow parsed{};
  if (lineOverflow_ || !golfParseIndexRow(line_, parsed) || parsed.playerSlot >= GolfRound::MAX_PLAYERS) return;
  memcpy(names_[parsed.playerSlot], parsed.playerName, sizeof(names_[parsed.playerSlot]));
  present_[parsed.playerSlot] = true;
}

#endif
