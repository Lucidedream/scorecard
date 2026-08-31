#pragma once

#include <cstddef>
#include <cstdint>

#include "golf/GolfRound.h"

void golfFormatReviewToPar(int16_t value, char* output, size_t size);
void golfFormatRoundStatus(const GolfRound& round, char* output, size_t size);
void golfFormatReviewPercent(uint16_t part, uint16_t whole, char* output, size_t size);
