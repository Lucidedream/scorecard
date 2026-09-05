#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfHistory.h"
#include "GolfRound.h"

enum class GolfExportFormat : uint8_t { Text, Csv, Json, Html };
enum class GolfExportLabel : uint8_t {
  Title,
  Course,
  Player,
  Slot,
  Date,
  Tee,
  Status,
  Archived,
  InProgress,
  Detail,
  SummaryOnly,
  Unavailable,
  Yes,
  No,
  Recovered,
  Score,
  Par,
  ToPar,
  Putts,
  In100,
  Out100,
  Short,
  Penalty,
  Hazards,
  Obs,
  Thru,
  Holes,
  OnePutts,
  ThreePutts,
  Front,
  Back,
  Hole,
  Entered,
  Yards,
  Si,
  Events,
  Dictionary,
  DownloadAgent,
  DownloadCsv,
  DownloadJson,
  DownloadHtml,
  DownloadHelp,
  SendHelp,
  Worst,
  Blue,
  White,
  Count
};
using GolfExportTranslate = const char* (*)(GolfExportLabel);

struct GolfExportData {
  GolfRound round{};
  GolfHistoryEntry summary{};
  uint8_t playerSlot = 0;
  bool detailed = true;
  bool archived = true;
  bool penaltiesRecorded = true;
  bool repaired = false;
};

// Each next() emits one bounded document block. Storage is owned by the caller;
// the immutable data and translation function must outlive the cursor.
class GolfRoundExport {
 public:
  static constexpr size_t BLOCK_CAPACITY = 2048;
  bool begin(const GolfExportData& data, GolfExportFormat format, GolfExportTranslate translate);
  bool next(char* output, size_t capacity, size_t& written);
  bool done() const { return finished; }
  static const char* extension(GolfExportFormat format);
  static const char* mimeType(GolfExportFormat format);

 private:
  const GolfExportData* data = nullptr;
  GolfExportTranslate translate = nullptr;
  GolfExportFormat format = GolfExportFormat::Text;
  uint16_t block = 0;
  bool finished = true;
};

const char* golfExportTranslate(GolfExportLabel label);
