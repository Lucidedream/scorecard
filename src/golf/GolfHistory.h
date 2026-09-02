#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfCsv.h"

inline constexpr uint8_t GOLF_HISTORY_CAPACITY = 50;

struct GolfHistoryEntry {
  char course[40];
  char playerName[GolfPlayer::NAME_CAPACITY];
  uint16_t dateYmd;
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t hazards;
  uint16_t obs;
  uint8_t holes;
  uint8_t playerSlot;
  bool penaltiesRecorded;
};

static_assert(sizeof(GolfHistoryEntry) <= 96);

using GolfHistoryMalformedCallback = void (*)(uint32_t lineNumber, void* user);

class GolfHistoryReader {
 public:
  bool reset(uint8_t playerSlot);
  void feed(const char* data, size_t size, GolfHistoryMalformedCallback malformedCallback = nullptr,
            void* callbackUser = nullptr);
  void finish(GolfHistoryMalformedCallback malformedCallback = nullptr, void* callbackUser = nullptr);

  uint8_t count() const { return entryCount; }
  bool overflowed() const { return validRows > GOLF_HISTORY_CAPACITY; }
  uint32_t totalValidRows() const { return validRows; }
  uint8_t playerSlot() const { return playerSlot_; }
  const GolfHistoryEntry& newest(uint8_t index) const;

 private:
  GolfHistoryEntry entries[GOLF_HISTORY_CAPACITY]{};
  char line[GOLF_CSV_ROW_BUFFER_SIZE]{};
  uint32_t lineNumber = 1;
  uint32_t validRows = 0;
  uint16_t lineLength = 0;
  uint8_t entryCount = 0;
  uint8_t nextEntry = 0;
  uint8_t playerSlot_ = GolfRound::NO_PLAYER;
  bool lineOverflow = false;

  void acceptLine(GolfHistoryMalformedCallback malformedCallback, void* callbackUser);
};

bool golfHistoryShowsToPar(const GolfHistoryEntry& entry);

class GolfIndexFileLocator {
 public:
  bool reset(uint8_t playerSlot, uint8_t newestIndex, uint32_t totalFilteredRows);
  void feed(const char* data, size_t size);
  bool finish();

  bool found() const { return found_; }
  const char* filename() const { return filename_; }

 private:
  char line_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  char filename_[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  uint32_t targetRow_ = 0;
  uint32_t validRow_ = 0;
  uint32_t lineNumber_ = 1;
  uint16_t lineLength_ = 0;
  uint8_t playerSlot_ = GolfRound::NO_PLAYER;
  bool lineOverflow_ = false;
  bool found_ = false;
  bool active_ = false;

  void acceptLine();
};

// Streams all valid rows chronologically and retains only the latest name for
// each stable slot. Defaults remain available for slots not yet present.
class GolfPlayerNamesReader {
 public:
  void reset();
  void feed(const char* data, size_t size);
  void finish();

  const char* name(uint8_t playerSlot) const;
  bool present(uint8_t playerSlot) const;
  uint32_t roundCount(uint8_t playerSlot) const;
  uint32_t totalRounds() const { return totalRounds_; }
  uint8_t playerCount() const;
  const GolfIndexRow& latestRound() const { return latestRound_; }
  bool hasLatestRound() const { return hasLatestRound_; }
  uint8_t firstPresent() const;

 private:
  char names_[GolfRound::MAX_PLAYERS][GolfPlayer::NAME_CAPACITY]{};
  char line_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  uint16_t lineLength_ = 0;
  uint32_t lineNumber_ = 1;
  bool present_[GolfRound::MAX_PLAYERS]{};
  uint32_t roundCounts_[GolfRound::MAX_PLAYERS]{};
  GolfIndexRow latestRound_{};
  char latestFile_[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  uint32_t totalRounds_ = 0;
  bool lineOverflow_ = false;
  bool hasLatestRound_ = false;

  void acceptLine();
};
