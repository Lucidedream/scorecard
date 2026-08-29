#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfPaths.h"

inline constexpr char GOLF_INDEX_HEADER[] = "date,course,holes,strokes,par,putts,in100,out100,file\r\n";
inline constexpr size_t GOLF_CSV_ROW_BUFFER_SIZE = 192;

struct GolfIndexRow {
  char date[GOLF_DATE_BUFFER_SIZE];
  char course[40];
  uint8_t holes;
  uint16_t strokes;
  uint16_t par;
  uint16_t putts;
  uint16_t in100;
  uint16_t out100;
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
  const char* file;
};

bool golfFormatIndexRow(const GolfIndexRowView& row, char* output, size_t outputSize);
bool golfFormatIndexRow(const GolfIndexRow& row, char* output, size_t outputSize);
bool golfParseIndexRow(const char* input, GolfIndexRow& row);
