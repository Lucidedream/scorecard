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

// The pure merge behind CourseStore::resolveAllTees(), factored out so it can be exercised
// with in-memory fixtures (no SD/HalStorage dependency): merges every tee available for
// `courseName` across `count` already-loaded (file, course) pairs, e.g. the output of
// CourseStore::enumerate() + load(). The first matching entry supplies primary/primaryFile;
// Blue and White are each resolved independently via CourseStore::resolveTee() against
// whichever entry actually carries that tee. Returns false when no entry matches courseName.
bool golfResolveAllTeesFrom(const GolfCourseFile* files, const GolfCourse* courses, uint8_t count,
                            const char* courseName, GolfCourseTeeSet& result);

#endif
