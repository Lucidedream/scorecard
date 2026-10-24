#pragma once
#include "GolfCourse.h"
enum class GolfCourseField : uint8_t { None, Maps, Name, Nines, NineName, Tees, TeeName, Par, Yards, StrokeIndex };
enum class GolfCourseValidationError : uint8_t {
  None,
  Version,
  Name,
  NineCount,
  TeeCount,
  Duplicate,
  ArrayLength,
  ParRange,
  YardageRange,
  StrokeIndexRange,
  StrokeIndexDuplicate,
  UnexpectedOptionalData
};
struct GolfCourseValidationResult {
  bool valid;
  GolfCourseValidationError error;
  GolfCourseField field;
  uint8_t hole = 0;
  uint16_t value = 0;
  uint16_t expected = 0;
};
GolfCourseValidationResult validateGolfCourse(const GolfCourse& course);
const char* golfCourseValidationErrorName(GolfCourseValidationError error);
const char* golfCourseFieldName(GolfCourseField field);
