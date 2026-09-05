#pragma once

#include <cstdint>

#include "golf/GolfRound.h"

enum class GolfPlayerSetupNext : uint8_t { ReviewRoster };

constexpr uint8_t golfClampPlayerCount(const uint8_t count) {
  return count < 1 ? 1 : (count > GOLF_MAX_PLAYERS ? GOLF_MAX_PLAYERS : count);
}

constexpr uint8_t golfStepPlayerCount(const uint8_t count, const int direction) {
  const int next = static_cast<int>(golfClampPlayerCount(count)) + direction;
  return static_cast<uint8_t>(next < 1 ? 1 : (next > GOLF_MAX_PLAYERS ? GOLF_MAX_PLAYERS : next));
}

constexpr GolfPlayerSetupNext golfPlayerSetupNext(const uint8_t) { return GolfPlayerSetupNext::ReviewRoster; }

inline void golfApplyPlayerCount(GolfRound& round, const uint8_t count, const TeeSelection defaultTee) {
  const uint8_t enabledCount = golfClampPlayerCount(count);
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    round.players[slot].tee = slot < enabledCount ? defaultTee : TeeSelection::NotPlay;
  }
}

inline void golfSetPlayerCount(GolfRound& round, uint8_t& playerCount, const uint8_t count,
                               const TeeSelection defaultTee) {
  playerCount = golfClampPlayerCount(count);
  golfApplyPlayerCount(round, playerCount, defaultTee);
}
