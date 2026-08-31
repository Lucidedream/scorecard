#include "GolfPaths.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

bool isAsciiAlphanumeric(unsigned char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9');
}

bool isLeapYear(uint16_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

uint8_t daysInMonth(uint16_t year, uint8_t month) {
  static constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return month >= 1 && month <= 12 ? DAYS[month - 1] : 0;
}

}  // namespace

bool golfSlug(const char* courseName, char* output, size_t outputSize) {
  if (courseName == nullptr || output == nullptr || outputSize == 0) {
    return false;
  }
  size_t written = 0;
  bool separatorPending = false;
  for (const auto* current = reinterpret_cast<const unsigned char*>(courseName); *current != 0; ++current) {
    if (!isAsciiAlphanumeric(*current)) {
      separatorPending = written != 0;
      continue;
    }
    if (separatorPending && written < 40) {
      if (written + 1 >= outputSize) {
        output[0] = '\0';
        return false;
      }
      output[written++] = '-';
    }
    separatorPending = false;
    if (written >= 40) {
      break;
    }
    if (written + 1 >= outputSize) {
      output[0] = '\0';
      return false;
    }
    output[written++] =
        *current >= 'A' && *current <= 'Z' ? static_cast<char>(*current + ('a' - 'A')) : static_cast<char>(*current);
  }
  if (written != 0 && output[written - 1] == '-') {
    --written;
  }
  if (written == 0) {
    static constexpr char FALLBACK[] = "course";
    if (outputSize < sizeof(FALLBACK)) {
      output[0] = '\0';
      return false;
    }
    memcpy(output, FALLBACK, sizeof(FALLBACK));
    return true;
  }
  if (written >= outputSize) {
    output[0] = '\0';
    return false;
  }
  output[written] = '\0';
  return true;
}

bool golfFormatDate(uint16_t dateYmd, char* output, size_t outputSize) {
  if (output == nullptr || outputSize < GOLF_DATE_BUFFER_SIZE) {
    return false;
  }
  const uint16_t year = 2000 + (dateYmd >> 9);
  const uint8_t month = (dateYmd >> 5) & 0x0F;
  const uint8_t day = dateYmd & 0x1F;
  if (day == 0 || day > daysInMonth(year, month)) {
    output[0] = '\0';
    return false;
  }
  return snprintf(output, outputSize, "%04u-%02u-%02u", year, month, day) == 10;
}

bool golfParseDate(const char* date, uint16_t& dateYmd) {
  if (date == nullptr || strlen(date) != 10 || date[4] != '-' || date[7] != '-') {
    return false;
  }
  for (uint8_t i = 0; i < 10; ++i) {
    if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) {
      return false;
    }
  }
  const uint16_t year =
      static_cast<uint16_t>((date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0');
  const uint8_t month = static_cast<uint8_t>((date[5] - '0') * 10 + date[6] - '0');
  const uint8_t day = static_cast<uint8_t>((date[8] - '0') * 10 + date[9] - '0');
  if (year < 2000 || year > 2127 || day == 0 || day > daysInMonth(year, month)) {
    return false;
  }
  dateYmd = static_cast<uint16_t>(((year - 2000) << 9) | (month << 5) | day);
  return true;
}

bool golfDateFromTimestamp(const int64_t timestamp, const int16_t utcOffsetMinutes, uint16_t& dateYmd) {
  const int64_t adjusted = timestamp + static_cast<int64_t>(utcOffsetMinutes) * 60;
  const time_t value = static_cast<time_t>(adjusted);
  tm calendar{};
  if (static_cast<int64_t>(value) != adjusted || gmtime_r(&value, &calendar) == nullptr) return false;
  const uint16_t year = static_cast<uint16_t>(calendar.tm_year + 1900);
  if (year < 2020 || year > 2127) return false;
  dateYmd = static_cast<uint16_t>(((year - 2000) << 9) | ((calendar.tm_mon + 1) << 5) | calendar.tm_mday);
  return true;
}

bool golfRoundFilename(uint16_t roundSequence, const char* courseName, uint16_t collisionSuffix, char* output,
                       size_t outputSize) {
  if (output == nullptr || outputSize == 0 || roundSequence == 0 || roundSequence > 9999 || collisionSuffix == 1) {
    return false;
  }
  char slug[GOLF_SLUG_BUFFER_SIZE];
  if (!golfSlug(courseName, slug, sizeof(slug))) {
    output[0] = '\0';
    return false;
  }
  const int length = collisionSuffix == 0
                         ? snprintf(output, outputSize, "round-%04u-%s.json", roundSequence, slug)
                         : snprintf(output, outputSize, "round-%04u-%s-%u.json", roundSequence, slug, collisionSuffix);
  if (length < 0 || static_cast<size_t>(length) >= outputSize) {
    output[0] = '\0';
    return false;
  }
  return true;
}

#endif
