#pragma once

#include <cstdint>
#include <cstring>

#include "CourseBuiltIns.h"
#include "GolfCourse.h"

inline constexpr uint8_t GOLF_MAX_COURSES = 32;
inline constexpr uint8_t GOLF_COURSE_FILENAME_BUFFER_SIZE = 48;

enum class GolfCourseSource : uint8_t { BuiltIn, Owner, Catalog };

struct GolfCourseFile {
  char filename[GOLF_COURSE_FILENAME_BUFFER_SIZE];
  int8_t builtInIndex = -1;
  GolfCourseSource source = GolfCourseSource::Owner;
};

struct GolfCourseListResult {
  uint8_t count;
  bool overflow;
};

struct GolfCourseSummary {
  GolfCourseFile file;
  char courseName[40];
  uint8_t nineCount;
  uint16_t defaultPar;
  char teeList[24];
};
struct GolfCourseCatalogInfo;
class CourseStore {
 public:
  static GolfCourseListResult enumerate(GolfCourseFile* files, uint8_t capacity);
  static bool load(const char* filename, GolfCourse& course);
  static bool load(const GolfCourseFile& file, GolfCourse& course);
  static bool loadCatalogInfo(const GolfCourseFile& file, GolfCourseCatalogInfo& info);
  static bool findByName(const char* name, GolfCourseFile& file, GolfCourse& course);
};
