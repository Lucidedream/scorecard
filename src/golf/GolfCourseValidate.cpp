#include "GolfCourseValidate.h"

#if defined(CROSSPOINT_GOLF)

namespace {

GolfCourseValidationResult validResult() {
  return {true, GolfCourseValidationError::None, GolfCourseField::None, 0, 0, 0};
}

GolfCourseValidationResult errorResult(GolfCourseValidationError error, GolfCourseField field, uint8_t hole,
                                       uint16_t value, uint16_t expected = 0) {
  return {false, error, field, hole, value, expected};
}

}  // namespace

GolfCourseValidationResult validateGolfCourse(const GolfCourse& course) {
  const uint16_t yardsLength = course.hasYards ? course.holeCount : 0;
  const uint16_t siLength = course.hasSi ? course.holeCount : 0;
  return validateGolfCourse(course, {course.holeCount, yardsLength, siLength});
}

GolfCourseValidationResult validateGolfCourse(const GolfCourse& course, const GolfCourseArrayLengths& lengths) {
  if (course.holeCount != 9 && course.holeCount != 18) {
    return errorResult(GolfCourseValidationError::HoleCount, GolfCourseField::None, 0, course.holeCount);
  }
  if (lengths.par != course.holeCount) {
    return errorResult(GolfCourseValidationError::ArrayLength, GolfCourseField::Par, 0, lengths.par, course.holeCount);
  }
  if (course.hasYards && lengths.yards != course.holeCount) {
    return errorResult(GolfCourseValidationError::ArrayLength, GolfCourseField::Yards, 0, lengths.yards,
                       course.holeCount);
  }
  if (course.hasSi && lengths.si != course.holeCount) {
    return errorResult(GolfCourseValidationError::ArrayLength, GolfCourseField::StrokeIndex, 0, lengths.si,
                       course.holeCount);
  }

  uint32_t seenStrokeIndexes = 0;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) {
    if (course.par[hole] < 3 || course.par[hole] > 6) {
      return errorResult(GolfCourseValidationError::ParRange, GolfCourseField::Par, hole, course.par[hole]);
    }
    if (course.hasYards) {
      if (course.yards[hole] < 1 || course.yards[hole] > 999) {
        return errorResult(GolfCourseValidationError::YardageRange, GolfCourseField::Yards, hole, course.yards[hole]);
      }
    } else if (course.yards[hole] != 0) {
      return errorResult(GolfCourseValidationError::UnexpectedOptionalData, GolfCourseField::Yards, hole,
                         course.yards[hole]);
    }
    if (course.hasSi) {
      const uint8_t strokeIndex = course.si[hole];
      if (strokeIndex < 1 || strokeIndex > course.holeCount) {
        return errorResult(GolfCourseValidationError::StrokeIndexRange, GolfCourseField::StrokeIndex, hole,
                           strokeIndex);
      }
      const uint32_t bit = 1UL << (strokeIndex - 1);
      if ((seenStrokeIndexes & bit) != 0) {
        return errorResult(GolfCourseValidationError::StrokeIndexDuplicate, GolfCourseField::StrokeIndex, hole,
                           strokeIndex);
      }
      seenStrokeIndexes |= bit;
    } else if (course.si[hole] != 0) {
      return errorResult(GolfCourseValidationError::UnexpectedOptionalData, GolfCourseField::StrokeIndex, hole,
                         course.si[hole]);
    }
  }

  for (uint8_t hole = course.holeCount; hole < GolfRound::MAX_HOLES; ++hole) {
    if (course.par[hole] != 0) {
      return errorResult(GolfCourseValidationError::TrailingEntry, GolfCourseField::Par, hole, course.par[hole]);
    }
    if (course.yards[hole] != 0) {
      return errorResult(GolfCourseValidationError::TrailingEntry, GolfCourseField::Yards, hole, course.yards[hole]);
    }
    if (course.si[hole] != 0) {
      return errorResult(GolfCourseValidationError::TrailingEntry, GolfCourseField::StrokeIndex, hole, course.si[hole]);
    }
  }
  return validResult();
}

const char* golfCourseValidationErrorName(GolfCourseValidationError error) {
  switch (error) {
    case GolfCourseValidationError::None:
      return "none";
    case GolfCourseValidationError::HoleCount:
      return "hole count must be 9 or 18";
    case GolfCourseValidationError::ArrayLength:
      return "array length does not match holes";
    case GolfCourseValidationError::ParRange:
      return "par must be 3 through 6";
    case GolfCourseValidationError::YardageRange:
      return "yardage must be 1 through 999";
    case GolfCourseValidationError::StrokeIndexRange:
      return "stroke index is out of range";
    case GolfCourseValidationError::StrokeIndexDuplicate:
      return "stroke index is duplicated";
    case GolfCourseValidationError::UnexpectedOptionalData:
      return "optional field contains data while absent";
    case GolfCourseValidationError::TrailingEntry:
      return "entry beyond hole count is nonzero";
  }
  return "unknown validation error";
}

const char* golfCourseFieldName(GolfCourseField field) {
  switch (field) {
    case GolfCourseField::None:
      return "course";
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
