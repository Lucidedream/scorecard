#pragma once

#include <cstdint>

#include "golf/GolfRound.h"

enum class GolfPlayerSelectState : uint8_t { Ready, Empty, LoadError };

constexpr GolfPlayerSelectState golfPlayerSelectState(const bool loadSucceeded, const uint8_t presentMask) {
  if (!loadSucceeded) return GolfPlayerSelectState::LoadError;
  return presentMask == 0 ? GolfPlayerSelectState::Empty : GolfPlayerSelectState::Ready;
}

constexpr uint8_t golfPlayerSelectCount(const uint8_t presentMask) {
  uint8_t count = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((presentMask & static_cast<uint8_t>(1U << slot)) != 0) ++count;
  }
  return count;
}

constexpr uint8_t golfPlayerSelectSlotAt(const uint8_t presentMask, const uint8_t row) {
  uint8_t presentRow = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if ((presentMask & static_cast<uint8_t>(1U << slot)) == 0) continue;
    if (presentRow++ == row) return slot;
  }
  return GolfRound::NO_PLAYER;
}

constexpr bool golfPlayerSelectSlotPresent(const uint8_t presentMask, const int slot) {
  return slot >= 0 && slot < GolfRound::MAX_PLAYERS && (presentMask & static_cast<uint8_t>(1U << slot)) != 0;
}

constexpr int golfPlayerSelectFirstPresent(const uint8_t presentMask) {
  for (int slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (golfPlayerSelectSlotPresent(presentMask, slot)) return slot;
  }
  return 0;
}

constexpr int golfPlayerSelectNextPresent(const uint8_t presentMask, const int current, const int direction) {
  if (presentMask == 0) return current >= 0 && current < GolfRound::MAX_PLAYERS ? current : 0;
  int candidate = current >= 0 && current < GolfRound::MAX_PLAYERS ? current : 0;
  const int step = direction < 0 ? -1 : 1;
  for (int checked = 0; checked < GolfRound::MAX_PLAYERS; ++checked) {
    candidate = (candidate + step + GolfRound::MAX_PLAYERS) % GolfRound::MAX_PLAYERS;
    if (golfPlayerSelectSlotPresent(presentMask, candidate)) return candidate;
  }
  return golfPlayerSelectFirstPresent(presentMask);
}
