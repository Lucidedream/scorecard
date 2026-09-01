#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfPaths.h"
#include "GolfRound.h"

inline constexpr char GOLF_INDEX_HEADER_V2[] = "date,course,holes,strokes,par,putts,in100,out100,file";
inline constexpr char GOLF_INDEX_HEADER_V3[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file";
inline constexpr char GOLF_INDEX_HEADER_V4[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,file";
inline constexpr char GOLF_INDEX_HEADER[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,file\r\n";
inline constexpr size_t GOLF_CSV_ROW_BUFFER_SIZE = 255;

enum class GolfIndexVersion : uint8_t { Unknown, V2, V3, V4 };

GolfIndexVersion golfIndexHeaderVersion(const char* line);

struct GolfIndexRow {
  char date[GOLF_DATE_BUFFER_SIZE];
  char course[40];
  char playerName[GolfPlayer::NAME_CAPACITY];
  char file[GOLF_ROUND_FILENAME_BUFFER_SIZE];
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

struct GolfIndexRowView {
  const char* date;
  const char* course;
  uint8_t holes;
  uint8_t playerSlot;
  const char* playerName;
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
// The normal reader accepts only the v4 row shape. Legacy shapes are available
// only to the explicit migrator through the versioned overload.
bool golfParseIndexRow(const char* input, GolfIndexRow& row);
bool golfParseIndexRow(const char* input, GolfIndexVersion version, GolfIndexRow& row);

uint8_t golfEnabledPlayerMask(const GolfRound& round);
bool golfMakeIndexRow(const GolfRound& round, uint8_t playerSlot, const char* filename, GolfIndexRow& row);

using GolfIndexRowSink = bool (*)(const char* data, size_t size, void* user);

struct GolfIndexGroupWriteResult {
  uint8_t rowCount;
  uint8_t slotMask;
  bool complete;
};

GolfIndexGroupWriteResult golfWriteIndexGroupRows(const GolfRound& round, const char* filename,
                                                  GolfIndexRow& rowScratch, char* rowBuffer, size_t rowBufferSize,
                                                  GolfIndexRowSink sink, void* user);

static_assert(sizeof(GolfIndexRow) <= 160);
