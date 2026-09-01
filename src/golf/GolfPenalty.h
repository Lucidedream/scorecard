#pragma once

#include <cstdint>

#include "GolfRound.h"
#include "GolfRules.h"

enum class GolfPenaltyKind : uint8_t { Hazard = 0, Ob = 1 };

struct GolfPenaltyEvent {
  GolfField field;
  GolfPenaltyKind kind;
};

enum class GolfPenaltyMutationStatus : uint8_t {
  Changed,
  NoMarker,
  HoleFull,
  CounterClamped,
  InvalidHole,
  InvalidEvent,
};

uint8_t golfPackPenaltyEvent(GolfField field, GolfPenaltyKind kind);
bool golfUnpackPenaltyEvent(uint8_t packed, GolfPenaltyEvent& event);
bool golfPenaltyEventAt(const GolfPlayerScore& score, uint8_t hole, uint8_t index, GolfPenaltyEvent& event);
GolfPenaltyMutationStatus golfAppendPenalty(GolfPlayerScore& score, uint8_t hole, GolfField field,
                                            GolfPenaltyKind kind);
GolfPenaltyMutationStatus golfRemoveLatestPenalty(GolfPlayerScore& score, uint8_t hole, GolfField field);
uint8_t golfHazardsForHole(const GolfPlayerScore& score, uint8_t hole);
uint8_t golfObsForHole(const GolfPlayerScore& score, uint8_t hole);
uint16_t golfHazardsForRound(const GolfPlayerScore& score, uint8_t holeCount);
uint16_t golfObsForRound(const GolfPlayerScore& score, uint8_t holeCount);
uint16_t golfPenaltyStrokesForHole(const GolfPlayerScore& score, uint8_t hole);
uint16_t golfPenaltyStrokesForRound(const GolfPlayerScore& score, uint8_t holeCount);
