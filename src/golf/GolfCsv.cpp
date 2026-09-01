#include "GolfCsv.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>
#include <cstring>
#include <limits>

#include "GolfPenalty.h"
#include "GolfStats.h"

namespace {

bool appendChar(const char value, char* output, const size_t outputSize, size_t& written) {
  if (written + 1 >= outputSize) return false;
  output[written++] = value;
  return true;
}

bool appendText(const char* value, char* output, const size_t outputSize, size_t& written) {
  if (value == nullptr) return false;
  while (*value != '\0') {
    if (!appendChar(*value++, output, outputSize, written)) return false;
  }
  return true;
}

bool appendCsvField(const char* value, char* output, const size_t outputSize, size_t& written) {
  if (value == nullptr) return false;
  const bool quote = strpbrk(value, ",\"\r\n") != nullptr;
  if (quote && !appendChar('"', output, outputSize, written)) return false;
  for (const char* current = value; *current != '\0'; ++current) {
    if (*current == '"' && !appendChar('"', output, outputSize, written)) return false;
    if (!appendChar(*current, output, outputSize, written)) return false;
  }
  return !quote || appendChar('"', output, outputSize, written);
}

bool parseField(const char*& current, char* output, const size_t outputSize) {
  size_t written = 0;
  const bool quoted = *current == '"';
  bool closed = !quoted;
  if (quoted) ++current;
  while (*current != '\0') {
    if (quoted && *current == '"') {
      if (current[1] == '"') {
        ++current;
      } else {
        ++current;
        closed = true;
        if (*current != ',' && *current != '\r' && *current != '\n' && *current != '\0') return false;
        break;
      }
    } else if (!quoted && (*current == ',' || *current == '\r' || *current == '\n')) {
      break;
    }
    if (written + 1 >= outputSize) return false;
    output[written++] = *current++;
  }
  if (!closed) return false;
  output[written] = '\0';
  return true;
}

bool countFields(const char* input, uint8_t& fields) {
  fields = 1;
  bool quoted = false;
  bool atFieldStart = true;
  for (const char* current = input; *current != '\0' && *current != '\r' && *current != '\n'; ++current) {
    if (*current == '"') {
      if (quoted && current[1] == '"') {
        ++current;
      } else if (quoted) {
        quoted = false;
      } else if (atFieldStart) {
        quoted = true;
      } else {
        return false;
      }
    } else if (*current == ',' && !quoted) {
      if (fields == UINT8_MAX) return false;
      ++fields;
      atFieldStart = true;
      continue;
    }
    atFieldStart = false;
  }
  return !quoted;
}

bool parseUnsigned(const char* value, const uint16_t maximum, uint16_t& parsed) {
  if (*value == '\0') return false;
  uint32_t result = 0;
  while (*value != '\0') {
    if (*value < '0' || *value > '9') return false;
    result = result * 10 + static_cast<uint8_t>(*value++ - '0');
    if (result > maximum) return false;
  }
  parsed = static_cast<uint16_t>(result);
  return true;
}

bool parseNumberField(const char*& current, const uint16_t maximum, uint16_t& output, const bool commaAfter) {
  char numeric[6]{};
  if (!parseField(current, numeric, sizeof(numeric)) || !parseUnsigned(numeric, maximum, output)) return false;
  if (commaAfter) return *current++ == ',';
  return true;
}

bool validUtf8(const char* value) {
  const auto* current = reinterpret_cast<const uint8_t*>(value);
  while (*current != 0) {
    if (*current < 0x80) {
      ++current;
      continue;
    }
    if (*current >= 0xc2 && *current <= 0xdf) {
      if ((current[1] & 0xc0) != 0x80) return false;
      current += 2;
      continue;
    }
    if (*current >= 0xe0 && *current <= 0xef) {
      if ((current[1] & 0xc0) != 0x80 || (current[2] & 0xc0) != 0x80 ||
          (*current == 0xe0 && current[1] < 0xa0) || (*current == 0xed && current[1] >= 0xa0)) {
        return false;
      }
      current += 3;
      continue;
    }
    if (*current >= 0xf0 && *current <= 0xf4) {
      if ((current[1] & 0xc0) != 0x80 || (current[2] & 0xc0) != 0x80 || (current[3] & 0xc0) != 0x80 ||
          (*current == 0xf0 && current[1] < 0x90) || (*current == 0xf4 && current[1] >= 0x90)) {
        return false;
      }
      current += 4;
      continue;
    }
    return false;
  }
  return true;
}

bool singleLineUtf8(const char* value, const bool requireNonempty) {
  return value != nullptr && (!requireNonempty || value[0] != '\0') && strpbrk(value, "\r\n") == nullptr &&
         validUtf8(value);
}

void copyDefaultPlayerName(char* output, const size_t capacity) {
  snprintf(output, capacity, "%s", GOLF_DEFAULT_PLAYER_NAMES[0]);
}

}  // namespace

