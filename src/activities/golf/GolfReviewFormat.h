#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "golf/GolfRound.h"

inline constexpr size_t GOLF_PLAYER_LABEL_CAPACITY = GolfPlayer::NAME_CAPACITY + 4;

size_t golfUtf8PrefixLength(std::string_view text, size_t limit);
bool golfPlayerNameHasVisibleText(std::string_view name);
void golfFormatPlayerLabel(uint8_t playerSlot, const char* playerName, const char* format, char* output, size_t size);
void golfFormatReviewToPar(int16_t value, const char* evenText, const char* positiveFormat,
                           const char* negativeFormat, char* output, size_t size);
void golfFormatRoundStatus(const GolfRound& round, const GolfPlayerScore& score, const char* evenText,
                           const char* positiveFormat, const char* negativeFormat, const char* statusFormat,
                           char* output, size_t size);
void golfFormatReviewPercent(uint16_t part, uint16_t whole, const char* format, char* output, size_t size);
