#include "GolfCourseValidate.h"
#if defined(CROSSPOINT_GOLF)
#include <cstring>

#include "GolfRoundDecode.h"
namespace {
bool nameValid(const char* name, size_t capacity, bool comma = false) {
  return name[0] && memchr(name, 0, capacity) && !strpbrk(name, comma ? ",\r\n" : "\r\n");
}
}  // namespace
GolfCourseValidationResult validateGolfCourse(const GolfCourse& c) {
  using E = GolfCourseValidationError;
  using F = GolfCourseField;
  if (!nameValid(c.courseName, sizeof(c.courseName))) return {false, E::Name, F::Name};
  if (c.nineCount < 1 || c.nineCount > GOLF_MAX_NINES) return {false, E::NineCount, F::Nines};
  if (c.teeCount < 1 || c.teeCount > GOLF_MAX_TEES) return {false, E::TeeCount, F::Tees};
  for (uint8_t n = 0; n < c.nineCount; ++n) {
    const auto& nine = c.nines[n];
    if (!nameValid(nine.name, sizeof(nine.name), true)) return {false, E::Name, F::NineName};
    for (uint8_t p = 0; p < n; ++p)
      if (strcmp(nine.name, c.nines[p].name) == 0) return {false, E::Duplicate, F::NineName};
    uint32_t seen = 0;
    for (uint8_t h = 0; h < 9; ++h) {
      if (nine.par[h] < 3 || nine.par[h] > 6) return {false, E::ParRange, F::Par, h, nine.par[h]};
      if (nine.hasSi) {
        const uint8_t si = nine.si[h];
        if (si < 1 || si > (c.nineCount == 1 ? 9 : 18)) return {false, E::StrokeIndexRange, F::StrokeIndex, h, si};
        if (seen & (1UL << si)) return {false, E::StrokeIndexDuplicate, F::StrokeIndex, h, si};
        seen |= 1UL << si;
      } else if (nine.si[h])
        return {false, E::UnexpectedOptionalData, F::StrokeIndex, h, nine.si[h]};
    }
  }
  for (uint8_t t = 0; t < c.teeCount; ++t) {
    const auto& tee = c.tees[t];
    if (!memchr(tee.name, 0, sizeof(tee.name)) || !golfTeeStringValid(tee.name)) return {false, E::Name, F::TeeName};
    for (uint8_t p = 0; p < t; ++p)
      if (strcmp(tee.name, c.tees[p].name) == 0) return {false, E::Duplicate, F::TeeName};
    for (uint8_t n = 0; n < c.nineCount; ++n)
      for (uint8_t h = 0; h < 9; ++h) {
        uint16_t y = tee.yards[n][h];
        if (tee.hasYards && (y < 1 || y > 999)) return {false, E::YardageRange, F::Yards, h, y};
        if (!tee.hasYards && y) return {false, E::UnexpectedOptionalData, F::Yards, h, y};
      }
  }
  return {true, E::None, F::None};
}
const char* golfCourseValidationErrorName(GolfCourseValidationError e) {
  switch (e) {
    case GolfCourseValidationError::None:
      return "none";
    case GolfCourseValidationError::Version:
      return "course version must be 2";
    case GolfCourseValidationError::Name:
      return "invalid name";
    case GolfCourseValidationError::NineCount:
      return "nine count must be 1 through 4";
    case GolfCourseValidationError::TeeCount:
      return "tee count must be at most 6";
    case GolfCourseValidationError::Duplicate:
      return "duplicate name";
    case GolfCourseValidationError::ArrayLength:
      return "invalid array length or value type";
    case GolfCourseValidationError::ParRange:
      return "par must be 3 through 6";
    case GolfCourseValidationError::YardageRange:
      return "yardage must be 1 through 999";
    case GolfCourseValidationError::StrokeIndexRange:
      return "stroke index out of range";
    case GolfCourseValidationError::StrokeIndexDuplicate:
      return "duplicate stroke index";
    case GolfCourseValidationError::UnexpectedOptionalData:
      return "data in absent field";
  }
  return "unknown";
}
const char* golfCourseFieldName(GolfCourseField f) {
  switch (f) {
    case GolfCourseField::None:
      return "course";
    case GolfCourseField::Name:
      return "name";
    case GolfCourseField::Nines:
      return "nines";
    case GolfCourseField::NineName:
      return "nines.name";
    case GolfCourseField::Tees:
      return "tees";
    case GolfCourseField::TeeName:
      return "tees.name";
    case GolfCourseField::Par:
      return "par";
    case GolfCourseField::Yards:
      return "yards";
    case GolfCourseField::StrokeIndex:
      return "si";
  }
  return "course";
}
#endif
