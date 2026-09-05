#include "CourseOrder.h"

#if defined(CROSSPOINT_GOLF)

#include <cctype>
#include <cstring>

int golfCompareCourseNames(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    const int a = std::tolower(static_cast<unsigned char>(*left));
    const int b = std::tolower(static_cast<unsigned char>(*right));
    if (a != b) return a - b;
    ++left;
    ++right;
  }
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

bool golfCourseSortsBefore(const GolfCourseFile& lhsFile, const GolfCourse& lhs, const GolfCourseFile& rhsFile,
                           const GolfCourse& rhs) {
  const bool lhsBuiltIn = lhsFile.builtInIndex >= 0;
  const bool rhsBuiltIn = rhsFile.builtInIndex >= 0;
  if (lhsBuiltIn != rhsBuiltIn) return lhsBuiltIn;
  if (lhsBuiltIn) return lhsFile.builtInIndex < rhsFile.builtInIndex;
  return golfCompareCourseNames(lhs.courseName, rhs.courseName) < 0;
}

bool golfResolveAllTeesFrom(const GolfCourseFile* files, const GolfCourse* courses, const uint8_t count,
                            const char* courseName, GolfCourseTeeSet& result) {
  result = {};
  if (courseName == nullptr || courseName[0] == '\0') return false;

  bool found = false;
  for (uint8_t index = 0; index < count; ++index) {
    if (strcmp(courses[index].courseName, courseName) != 0) continue;
    if (!found) {
      result.primaryFile = files[index];
      result.primary = courses[index];
      found = true;
    }
    GolfTeeResolution resolved{};
    if (!result.hasBlue && CourseStore::resolveTee(files[index], courses[index], TeeSelection::Blue, resolved)) {
      result.blue.hasYards = resolved.hasYards;
      if (resolved.hasYards) memcpy(result.blue.yards, resolved.yards, sizeof(result.blue.yards));
      result.hasBlue = true;
    }
    resolved = {};
    if (!result.hasWhite && CourseStore::resolveTee(files[index], courses[index], TeeSelection::White, resolved)) {
      result.white.hasYards = resolved.hasYards;
      if (resolved.hasYards) memcpy(result.white.yards, resolved.yards, sizeof(result.white.yards));
      result.hasWhite = true;
    }
  }
  return found;
}

#endif
