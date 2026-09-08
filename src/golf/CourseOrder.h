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

// CONTRACTS-V2 §34.4: the shared load body behind the new-round course picker
// (GolfSetupActivity) and the read-only course browser (GolfCourseMapListActivity). Given
// `count` already-enumerated-and-loaded (file, course) pairs, stable-insertion-sorts them
// in place by golfCourseSortsBefore(), then collapses each adjacent run of byte-identical
// courseName into one row: primaryIndex[0..return) receives each run's first (primary)
// index into the reordered arrays. Returns the deduped row count. `primaryIndex` must have
// room for `count` entries; `count` must not exceed GOLF_MAX_COURSES.
uint8_t golfSortAndDedupCourses(GolfCourseFile* files, GolfCourse* courses, uint8_t count, uint8_t* primaryIndex);

// The pure merge behind CourseStore::resolveAllTees(), factored out so it can be exercised
// with in-memory fixtures (no SD/HalStorage dependency): gathers every tee available for
// `courseName` across `count` already-loaded (file, course) pairs, e.g. the output of
// CourseStore::enumerate() + load(). The first matching entry supplies primary/primaryFile.
// Tees are appended in entry order, deduped by name, capped at GOLF_MAX_TEES; each is
// resolved via CourseStore::resolveTee() against the entry that named it. Returns false
// when no entry matches courseName.
bool golfResolveAllTeesFrom(const GolfCourseFile* files, const GolfCourse* courses, uint8_t count,
                            const char* courseName, GolfCourseTeeSet& result);

// CONTRACTS-V2 §32.7: the tee a fresh round pre-selects for player 1. "Blue" when the set has
// a tee named "Blue", else "White" when present, else the first tee's name, else "" for an
// empty set. Case-sensitive name match; does not gate on hasYards.
const char* golfDefaultTeeForSet(const GolfCourseTeeSet& teeSet);

#endif
