#include "GolfCourseCatalogInfo.h"
#if defined(CROSSPOINT_GOLF)
#include <cstring>
namespace {
bool optionalNumber(JsonVariantConst value, uint32_t max, uint32_t& out, bool& present, uint32_t min = 1) {
  present = !value.isUnbound();
  if (!present) return true;
  if (!value.is<uint32_t>()) return false;
  out = value.as<uint32_t>();
  return out >= min && out <= max;
}
bool optionalText(JsonVariantConst value, char* out, size_t size, bool& present) {
  present = !value.isUnbound();
  if (!present) return true;
  if (!value.is<const char*>()) return false;
  const char* text = value.as<const char*>();
  const size_t len = strlen(text);
  if (!len || len >= size) return false;
  for (size_t i = 0; i < len; ++i)
    if (text[i] < 32 || text[i] >= 127) return false;
  memcpy(out, text, len + 1);
  return true;
}
}  // namespace
bool golfParseCatalogInfo(JsonVariantConst json, GolfCourseCatalogInfo& info) {
  info = {};
  const auto b = json["baigolf"];
  if (!b.is<JsonObjectConst>() || !b["id"].is<uint32_t>() || !b["id"].as<uint32_t>()) return false;
  GolfCourseCatalogInfo parsed{};
  parsed.id = b["id"].as<uint32_t>();
  uint32_t n = 0;
  if (!optionalNumber(b["rating"], 50, n, parsed.hasRating, 0)) return false;
  parsed.rating = n;
  if (!optionalNumber(b["reviews"], UINT32_MAX, parsed.reviews, parsed.hasReviews) ||
      parsed.hasRating != parsed.hasReviews)
    return false;
  if (!optionalNumber(b["holes"], UINT16_MAX, n, parsed.hasHoles)) return false;
  parsed.holes = parsed.hasHoles ? n : 0;
  if (!optionalNumber(b["par"], UINT16_MAX, n, parsed.hasPar)) return false;
  parsed.par = parsed.hasPar ? n : 0;
  if (!optionalNumber(b["yards"], UINT32_MAX, parsed.yards, parsed.hasYards)) return false;
  if (!optionalNumber(b["opened"], 9999, n, parsed.hasOpened, 1000)) return false;
  parsed.opened = parsed.hasOpened ? n : 0;
  if (!optionalText(b["region"], parsed.region, sizeof(parsed.region), parsed.hasRegion) ||
      !optionalText(b["designer"], parsed.designer, sizeof(parsed.designer), parsed.hasDesigner))
    return false;
  parsed.hasScores = !b["scores"].isUnbound();
  if (parsed.hasScores) {
    const auto scores = b["scores"].as<JsonArrayConst>();
    if (scores.isNull() || scores.size() != 6) return false;
    for (uint8_t i = 0; i < 6; ++i) {
      if (!scores[i].is<uint8_t>() || scores[i].as<uint8_t>() > 50) return false;
      parsed.scores[i] = scores[i].as<uint8_t>();
    }
  }
  info = parsed;
  return true;
}
#endif
