#pragma once

#include <cstdint>

class GfxRenderer;

void golfDrawLargeNumber(const GfxRenderer& renderer, int centerX, int y, int height, uint16_t value, bool ink = true,
                         bool outline = false);
