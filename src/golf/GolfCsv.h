#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfPaths.h"

inline constexpr char GOLF_INDEX_HEADER_V2[] = "date,course,holes,strokes,par,putts,in100,out100,file";
inline constexpr char GOLF_INDEX_HEADER_V3[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file";
inline constexpr char GOLF_INDEX_HEADER[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file\r\n";
inline constexpr size_t GOLF_CSV_ROW_BUFFER_SIZE = 192;

enum class GolfIndexVersion : uint8_t { Unknown, V2, V3 };

// Classifies a header line stripped of its trailing CRLF. Returns Unknown when
// the line is not a recognised index.csv header.
GolfIndexVersion golfIndexHeaderVersion(const char* line);

struct GolfIndexRow {
  char date[GOLF_DATE_BUFFER_SIZE];
  char course[40];
  uint8_t holes;
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t hazards;
  uint16_t obs;
  // False for a v2 row and for a v3 row whose hazards/obs fields are empty
  // (a round migrated from before penalty tracking). Empty means "not recorded",
  // never zero — trends must exclude such rounds from penalty averages.
  bool penaltiesRecorded;
  char file[GOLF_ROUND_FILENAME_BUFFER_SIZE];
};

struct GolfIndexRowView {
  const char* date;
  const char* course;
  uint8_t holes;
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
  uint16_t hazards;
  uint16_t obs;
  bool penaltiesRecorded;
  const char* file;
};

bool golfFormatIndexRow(const GolfIndexRowView& row, char* output, size_t outputSize);
bool golfFormatIndexRow(const GolfIndexRow& row, char* output, size_t outputSize);
bool golfParseIndexRow(const char* input, GolfIndexRow& row);
