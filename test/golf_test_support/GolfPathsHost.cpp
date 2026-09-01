#include "GolfPaths.h"

#if defined(CROSSPOINT_GOLF)

// The Windows host toolchain has no gmtime_r, so data-layer suites link only
// the date codec they exercise instead of the firmware-only timestamp helper.

#include <cstdio>
#include <cstring>

namespace {

bool isLeapYear(const uint16_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  static constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) return 29;
  return month >= 1 && month <= 12 ? DAYS[month - 1] : 0;
}

}  // namespace

bool golfFormatDate(const uint16_t dateYmd, char* output, const size_t outputSize) {
  if (output == nullptr || outputSize < GOLF_DATE_BUFFER_SIZE) return false;
  const uint16_t year = 2000 + (dateYmd >> 9);
  const uint8_t month = (dateYmd >> 5) & 0x0f;
  const uint8_t day = dateYmd & 0x1f;
  if (day == 0 || day > daysInMonth(year, month)) {
    output[0] = '\0';
    return false;
  }
  return snprintf(output, outputSize, "%04u-%02u-%02u", year, month, day) == 10;
}

bool golfParseDate(const char* date, uint16_t& dateYmd) {
  if (date == nullptr || strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
  for (uint8_t index = 0; index < 10; ++index) {
    if (index != 4 && index != 7 && (date[index] < '0' || date[index] > '9')) return false;
  }
  const uint16_t year =
      static_cast<uint16_t>((date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0');
  const uint8_t month = static_cast<uint8_t>((date[5] - '0') * 10 + date[6] - '0');
  const uint8_t day = static_cast<uint8_t>((date[8] - '0') * 10 + date[9] - '0');
  if (year < 2000 || year > 2127 || day == 0 || day > daysInMonth(year, month)) return false;
  dateYmd = static_cast<uint16_t>(((year - 2000) << 9) | (month << 5) | day);
  return true;
}

#endif
