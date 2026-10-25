#include "GolfCatalog.h"
#if defined(CROSSPOINT_GOLF)
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {
bool number(const char* p, size_t n, uint32_t& out) {
  out = 0;
  for (size_t i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return false;
    out = out * 10 + p[i] - '0';
  }
  return n != 0;
}
bool validUtf8(const char* text, size_t size) {
  for (size_t i = 0; i < size;) {
    const auto lead = static_cast<uint8_t>(text[i++]);
    if (lead < 0x80) continue;
    const uint8_t extra = lead >= 0xc2 && lead <= 0xdf   ? 1
                          : lead >= 0xe0 && lead <= 0xef ? 2
                          : lead >= 0xf0 && lead <= 0xf4 ? 3
                                                         : 0;
    if (!extra || i + extra > size) return false;
    const auto next = static_cast<uint8_t>(text[i]);
    if ((lead == 0xe0 && next < 0xa0) || (lead == 0xed && next >= 0xa0) || (lead == 0xf0 && next < 0x90) ||
        (lead == 0xf4 && next >= 0x90))
      return false;
    for (uint8_t n = 0; n < extra; ++n)
      if ((static_cast<uint8_t>(text[i++]) & 0xc0) != 0x80) return false;
  }
  return true;
}
bool fieldNumber(const char*& p, uint32_t& out, char delimiter) {
  const char* start = p;
  while (*p >= '0' && *p <= '9') ++p;
  if (p - start > 5 || !number(start, p - start, out) || *p != delimiter) return false;
  if (*p) ++p;
  return true;
}
}  // namespace

bool golfParseCatalogRecord(const char* b, size_t size, GolfCatalogRecord& out) {
  if (!b || size != GOLF_CATALOG_RECORD_SIZE) return false;
  for (uint8_t i : {8, 11, 13, 17, 20, 26, 28, 69})
    if (b[i] != '|') return false;
  for (uint8_t i = 70; i < 95; ++i)
    if (b[i] != ' ') return false;
  if (b[95] != '\n' || (b[27] != 'Y' && b[27] != 'N')) return false;
  GolfCatalogRecord r{};
  uint32_t n;
  if (!number(b, 8, r.id) || !r.id || !number(b + 9, 2, n) || n >= GOLF_CATALOG_MAX_REGIONS) return false;
  r.region = n;
  if (!number(b + 12, 1, n) || n < 1 || n > 4) return false;
  r.nineCount = n;
  if (!number(b + 14, 3, n) || !n) return false;
  r.par = n;
  r.hasRating = b[18] != '-' || b[19] != '-';
  if (r.hasRating) {
    if (!number(b + 18, 2, n) || n > 50) return false;
    r.rating = n;
  }
  if (!number(b + 21, 5, r.reviews) || (r.hasRating && !r.reviews) || (!r.hasRating && r.reviews)) return false;
  size_t length = 40;
  while (length && b[29 + length - 1] == ' ') --length;
  if (!length || length >= sizeof(r.name)) return false;
  for (size_t i = 0; i < length; ++i) {
    if (static_cast<unsigned char>(b[29 + i]) < 32 || b[29 + i] == 127 || b[29 + i] == '|' || b[29 + i] == ',')
      return false;
  }
  if (!validUtf8(b + 29, length)) return false;
  memcpy(r.name, b + 29, length);
  r.hasMaps = b[27] == 'Y';
  out = r;
  return true;
}
void golfSortCatalogRegions(GolfCatalogRegion* regions, const uint8_t count) {
  std::sort(regions, regions + count, [](const GolfCatalogRegion& a, const GolfCatalogRegion& b) {
    return a.count != b.count ? a.count > b.count : strcmp(a.name, b.name) < 0;
  });
}
void golfSortCatalogOrdinals(uint16_t* records, uint16_t count, const uint8_t* ratings, uint16_t firstRecord) {
  std::sort(records, records + count, [ratings, firstRecord](uint16_t a, uint16_t b) {
    const uint8_t left = ratings[a - firstRecord], right = ratings[b - firstRecord];
    return left != right ? left > right : a < b;
  });
}

