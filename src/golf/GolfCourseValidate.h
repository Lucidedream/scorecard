#pragma once

#include <cstdint>

#include "GolfCourse.h"

enum class GolfCourseField : uint8_t { None, Par, Yards, StrokeIndex };

enum class GolfCourseValidationError : uint8_t {
  None,
  HoleCount,
  ArrayLength,
  ParRange,
  YardageRange,
  StrokeIndexRange,
  StrokeIndexDuplicate,
  UnexpectedOptionalData,
  TrailingEntry,
};

struct GolfCourseArrayLengths {
  uint16_t par;
  uint16_t yards;
  uint16_t si;
};

struct GolfCourseValidationResult {
  bool valid;
  GolfCourseValidationError error;
  GolfCourseField field;
  uint8_t hole;
  uint16_t value;
  uint16_t expected;
};

GolfCourseValidationResult validateGolfCourse(const GolfCourse& course);
GolfCourseValidationResult validateGolfCourse(const GolfCourse& course, const GolfCourseArrayLengths& lengths);
const char* golfCourseValidationErrorName(GolfCourseValidationError error);
const char* golfCourseFieldName(GolfCourseField field);