GolfIndexVersion golfIndexHeaderVersion(const char* line) {
  if (line == nullptr) return GolfIndexVersion::Unknown;
  if (strcmp(line, GOLF_INDEX_HEADER_V4) == 0) return GolfIndexVersion::V4;
  if (strcmp(line, GOLF_INDEX_HEADER_V3) == 0) return GolfIndexVersion::V3;
  if (strcmp(line, GOLF_INDEX_HEADER_V2) == 0) return GolfIndexVersion::V2;
  return GolfIndexVersion::Unknown;
}

bool golfFormatIndexRow(const GolfIndexRowView& row, char* output, const size_t outputSize) {
  if (output == nullptr || outputSize == 0 || row.playerSlot >= GolfRound::MAX_PLAYERS ||
      !singleLineUtf8(row.course, true) || !singleLineUtf8(row.playerName, true) || row.date == nullptr ||
      strpbrk(row.date, ",\r\n") != nullptr || row.file == nullptr || row.file[0] == '\0' ||
      strpbrk(row.file, ",\r\n") != nullptr) {
    return false;
  }

  size_t written = 0;
  char holesAndSlot[12];
  char totals[72];
  const int holesAndSlotLength = snprintf(holesAndSlot, sizeof(holesAndSlot), ",%u,%u,", row.holes, row.playerSlot);
  const int totalsLength = row.penaltiesRecorded
                               ? snprintf(totals, sizeof(totals), ",%u,%u,%u,%u,%u,%u,%u,", row.strokes, row.par,
                                          row.putts, row.in100, row.out100, row.hazards, row.obs)
                               : snprintf(totals, sizeof(totals), ",%u,%u,%u,%u,%u,,,", row.strokes, row.par,
                                          row.putts, row.in100, row.out100);
  if (holesAndSlotLength < 0 || static_cast<size_t>(holesAndSlotLength) >= sizeof(holesAndSlot) || totalsLength < 0 ||
      static_cast<size_t>(totalsLength) >= sizeof(totals) ||
      !appendText(row.date, output, outputSize, written) || !appendChar(',', output, outputSize, written) ||
      !appendCsvField(row.course, output, outputSize, written) ||
      !appendText(holesAndSlot, output, outputSize, written) ||
      !appendCsvField(row.playerName, output, outputSize, written) ||
      !appendText(totals, output, outputSize, written) ||
      !appendText(row.file, output, outputSize, written) || !appendText("\r\n", output, outputSize, written)) {
    output[0] = '\0';
    return false;
  }
  output[written] = '\0';
  return true;
}

bool golfFormatIndexRow(const GolfIndexRow& row, char* output, const size_t outputSize) {
  return golfFormatIndexRow({row.date,       row.course, row.holes,  row.playerSlot, row.playerName,
                             row.strokes,    row.par,    row.putts,  row.in100,      row.out100,
                             row.hazards,    row.obs,    row.penaltiesRecorded,      row.file},
                            output, outputSize);
}