void GolfCatalogRegionsReader::feed(const char* bytes, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (bytes[i] == '\n') {
      acceptLine();
      length = 0;
      overflow = false;
    } else if (bytes[i] != '\r') {
      if (length + 1 < sizeof(line))
        line[length++] = bytes[i];
      else
        overflow = true;
    }
  }
}
void GolfCatalogRegionsReader::acceptLine() {
  line[length] = 0;
  ++lineNumber;
  if (lineNumber == 1) {
    constexpr char PREFIX[] = "# golf-catalog 1 ";
    const char* p = line + sizeof(PREFIX) - 1;
    uint32_t regionsN = 0, coursesN = 0;
    uint16_t date;
    char dateString[11]{};
    if (!overflow && length >= sizeof(PREFIX) + 13 && !strncmp(line, PREFIX, sizeof(PREFIX) - 1)) {
      memcpy(dateString, p, 10);
      p += 10;
      headerValid = golfParseDate(dateString, date) && *p++ == ' ' && fieldNumber(p, regionsN, ' ') &&
                    fieldNumber(p, coursesN, 0) && regionsN > 0 && regionsN <= GOLF_CATALOG_MAX_REGIONS &&
                    coursesN > 0 && coursesN <= UINT16_MAX && !*p;
    }
    if (!headerValid) LOG_ERR("GOLF", "Bad catalog regions header");
    declaredRegions = regionsN;
    courseCount = coursesN;
    return;
  }
  if (!valid()) return;
  GolfCatalogRegion r{};
  uint32_t idx, countN, first;
  const char* p = line;
  bool ok = !overflow && length > 3 && line[2] == '|' && fieldNumber(p, idx, '|') && idx < declaredRegions;
  const char* name = p;
  while (*p && *p != '|') ++p;
  const size_t nameLen = p - name;
  ok = ok && *p == '|' && nameLen > 0 && nameLen < sizeof(r.name);
  if (*p) ++p;
  ok = ok && fieldNumber(p, countN, '|') && fieldNumber(p, first, 0) && !*p && countN > 0 &&
       countN <= GOLF_CATALOG_MAX_REGION_COURSES && first + countN <= courseCount;
  for (size_t i = 0; ok && i < nameLen; ++i) ok = name[i] >= 32 && name[i] < 127;
  for (uint8_t i = 0; ok && i < count; ++i)
    ok = regions[i].index != idx &&
         (first + countN <= regions[i].firstRecord || first >= regions[i].firstRecord + regions[i].count);
  if (!ok || count >= GOLF_CATALOG_MAX_REGIONS) {
    LOG_ERR("GOLF", "Skipped catalog region line %lu", static_cast<unsigned long>(lineNumber));
    return;
  }
  memcpy(r.name, name, nameLen);
  r.index = idx;
  r.count = countN;
  r.firstRecord = first;
  regions[count++] = r;
}
bool GolfCatalogRegionsReader::finish() {
  if (length || overflow) {
    acceptLine();
    length = 0;
    overflow = false;
  }
  return valid() && count > 0;
}
void GolfRecentReader::feedReverse(const char* bytes, size_t size, GolfRecentResolver resolver, void* user) {
  for (size_t i = size; i > 0 && count < 5; --i) {
    const char c = bytes[i - 1];
    if (!reverseBoundary) {
      if (c == '\n') reverseBoundary = true;
      continue;
    }
    if (c == '\n') {
      acceptNewestLine(resolver, user);
      length = 0;
      overflow = false;
    } else if (c != '\r') {
      if (length + 1 < sizeof(line))
        line[length++] = c;
      else
        overflow = true;
    }
  }
}
void GolfRecentReader::finishReverse(GolfRecentResolver resolver, void* user) {
  if (count < 5 && length) acceptNewestLine(resolver, user);
  length = 0;
  overflow = false;
}
void GolfRecentReader::acceptNewestLine(GolfRecentResolver resolver, void* user) {
  std::reverse(line, line + length);
  line[length] = 0;
  if (golfIndexHeaderVersion(line) != GolfIndexVersion::Unknown) return;
  if (overflow || !golfParseIndexRow(line, parsed)) {
    LOG_ERR("GOLF", "Skipped malformed Recent CSV row");
    return;
  }
  for (uint8_t i = 0; i < count; ++i)
    if (!strcmp(names[i], parsed.course)) return;
  if (resolver && !resolver(parsed.course, user)) return;
  memcpy(names[count++], parsed.course, sizeof(names[0]));
}
#endif
