#pragma once

#include <cstddef>
#include <cstdint>

inline constexpr size_t GOLF_SLUG_BUFFER_SIZE = 41;
inline constexpr size_t GOLF_DATE_BUFFER_SIZE = 11;
inline constexpr size_t GOLF_ROUND_FILENAME_BUFFER_SIZE = 64;

bool golfSlug(const char* courseName, char* output, size_t outputSize);
bool golfFormatDate(uint16_t dateYmd, char* output, size_t outputSize);
bool golfParseDate(const char* date, uint16_t& dateYmd);
bool golfRoundFilename(uint16_t roundSequence, const char* courseName, uint16_t collisionSuffix, char* output,
                       size_t outputSize);
