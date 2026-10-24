#pragma once
#include <ArduinoJson.h>

#include <cstdint>

struct GolfCourseCatalogInfo {
  uint32_t id = 0;
  uint32_t reviews = 0;
  uint32_t yards = 0;
  uint16_t holes = 0;
  uint16_t par = 0;
  uint16_t opened = 0;
  uint8_t rating = 0;
  uint8_t scores[6]{};
  char region[24]{};
  char designer[32]{};
  bool hasRating = false;
  bool hasReviews = false;
  bool hasScores = false;
  bool hasRegion = false;
  bool hasHoles = false;
  bool hasPar = false;
  bool hasYards = false;
  bool hasDesigner = false;
  bool hasOpened = false;
};
// Reads the optional baigolf object from a course document; failure clears info.
bool golfParseCatalogInfo(JsonVariantConst json, GolfCourseCatalogInfo& info);
