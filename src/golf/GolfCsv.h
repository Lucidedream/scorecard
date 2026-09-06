#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfPaths.h"
#include "GolfRound.h"

inline constexpr char GOLF_INDEX_HEADER_V2[] = "date,course,holes,strokes,par,putts,in100,out100,file";
inline constexpr char GOLF_INDEX_HEADER_V3[] = "date,course,holes,strokes,par,putts,in100,out100,hazards,obs,file";
inline constexpr char GOLF_INDEX_HEADER_V4[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,file";
inline constexpr char GOLF_INDEX_HEADER_V5[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,fairwayHoles,gir,"
    "girHoles,file";
inline constexpr char GOLF_INDEX_HEADER[] =
    "date,course,holes,playerSlot,playerName,strokes,par,putts,in100,out100,hazards,obs,fairways,fairwayHoles,gir,"
    "girHoles,file\r\n";
// Widest v5 row: 10 (date) + 80 (40-char all-quote course) + 7 (holes/slot) +
// 48 (24-char all-quote name) + 67 (11 uint16 totals cells) + 63 (file) + 2
// (CRLF) + NUL = 279. Pre-v5 this was 255 (v4's four fewer cells).
inline constexpr size_t GOLF_CSV_ROW_BUFFER_SIZE = 288;

enum class GolfIndexVersion : uint8_t { Unknown, V2, V3, V4, V5 };

GolfIndexVersion golfIndexHeaderVersion(const char* line);

// The four v5 FIR/GIR fields follow the hazards/obs "empty means not recorded"
// rule (CONTRACTS-V2 §31.6). Each pair is written both-empty or both-present:
//   * fairways/fairwayHoles: present only when fairwaysRecorded (the source round
//     was v5+ and actually carries the fairway array). A v4 round decodes with
//     every fairwayHit bit clear, indistinguishable from "hit 0 fairways", so it
//     is written blank.
//   * gir/girHoles: present whenever girRecorded (the round has usable par).
//     GIR needs no stored input, so it is available for any round, v2 included.
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
  uint16_t fairways;
  uint16_t fairwayHoles;
  uint16_t gir;
  uint16_t girHoles;
  uint8_t holes;
  uint8_t playerSlot;
  bool penaltiesRecorded;
  bool fairwaysRecorded;
  bool girRecorded;
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
  uint16_t fairways;
  uint16_t fairwayHoles;
  uint16_t gir;
  uint16_t girHoles;
  bool penaltiesRecorded;
  bool fairwaysRecorded;
  bool girRecorded;
  const char* file;
};

bool golfFormatIndexRow(const GolfIndexRowView& row, char* output, size_t outputSize);
bool golfFormatIndexRow(const GolfIndexRow& row, char* output, size_t outputSize);
// The normal reader accepts only the v4 row shape. Legacy shapes are available
// only to the explicit migrator through the versioned overload.
bool golfParseIndexRow(const char* input, GolfIndexRow& row);
bool golfParseIndexRow(const char* input, GolfIndexVersion version, GolfIndexRow& row);

uint8_t golfEnabledPlayerMask(const GolfRound& round);
// fairwaysRecorded is threaded from the caller: true for a live round (always
// v5), or GolfRoundFileInfo::fairwaysRecorded on a rebuild. When false, the
// fairways/fairwayHoles cells are left blank. gir/girHoles are filled whenever
// the round has usable par, regardless of fairwaysRecorded.
bool golfMakeIndexRow(const GolfRound& round, uint8_t playerSlot, const char* filename, bool fairwaysRecorded,
                      GolfIndexRow& row);

using GolfIndexRowSink = bool (*)(const char* data, size_t size, void* user);

struct GolfIndexGroupWriteResult {
  uint8_t rowCount;
  uint8_t slotMask;
  bool complete;
};

GolfIndexGroupWriteResult golfWriteIndexGroupRows(const GolfRound& round, const char* filename, bool fairwaysRecorded,
                                                  GolfIndexRow& rowScratch, char* rowBuffer, size_t rowBufferSize,
                                                  GolfIndexRowSink sink, void* user);

// The v5 FIR/GIR fields add four uint16_t plus two bools; the pre-v5 ceiling was
// 160 (actual 158). CONTRACTS-V2 §31.6 mandates uint16_t for the four counts.
static_assert(sizeof(GolfIndexRow) <= 168);
