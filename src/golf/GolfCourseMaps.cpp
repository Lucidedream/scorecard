#include "GolfPaths.h"
#if defined(CROSSPOINT_GOLF)
#include <cstring>

#include "GolfCourse.h"
bool golfCourseMapsDir(const GolfCourse& course, char* output, size_t size) {
  if (!course.mapsDir[0]) return golfSlug(course.courseName, output, size);
  if (!output || size <= strlen(course.mapsDir)) return false;
  memcpy(output, course.mapsDir, strlen(course.mapsDir) + 1);
  return true;
}
#endif
