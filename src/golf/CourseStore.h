#pragma once

#include <cstdint>
#include <cstring>

#include "GolfCourse.h"

inline constexpr uint8_t GOLF_MAX_COURSES = 32;
inline constexpr uint8_t GOLF_COURSE_FILENAME_BUFFER_SIZE = 48;

struct GolfCourseFile {
  char filename[GOLF_COURSE_FILENAME_BUFFER_SIZE];
  int8_t builtInIndex = -1;
};

struct GolfCourseListResult {
  uint8_t count;
  bool overflow;
};

class CourseStore {
 public:
  static GolfCourseListResult enumerate(GolfCourseFile* files, uint8_t capacity);
  static bool load(const char* filename, GolfCourse& course);
  static bool load(const GolfCourseFile& file, GolfCourse& course);
  static bool findByName(const char* courseName, GolfCourse& course);
  static void applyGolfCourse(const GolfCourse& course, GolfRound& round, uint16_t dateYmd) {
    round = {};
    memcpy(round.courseName, course.courseName, sizeof(round.courseName));
    memcpy(round.tees, course.tees, sizeof(round.tees));
    round.dateYmd = dateYmd;
    round.holeCount = course.holeCount;
    memcpy(round.par, course.par, sizeof(round.par));
    memcpy(round.yards, course.yards, sizeof(round.yards));
  }
};
