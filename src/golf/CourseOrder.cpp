#include "CourseOrder.h"

#if defined(CROSSPOINT_GOLF)

#include <cctype>

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

#endif
