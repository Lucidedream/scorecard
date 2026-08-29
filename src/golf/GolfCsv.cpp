#include "GolfCsv.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>
#include <cstring>
#include <limits>

namespace {

bool appendChar(char value, char* output, size_t outputSize, size_t& written) {
  if (written + 1 >= outputSize) {
    return false;
  }
  output[written++] = value;
  return true;
}

bool appendText(const char* value, char* output, size_t outputSize, size_t& written) {
  while (*value != '\0') {
    if (!appendChar(*value++, output, outputSize, written)) {
      return false;
    }
  }
  return true;
}

bool appendCourse(const char* course, char* output, size_t outputSize, size_t& written) {
  const bool quote = strpbrk(course, ",\"\r\n") != nullptr;
  if (quote && !appendChar('"', output, outputSize, written)) {
    return false;
  }
  for (const char* current = course; *current != '\0'; ++current) {
    if (*current == '"' && !appendChar('"', output, outputSize, written)) {
      return false;
    }
    if (!appendChar(*current, output, outputSize, written)) {
      return false;
    }
  }
  return !quote || appendChar('"', output, outputSize, written);
}

bool parseField(const char*& current, char* output, size_t outputSize) {
  size_t written = 0;
  const bool quoted = *current == '"';
  bool closed = !quoted;
  if (quoted) {
    ++current;
  }
  while (*current != '\0') {
    if (quoted && *current == '"') {
      if (current[1] == '"') {
        ++current;
      } else {
        ++current;
        closed = true;
        if (*current != ',' && *current != '\r' && *current != '\n' && *current != '\0') {
          return false;
        }
        break;
      }
    } else if (!quoted && (*current == ',' || *current == '\r' || *current == '\n')) {
      break;
    }
    if (written + 1 >= outputSize) {
      return false;
    }
    output[written++] = *current++;
  }
  if (!closed) {
    return false;
  }
  output[written] = '\0';
  return true;
}

bool parseUnsigned(const char* value, uint16_t maximum, uint16_t& parsed) {
  if (*value == '\0') {
    return false;
  }
  uint32_t result = 0;
  while (*value != '\0') {
    if (*value < '0' || *value > '9') {
      return false;
    }
    result = result * 10 + static_cast<uint8_t>(*value++ - '0');
    if (result > maximum) {
      return false;
    }
  }
  parsed = static_cast<uint16_t>(result);
  return true;
}

}  // namespace

bool golfFormatIndexRow(const GolfIndexRowView& row, char* output, size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return false;
  }
  size_t written = 0;
  char totals[64];
  const int totalsLength = snprintf(totals, sizeof(totals), ",%u,%u,%u,%u,%u,%u,", row.holes, row.strokes, row.par,
                                    row.putts, row.in100, row.out100);
  if (totalsLength < 0 || static_cast<size_t>(totalsLength) >= sizeof(totals) ||
      !appendText(row.date, output, outputSize, written) || !appendChar(',', output, outputSize, written) ||
      !appendCourse(row.course, output, outputSize, written) || !appendText(totals, output, outputSize, written) ||
      !appendText(row.file, output, outputSize, written) || !appendText("\r\n", output, outputSize, written)) {
    output[0] = '\0';
    return false;
  }
  output[written] = '\0';
  return true;
}

bool golfFormatIndexRow(const GolfIndexRow& row, char* output, size_t outputSize) {
  return golfFormatIndexRow(
      {row.date, row.course, row.holes, row.strokes, row.par, row.putts, row.in100, row.out100, row.file}, output,
      outputSize);
}

bool golfParseIndexRow(const char* input, GolfIndexRow& row) {
  if (input == nullptr) {
    return false;
  }
  GolfIndexRow parsed{};
  char numeric[6]{};
  const char* current = input;
  if (!parseField(current, parsed.date, sizeof(parsed.date)) || *current++ != ',' ||
      !parseField(current, parsed.course, sizeof(parsed.course)) || *current++ != ',') {
    return false;
  }
  uint16_t holes = 0;
  uint16_t* values[] = {&holes, &parsed.strokes, &parsed.par, &parsed.putts, &parsed.in100, &parsed.out100};
  for (uint8_t field = 0; field < 6; ++field) {
    if (!parseField(current, numeric, sizeof(numeric)) ||
        !parseUnsigned(numeric, field == 0 ? std::numeric_limits<uint8_t>::max() : std::numeric_limits<uint16_t>::max(),
                       *values[field]) ||
        *current++ != ',') {
      return false;
    }
  }
  if (!parseField(current, parsed.file, sizeof(parsed.file))) {
    return false;
  }
  if (*current == '\r') {
    ++current;
  }
  if (*current == '\n') {
    ++current;
  }
  if (*current != '\0') {
    return false;
  }
  parsed.holes = static_cast<uint8_t>(holes);
  row = parsed;
  return true;
}

#endif
