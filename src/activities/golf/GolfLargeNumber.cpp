#include "GolfLargeNumber.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>

namespace {

uint8_t segmentMask(const uint8_t digit) {
  static constexpr uint8_t MASKS[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  return digit < 10 ? MASKS[digit] : 0;
}

void drawSegment(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                 const bool ink, const bool outline) {
  if (outline) {
    renderer.drawRect(x, y, width, height, 2, ink);
  } else {
    renderer.fillRect(x, y, width, height, ink);
  }
}

void drawDigit(const GfxRenderer& renderer, const int x, const int y, const int height, const uint8_t digit,
               const bool ink, const bool outline) {
  const int thickness = height / 10;
  const int width = height * 11 / 20;
  const int half = height / 2;
  const uint8_t mask = segmentMask(digit);
  if (mask & 0x01) drawSegment(renderer, x + thickness, y, width - thickness * 2, thickness, ink, outline);
  if (mask & 0x02)
    drawSegment(renderer, x + width - thickness, y + thickness, thickness, half - thickness, ink, outline);
  if (mask & 0x04) drawSegment(renderer, x + width - thickness, y + half, thickness, half - thickness, ink, outline);
  if (mask & 0x08)
    drawSegment(renderer, x + thickness, y + height - thickness, width - thickness * 2, thickness, ink, outline);
  if (mask & 0x10) drawSegment(renderer, x, y + half, thickness, half - thickness, ink, outline);
  if (mask & 0x20) drawSegment(renderer, x, y + thickness, thickness, half - thickness, ink, outline);
  if (mask & 0x40)
    drawSegment(renderer, x + thickness, y + half - thickness / 2, width - thickness * 2, thickness, ink, outline);
}

}  // namespace

void golfDrawLargeNumber(const GfxRenderer& renderer, const int centerX, const int y, const int height,
                         const uint16_t value, const bool ink, const bool outline) {
  const int digitWidth = height * 11 / 20;
  const int gap = height / 10;
  const uint8_t digits = value >= 100 ? 3 : value >= 10 ? 2 : 1;
  const int totalWidth = digitWidth * digits + gap * (digits - 1);
  int x = centerX - totalWidth / 2;
  uint16_t divisor = digits == 3 ? 100 : digits == 2 ? 10 : 1;
  while (divisor > 0) {
    drawDigit(renderer, x, y, height, static_cast<uint8_t>((value / divisor) % 10), ink, outline);
    x += digitWidth + gap;
    divisor /= 10;
  }
}

#endif
