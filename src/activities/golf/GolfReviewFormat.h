#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "golf/CourseStore.h"
#include "golf/GolfRound.h"

inline constexpr size_t GOLF_PLAYER_LABEL_CAPACITY = GolfPlayer::NAME_CAPACITY + 4;

// Display label for a tee name (CONTRACTS-V2 §32.4): the built-in course's "Blue" and
// "White" keep their translated UI strings, an empty string is the "did not play"
// sentinel, and every other (free-form / SD-course) tee name prints verbatim.
const char* golfTeeDisplayLabel(const char* tee);

// Joins a course's tee display labels into `output` for a course-list subtitle
// (CONTRACTS-V2 §32.3), e.g. "Blue · White · Black". An empty set prints an em dash;
// names that overrun `size` are truncated with an ellipsis.
void golfFormatTeeList(const GolfCourseTeeSet& teeSet, char* output, size_t size);

size_t golfUtf8PrefixLength(std::string_view text, size_t limit);
bool golfPlayerNameHasVisibleText(std::string_view name);
void golfFormatPlayerLabel(uint8_t playerSlot, const char* playerName, const char* format, char* output, size_t size);
void golfFormatReviewToPar(int16_t value, const char* evenText, const char* positiveFormat, const char* negativeFormat,
                           char* output, size_t size);
void golfFormatRoundStatus(const GolfRound& round, const GolfPlayerScore& score, const char* evenText,
                           const char* positiveFormat, const char* negativeFormat, const char* statusFormat,
                           char* output, size_t size);
void golfFormatReviewPercent(uint16_t part, uint16_t whole, const char* format, char* output, size_t size);
