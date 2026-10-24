#pragma once

#include <cstddef>
#include <cstdint>

#include "GolfCsv.h"

inline constexpr uint8_t GOLF_CATALOG_MAX_REGIONS = 64;
inline constexpr uint16_t GOLF_CATALOG_MAX_REGION_COURSES = 512;
inline constexpr size_t GOLF_CATALOG_RECORD_SIZE = 96;

struct GolfCatalogRegion {
  char name[24]{};
  uint16_t count = 0;
  uint16_t firstRecord = 0;
  uint8_t index = 0;
};
static_assert(sizeof(GolfCatalogRegion) <= 32);
struct GolfCatalogRecord {
  char name[40]{};
  uint32_t id = 0;
  uint32_t reviews = 0;
  uint16_t par = 0;
  uint8_t region = 0;
  uint8_t nineCount = 0;
  uint8_t rating = 0;
  bool hasRating = false;
  bool hasMaps = false;
};
bool golfParseCatalogRecord(const char* bytes, size_t size, GolfCatalogRecord& record);
void golfSortCatalogOrdinals(uint16_t* records, uint16_t count, const uint8_t* ratings, uint16_t firstRecord);

// Bad lines are skipped; a bad header makes the whole reader invalid.
class GolfCatalogRegionsReader {
 public:
  void feed(const char* bytes, size_t size);
  bool finish();
  bool valid() const { return headerValid; }
  GolfCatalogRegion regions[GOLF_CATALOG_MAX_REGIONS]{};
  uint16_t courseCount = 0;
  uint8_t count = 0;

 private:
  char line[128]{};
  uint16_t length = 0;
  uint16_t declaredRegions = 0;
  uint32_t lineNumber = 0;
  bool overflow = false;
  bool headerValid = false;
  void acceptLine();
};

using GolfRecentResolver = bool (*)(const char* name, void* user);
class GolfRecentReader {
 public:
  void feedReverse(const char* bytes, size_t size, GolfRecentResolver resolver = nullptr, void* user = nullptr);
  void finishReverse(GolfRecentResolver resolver = nullptr, void* user = nullptr);
  char names[5][40]{};
  uint8_t count = 0;

 private:
  char line[GOLF_CSV_ROW_BUFFER_SIZE]{};
  GolfIndexRow parsed{};
  uint16_t length = 0;
  bool overflow = false;
  bool reverseBoundary = false;
  void acceptNewestLine(GolfRecentResolver resolver, void* user);
};
