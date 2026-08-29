#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfCsv.h"

inline constexpr uint8_t GOLF_HISTORY_CAPACITY = 50;

struct GolfHistoryEntry {
  char course[40];
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint8_t holes;
};

using GolfHistoryMalformedCallback = void (*)(uint32_t lineNumber, void* user);

class GolfHistoryReader {
 public:
  void reset();
  void feed(const char* data, size_t size, GolfHistoryMalformedCallback malformedCallback = nullptr,
            void* callbackUser = nullptr);
  void finish(GolfHistoryMalformedCallback malformedCallback = nullptr, void* callbackUser = nullptr);

  uint8_t count() const { return entryCount; }
  bool overflowed() const { return validRows > GOLF_HISTORY_CAPACITY; }
  uint32_t totalValidRows() const { return validRows; }
  const GolfHistoryEntry& newest(uint8_t index) const;

 private:
  GolfHistoryEntry entries[GOLF_HISTORY_CAPACITY]{};
  char line[GOLF_CSV_ROW_BUFFER_SIZE]{};
  uint32_t lineNumber = 1;
  uint32_t validRows = 0;
  uint16_t lineLength = 0;
  uint8_t entryCount = 0;
  uint8_t nextEntry = 0;
  bool lineOverflow = false;

  void acceptLine(GolfHistoryMalformedCallback malformedCallback, void* callbackUser);
};

bool golfHistoryShowsToPar(const GolfHistoryEntry& entry);

// Recovers the `file` column of a single index.csv row on a second streaming pass,
// so History can open one round file without holding a filename per cached entry
// (GolfHistoryEntry stays within its 56-byte budget). The target is given in
// newest-first terms to match the list; `totalValidRows` comes from the first pass.
class GolfIndexFileLocator {
 public:
  // newestIndex 0 is the most recent valid row.
  void reset(uint8_t newestIndex, uint32_t totalValidRows);
  void feed(const char* data, size_t size);
  bool finish();  // returns found()

  bool found() const { return found_; }
  const char* filename() const { return filename_; }

 private:
  char line_[GOLF_CSV_ROW_BUFFER_SIZE]{};
  char filename_[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  uint32_t targetRow_ = 0;  // file-order index of the wanted row
  uint32_t validRow_ = 0;
  uint32_t lineNumber_ = 1;
  uint16_t lineLength_ = 0;
  bool lineOverflow_ = false;
  bool found_ = false;
  bool active_ = false;

  void acceptLine();
};