bool golfParseIndexRow(const char* input, const GolfIndexVersion version, GolfIndexRow& row) {
  if (input == nullptr || version == GolfIndexVersion::Unknown) return false;
  const uint8_t expectedFields = version == GolfIndexVersion::V2 ? 9 : version == GolfIndexVersion::V3 ? 11 : 13;
  uint8_t fieldCount = 0;
  if (!countFields(input, fieldCount) || fieldCount != expectedFields) return false;

  GolfIndexRow parsed{};
  const char* current = input;
  if (!parseField(current, parsed.date, sizeof(parsed.date)) || *current++ != ',' ||
      !parseField(current, parsed.course, sizeof(parsed.course)) || *current++ != ',' ||
      !singleLineUtf8(parsed.course, true)) {
    return false;
  }

  uint16_t value = 0;
  if (!parseNumberField(current, std::numeric_limits<uint8_t>::max(), value, true)) return false;
  parsed.holes = static_cast<uint8_t>(value);
  if (version == GolfIndexVersion::V4) {
    if (!parseNumberField(current, GolfRound::MAX_PLAYERS - 1, value, true)) return false;
    parsed.playerSlot = static_cast<uint8_t>(value);
    if (!parseField(current, parsed.playerName, sizeof(parsed.playerName)) || *current++ != ',' ||
        !singleLineUtf8(parsed.playerName, true)) {
      return false;
    }
  } else {
    parsed.playerSlot = 0;
    copyDefaultPlayerName(parsed.playerName, sizeof(parsed.playerName));
  }

  uint16_t* totals[] = {&parsed.strokes, &parsed.par, &parsed.putts, &parsed.in100, &parsed.out100};
  for (uint8_t field = 0; field < 5; ++field) {
    if (!parseNumberField(current, std::numeric_limits<uint16_t>::max(), *totals[field], true)) return false;
  }

  if (version == GolfIndexVersion::V3 || version == GolfIndexVersion::V4) {
    char hazards[6]{};
    char obs[6]{};
    if (!parseField(current, hazards, sizeof(hazards)) || *current++ != ',' ||
        !parseField(current, obs, sizeof(obs)) || *current++ != ',' ||
        !parseField(current, parsed.file, sizeof(parsed.file))) {
      return false;
    }
    const bool hazardsEmpty = hazards[0] == '\0';
    const bool obsEmpty = obs[0] == '\0';
    if (hazardsEmpty != obsEmpty) return false;
    if (!hazardsEmpty) {
      if (!parseUnsigned(hazards, std::numeric_limits<uint16_t>::max(), parsed.hazards) ||
          !parseUnsigned(obs, std::numeric_limits<uint16_t>::max(), parsed.obs)) {
        return false;
      }
      parsed.penaltiesRecorded = true;
    }
  } else if (!parseField(current, parsed.file, sizeof(parsed.file))) {
    return false;
  }

  if (*current == '\r') ++current;
  if (*current == '\n') ++current;
  if (*current != '\0' || parsed.file[0] == '\0' || strpbrk(parsed.file, ",\r\n") != nullptr) return false;
  row = parsed;
  return true;
}

bool golfParseIndexRow(const char* input, GolfIndexRow& row) {
  return golfParseIndexRow(input, GolfIndexVersion::V4, row);
}

uint8_t golfEnabledPlayerMask(const GolfRound& round) {
  uint8_t mask = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (golfPlayerIsEnabled(round.players[slot])) mask |= static_cast<uint8_t>(1U << slot);
  }
  return mask;
}

bool golfMakeIndexRow(const GolfRound& round, const uint8_t playerSlot, const char* filename, GolfIndexRow& row) {
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot]) || filename == nullptr ||
      filename[0] == '\0' || strlen(filename) >= GOLF_ROUND_FILENAME_BUFFER_SIZE ||
      strpbrk(filename, ",\r\n") != nullptr || memchr(round.courseName, '\0', sizeof(round.courseName)) == nullptr ||
      memchr(round.players[playerSlot].name, '\0', sizeof(round.players[playerSlot].name)) == nullptr ||
      !singleLineUtf8(round.courseName, true) || !singleLineUtf8(round.players[playerSlot].name, true)) {
    return false;
  }
  const GolfPlayer& player = round.players[playerSlot];
  const GolfPlayerScore& score = player.score;
  GolfIndexRow built{};
  golfFormatDate(round.dateYmd, built.date, sizeof(built.date));
  memcpy(built.course, round.courseName, sizeof(built.course));
  memcpy(built.playerName, player.name, sizeof(built.playerName));
  snprintf(built.file, sizeof(built.file), "%s", filename);
  built.holes = round.holeCount;
  built.playerSlot = playerSlot;
  built.strokes = golfScore(round, score);
  built.par = golfParTotal(round, score);
  built.putts = golfPuttsTotal(round, score);
  built.in100 = golfIn100Total(round, score);
  built.out100 = golfLongTotal(round, score);
  built.hazards = golfHazardsForRound(score, round.holeCount);
  built.obs = golfObsForRound(score, round.holeCount);
  built.penaltiesRecorded = true;
  row = built;
  return true;
}

GolfIndexGroupWriteResult golfWriteIndexGroupRows(const GolfRound& round, const char* filename,
                                                  GolfIndexRow& rowScratch, char* rowBuffer,
                                                  const size_t rowBufferSize, const GolfIndexRowSink sink,
                                                  void* user) {
  GolfIndexGroupWriteResult result{};
  if (rowBuffer == nullptr || rowBufferSize == 0 || sink == nullptr) return result;
  const uint8_t expectedMask = golfEnabledPlayerMask(round);
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((expectedMask & (1U << slot)) == 0) continue;
    if (!golfMakeIndexRow(round, slot, filename, rowScratch) ||
        !golfFormatIndexRow(rowScratch, rowBuffer, rowBufferSize) ||
        !sink(rowBuffer, strlen(rowBuffer), user)) {
      return result;
    }
    result.slotMask |= static_cast<uint8_t>(1U << slot);
    ++result.rowCount;
  }
  result.complete = result.rowCount > 0 && result.slotMask == expectedMask;
  return result;
}

#endif
