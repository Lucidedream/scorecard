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

uint8_t golfSortAndDedupCourses(GolfCourseFile* files, GolfCourse* courses, const uint8_t count,
                                uint8_t* primaryIndex) {
  for (uint8_t i = 1; i < count; ++i) {
    const GolfCourse value = courses[i];
    const GolfCourseFile valueFile = files[i];
    uint8_t position = i;
    while (position > 0 && golfCourseSortsBefore(valueFile, value, files[position - 1], courses[position - 1])) {
      courses[position] = courses[position - 1];
      files[position] = files[position - 1];
      --position;
    }
    courses[position] = value;
    files[position] = valueFile;
  }

  // Course names sharing a comparator equivalence class land contiguously after the sort
  // above; collapse each run into a single row keyed by its first (primary) entry.
  uint8_t courseCount = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (courseCount > 0 && strcmp(courses[i].courseName, courses[primaryIndex[courseCount - 1]].courseName) == 0) {
      continue;
    }
    primaryIndex[courseCount++] = i;
  }
  return courseCount;
}

namespace {

// CONTRACTS-V2 §35 name-priority list: the golf-conventional tee order. A name matching
// none of these sorts after every listed name.
int golfTeeNamePriorityIndex(const char* name) {
  static constexpr const char* kPriority[] = {"Black", "Gold", "Blue", "White", "Green", "Yellow", "Red", "Orange"};
  for (int i = 0; i < static_cast<int>(sizeof(kPriority) / sizeof(kPriority[0])); ++i) {
    if (golfCompareCourseNames(name, kPriority[i]) == 0) return i;
  }
  return 100;
}

// Total yardage across the fixed 18-slot array. A 9-hole tee has its back nine zeroed,
// so the sums still order correctly.
uint32_t golfTeeTotalYards(const GolfCourseTee& tee) {
  uint32_t total = 0;
  for (uint8_t hole = 0; hole < GolfRound::MAX_HOLES; ++hole) total += tee.yards[hole];
  return total;
}

// Appends one tee to the set: skips empty names, names already present, and anything past
// GOLF_MAX_TEES. The resolution (and any yardage) is copied by value from `course`, which
// the caller may hold in scratch memory that does not outlive this call.
void appendTee(GolfCourseTeeSet& result, const GolfCourseFile& file, const GolfCourse& course, const char* name) {
  if (name == nullptr || name[0] == '\0' || result.teeCount >= GOLF_MAX_TEES) return;
  for (uint8_t i = 0; i < result.teeCount; ++i) {
    if (strcmp(result.tees[i].name, name) == 0) return;
  }
  GolfCourseTee& slot = result.tees[result.teeCount];
  slot = {};
  golfSetTeeString(slot.name, name);
  GolfTeeResolution resolved{};
  slot.resolved = CourseStore::resolveTee(file, course, name, resolved);
  slot.hasYards = resolved.hasYards;
  if (resolved.hasYards) memcpy(slot.yards, resolved.yards, sizeof(slot.yards));
  ++result.teeCount;
}

}  // namespace

bool golfTeeSortsBefore(const GolfCourseTee& a, const GolfCourseTee& b) {
  if (a.hasYards != b.hasYards) return a.hasYards;  // yardless tees sink below tees with yardage
  if (a.hasYards) {
    const uint32_t aTotal = golfTeeTotalYards(a);
    const uint32_t bTotal = golfTeeTotalYards(b);
    if (aTotal != bTotal) return aTotal > bTotal;  // longer course first
  }
  const int aPriority = golfTeeNamePriorityIndex(a.name);
  const int bPriority = golfTeeNamePriorityIndex(b.name);
  if (aPriority != bPriority) return aPriority < bPriority;
  return strcmp(a.name, b.name) < 0;
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
    // An unmodified built-in contributes its flash alternates (Sanyang Blue/White) ahead
    // of its own label; an SD file (even one overriding a built-in slot) contributes only
    // the tee its `tees` string names.
    if (files[index].filename[0] == '\0') {
      const char* const* names = nullptr;
      uint8_t nameCount = 0;
      golfBuiltInTeeNames(files[index].builtInIndex, names, nameCount);
      for (uint8_t n = 0; n < nameCount; ++n) appendTee(result, files[index], courses[index], names[n]);
    }
    appendTee(result, files[index], courses[index], courses[index].tees);
  }

  // A label-less course carries no tee names of its own; keep the historical Blue/White
  // selection-only choices so such a course is still playable (CONTRACTS-V2 §32.2).
  if (found && result.teeCount == 0) {
    GolfTeeResolution probe{};
    if (CourseStore::resolveTee(result.primaryFile, result.primary, "Blue", probe)) {
      appendTee(result, result.primaryFile, result.primary, "Blue");
    }
    if (CourseStore::resolveTee(result.primaryFile, result.primary, "White", probe)) {
      appendTee(result, result.primaryFile, result.primary, "White");
    }
  }

  // CONTRACTS-V2 §35: reorder the gathered set longest-first so every consumer that
  // iterates it renders in golf-conventional order without re-sorting. Stable insertion
  // sort -- teeCount <= GOLF_MAX_TEES (6).
  for (uint8_t i = 1; i < result.teeCount; ++i) {
    const GolfCourseTee value = result.tees[i];
    uint8_t position = i;
    while (position > 0 && golfTeeSortsBefore(value, result.tees[position - 1])) {
      result.tees[position] = result.tees[position - 1];
      --position;
    }
    result.tees[position] = value;
  }
  return found;
}

const char* golfDefaultTeeForSet(const GolfCourseTeeSet& teeSet) {
  for (uint8_t i = 0; i < teeSet.teeCount; ++i) {
    if (strcmp(teeSet.tees[i].name, "Blue") == 0) return teeSet.tees[i].name;
  }
  for (uint8_t i = 0; i < teeSet.teeCount; ++i) {
    if (strcmp(teeSet.tees[i].name, "White") == 0) return teeSet.tees[i].name;
  }
  return teeSet.teeCount > 0 ? teeSet.tees[0].name : "";
}

#endif
