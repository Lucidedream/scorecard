#pragma once

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

// JSON-array and string readers shared by GolfRoundStore (state.json) and
// GolfRoundFile (completed-round files). Both files decode the same v2 golf-round
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
