#pragma once

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "GolfPenalty.h"

// JSON-array and string readers shared by GolfRoundStore (state.json) and
// GolfRoundFile (completed-round files). Both files decode the same golf-round
// shape, so the element-by-element reading lives here once rather than in each
// translation unit. Firmware-only: it pulls in ArduinoJson. The length-vs-holes and
// version decisions live in the JSON-free GolfRoundDecode instead.

// Copies a JSON string value into a fixed buffer. Rejects a non-string, or one that
// would not fit including its terminator.
inline bool golfReadJsonString(JsonVariantConst value, char* output, size_t capacity) {
  if (!value.is<const char*>()) {
    return false;
  }
  const char* source = value.as<const char*>();
  const size_t length = strlen(source);
  if (length >= capacity) {
    return false;
  }
  memcpy(output, source, length + 1);
  return true;
}

inline void golfAddJsonPenalties(JsonDocument& doc, const GolfRound& round) {
  JsonArray holes = doc["penalties"].to<JsonArray>();
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    JsonArray events = holes.add<JsonArray>();
    const uint8_t count = round.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                              ? round.penaltyCount[hole]
                              : GolfRound::MAX_PENALTIES_PER_HOLE;
    for (uint8_t index = 0; index < count; ++index) {
      GolfPenaltyEvent event{};
      if (!golfPenaltyEventAt(round, hole, index, event)) continue;
      JsonArray pair = events.add<JsonArray>();
      pair.add(static_cast<uint8_t>(event.field));
      pair.add(static_cast<uint8_t>(event.kind));
    }
  }
}

inline bool golfReadJsonPenalties(const JsonVariantConst value, GolfRound& round, uint16_t& holeCount) {
  const JsonArrayConst holes = value.as<JsonArrayConst>();
  if (holes.isNull()) {
    holeCount = 0;
    return false;
  }
  const size_t size = holes.size();
  holeCount = size > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(size);
  const uint16_t readHoles = holeCount < GolfRound::MAX_HOLES ? holeCount : GolfRound::MAX_HOLES;
  for (uint16_t hole = 0; hole < readHoles; ++hole) {
    const JsonArrayConst events = holes[hole].as<JsonArrayConst>();
    if (events.isNull()) {
      round.penaltyCount[hole] = 1;
      round.penaltyEvents[hole][0] = 0x08;
      continue;
    }
    const size_t eventCount = events.size();
    round.penaltyCount[hole] = eventCount > GolfRound::MAX_PENALTIES_PER_HOLE
                                   ? static_cast<uint8_t>(GolfRound::MAX_PENALTIES_PER_HOLE + 1)
                                   : static_cast<uint8_t>(eventCount);
    const uint8_t readEvents = eventCount < GolfRound::MAX_PENALTIES_PER_HOLE ? static_cast<uint8_t>(eventCount)
                                                                              : GolfRound::MAX_PENALTIES_PER_HOLE;
    for (uint8_t index = 0; index < readEvents; ++index) {
      uint8_t packed = 0x08;
      const JsonArrayConst pair = events[index].as<JsonArrayConst>();
      if (!pair.isNull() && pair.size() == 2 && pair[0].is<int>() && pair[1].is<int>()) {
        const int field = pair[0].as<int>();
        const int kind = pair[1].as<int>();
        if (field >= 0 && field <= static_cast<int>(GolfField::Out100) && kind >= 0 &&
            kind <= static_cast<int>(GolfPenaltyKind::Ob)) {
          packed = golfPackPenaltyEvent(static_cast<GolfField>(field), static_cast<GolfPenaltyKind>(kind));
        }
      }
      const uint8_t shift = index % 2 == 0 ? 0 : 4;
      uint8_t& target = round.penaltyEvents[hole][index / 2];
      target = static_cast<uint8_t>((target & ~(0x0f << shift)) | ((packed & 0x0f) << shift));
    }
  }
  return true;
}

// Reads a JSON array of integers into `output`, clamping bounds checks against
// `maximum`, and reports the array's actual length in `count`. Writes at most
// `capacity` elements; a longer array still reports its true `count` so the caller's
// length-equals-holes check can reject it. Returns false if the value is not an
// array, an element is not an integer, or an element is negative or above `maximum`.
template <typename T>
bool golfReadJsonHoleArray(JsonVariantConst value, T* output, uint16_t capacity, int32_t maximum, uint16_t& count) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.isNull()) {
    count = 0;
    return false;
  }
  const size_t size = array.size();
  count = size > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(size);
  const uint16_t readCount = count < capacity ? count : capacity;
  for (uint16_t index = 0; index < readCount; ++index) {
    const JsonVariantConst element = array[index];
    if (!element.is<int>()) {
      return false;
    }
    const int item = element.as<int>();
    if (item < 0 || item > maximum) {
      return false;
    }
    output[index] = static_cast<T>(item);
  }
  return true;
}

#endif
