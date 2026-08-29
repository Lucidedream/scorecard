#pragma once

#if defined(CROSSPOINT_GOLF)

#include "CourseStore.h"
#include "GolfCourse.h"

// Case-insensitive, strcmp-style comparison of two course display names.
int golfCompareCourseNames(const char* left, const char* right);

// CONTRACTS-V2 §7 / §7.1 course list ordering, as a pure predicate:
//   * Built-in courses sort first, in fixed table order.
//   * An SD file that overrides a built-in inherits that built-in's slot; its
//     builtInIndex is carried through by CourseStore::enumerate(), so it compares
//     equal to the built-in it replaces.
//   * SD courses that override nothing sort alphabetically (case-insensitive) after
//     every built-in.
bool golfCourseSortsBefore(const GolfCourseFile& lhsFile, const GolfCourse& lhs, const GolfCourseFile& rhsFile,
                           const GolfCourse& rhs);

#endif
