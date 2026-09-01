#include "GolfReviewFormat.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>
#include <cstring>

#include "golf/GolfStats.h"

size_t golfUtf8PrefixLength(const std::string_view text, const size_t limit) {
  if (text.size() <= limit) return text.size();
  size_t length = limit;
  while (length > 0 && (static_cast<uint8_t>(text[length]) & 0xC0U) == 0x80U) --length;
  return length;
}

bool golfPlayerNameHasVisibleText(const std::string_view name) {
  for (const char value : name) {
    const uint8_t byte = static_cast<uint8_t>(value);
    if (byte > 0x7FU || (value != ' ' && value != '\t' && value != '\n' && value != '\r' && value != '\f' &&
                        value != '\v')) {
      return true;
    }
  }
  return false;
}

void golfFormatPlayerLabel(const uint8_t playerSlot, const char* playerName, const char* format, char* output,
                           const size_t size) {
  if (output == nullptr || size == 0) return;
  output[0] = '\0';
  if (format == nullptr) return;

  char boundedName[GolfPlayer::NAME_CAPACITY]{};
  const size_t sourceLength =
      playerName == nullptr ? 0 : strnlen(playerName, static_cast<size_t>(GolfPlayer::NAME_CAPACITY));
  size_t nameLength = sourceLength;
  if (nameLength >= sizeof(boundedName)) {
    nameLength = golfUtf8PrefixLength(std::string_view(playerName, sourceLength), sizeof(boundedName) - 1);
  }
  if (nameLength > 0) memcpy(boundedName, playerName, nameLength);

  while (true) {
    const int written =
        snprintf(output, size, format, static_cast<unsigned>(playerSlot) + 1U, boundedName);
    if (written >= 0 && static_cast<size_t>(written) < size) return;
    if (nameLength == 0) {
      output[size - 1] = '\0';
      return;
    }
    nameLength = golfUtf8PrefixLength(std::string_view(boundedName, nameLength), nameLength - 1);
    boundedName[nameLength] = '\0';
  }
}

void golfFormatReviewToPar(const int16_t value, const char* evenText, const char* positiveFormat,
                           const char* negativeFormat, char* output, const size_t size) {
  if (output == nullptr || size == 0) return;
  output[0] = '\0';
  if (value == 0) {
    if (evenText != nullptr) snprintf(output, size, "%s", evenText);
    return;
  }
  const char* format = value > 0 ? positiveFormat : negativeFormat;
  if (format != nullptr) snprintf(output, size, format, static_cast<int>(value));
}

void golfFormatRoundStatus(const GolfRound& round, const GolfPlayerScore& score, const char* evenText,
                           const char* positiveFormat, const char* negativeFormat, const char* statusFormat,
                           char* output, const size_t size) {
  if (output == nullptr || size == 0) return;
  output[0] = '\0';
  if (!golfHasPar(round)) {
    snprintf(output, size, "%u", static_cast<unsigned>(golfScore(round, score)));
    return;
  }
  char toPar[8];
  golfFormatReviewToPar(golfToPar(round, score), evenText, positiveFormat, negativeFormat, toPar, sizeof(toPar));
  if (statusFormat != nullptr) {
    snprintf(output, size, statusFormat, static_cast<unsigned>(golfScore(round, score)), toPar);
  }
}

void golfFormatReviewPercent(const uint16_t part, const uint16_t whole, const char* format, char* output,
                             const size_t size) {
  if (output == nullptr || size == 0) return;
  output[0] = '\0';
  if (format == nullptr) return;
  const uint16_t tenths =
      whole == 0 ? 0 : static_cast<uint16_t>((static_cast<uint32_t>(part) * 1000 + whole / 2) / whole);
  snprintf(output, size, format, static_cast<unsigned long>(tenths / 10),
           static_cast<unsigned long>(tenths % 10));
}

#endif
