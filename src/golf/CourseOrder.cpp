#include "CourseOrder.h"

#if defined(CROSSPOINT_GOLF)

#include <Logging.h>

#include <cctype>
#include <cstring>
#include <initializer_list>

int golfCompareCourseNames(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    const int a = std::tolower(static_cast<unsigned char>(*left));
    const int b = std::tolower(static_cast<unsigned char>(*right));
    if (a != b) {
      return a - b;
    }
    ++left;
    ++right;
  }
  return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

void golfSortCourses(GolfCourseSummary* courses, uint8_t count) {
  for (uint8_t courseIndex = 1; courseIndex < count; ++courseIndex) {
    const GolfCourseSummary value = courses[courseIndex];
    uint8_t position = courseIndex;
    while (position > 0) {
      const auto& previous = courses[position - 1];
      const bool sortsBefore =
          value.file.builtInIndex >= 0
              ? (previous.file.builtInIndex < 0 || value.file.builtInIndex < previous.file.builtInIndex)
              : (previous.file.builtInIndex < 0 && golfCompareCourseNames(value.courseName, previous.courseName) < 0);
      if (!sortsBefore) {
        break;
      }
      courses[position] = previous;
      --position;
    }
    courses[position] = value;
  }
}

namespace {

// CONTRACTS-V2 §35 name-priority list: the golf-conventional tee order. A name matching
// none of these sorts after every listed name.
int golfTeeNamePriorityIndex(const char* name) {
  static constexpr const char* kPriority[] = {"Black", "Gold", "Blue", "White", "Green", "Yellow", "Red", "Orange"};
  for (int i = 0; i < static_cast<int>(sizeof(kPriority) / sizeof(kPriority[0])); ++i) {
    if (golfCompareCourseNames(name, kPriority[i]) == 0) {
      return i;
    }
  }
  return 100;
}

uint32_t golfTeeTotalYards(const GolfCourseTee& tee) {
  uint32_t total = 0;
  for (const auto& nine : tee.yards) {
    for (uint16_t yards : nine) {
      total += yards;
    }
  }
  return total;
}

}  // namespace

bool golfTeeSortsBefore(const GolfCourseTee& a, const GolfCourseTee& b) {
  if (a.hasYards != b.hasYards) {
    return a.hasYards;  // Yardless tees sink below tees with yardage.
  }
  if (a.hasYards) {
    const uint32_t aTotal = golfTeeTotalYards(a);
    const uint32_t bTotal = golfTeeTotalYards(b);
    if (aTotal != bTotal) {
      return aTotal > bTotal;  // Longer course first.
    }
  }
  const int aPriority = golfTeeNamePriorityIndex(a.name);
  const int bPriority = golfTeeNamePriorityIndex(b.name);
  if (aPriority != bPriority) {
    return aPriority < bPriority;
  }
  return strcmp(a.name, b.name) < 0;
}

const char* golfDefaultTee(const GolfCourse& course) {
  for (const char* name : {"Blue", "White"}) {
    for (uint8_t teeIndex = 0; teeIndex < course.teeCount; ++teeIndex) {
      if (strcmp(course.tees[teeIndex].name, name) == 0) {
        return course.tees[teeIndex].name;
      }
    }
  }
  return course.teeCount ? course.tees[0].name : "";
}

GolfNineSelection golfDefaultNineSelection(const GolfCourse& course) {
  return golfDefaultNineSelection(course.nineCount);
}

bool golfNineSelectionValid(const GolfCourse& course, GolfNineSelection selection) {
  return course.nineCount >= 1 && course.nineCount <= GOLF_MAX_NINES && selection.front < course.nineCount &&
         (course.nineCount == 1 ? selection.back == GOLF_NO_NINE
                                : selection.back < course.nineCount && selection.back != selection.front);
}

bool golfComposeRound(const GolfCourse& course, GolfNineSelection selection, uint16_t dateYmd, GolfRound& round) {
  if (!golfNineSelectionValid(course, selection)) {
    return false;
  }
  round = {};
  initializeGolfPlayerDefaults(round);
  memcpy(round.courseName, course.courseName, sizeof(round.courseName));
  round.dateYmd = dateYmd;
  round.holeCount = selection.back == GOLF_NO_NINE ? 9 : 18;
  round.nines[0] = selection.front;
  round.nines[1] = selection.back;
  round.hasSi = true;
  uint32_t seen = 0;
  for (uint8_t hole = 0; hole < round.holeCount; ++hole) {
    const GolfNine& nine = course.nines[round.nines[hole / GOLF_HOLES_PER_NINE]];
    round.par[hole] = nine.par[hole % GOLF_HOLES_PER_NINE];
    round.si[hole] = nine.si[hole % GOLF_HOLES_PER_NINE];
    const uint8_t strokeIndex = round.si[hole];
    if (!nine.hasSi || strokeIndex == 0 || strokeIndex > round.holeCount || (seen & (1UL << strokeIndex))) {
      round.hasSi = false;
    }
    if (strokeIndex > 0 && strokeIndex <= round.holeCount) {
      seen |= 1UL << strokeIndex;
    }
  }
  if (!round.hasSi) {
    memset(round.si, 0, sizeof(round.si));
    LOG_INF("GOLF", "Composed nines have no canonical stroke index");
  }
  return true;
}

bool golfComposeTeeYards(const GolfCourse& course, uint8_t teeIndex, GolfNineSelection selection,
                         uint16_t (&yards)[GOLF_MAX_HOLES]) {
  if (!golfNineSelectionValid(course, selection) || teeIndex >= course.teeCount || !course.tees[teeIndex].hasYards) {
    return false;
  }
  memset(yards, 0, sizeof(yards));
  memcpy(yards, course.tees[teeIndex].yards[selection.front], 9 * sizeof(uint16_t));
  if (selection.back != GOLF_NO_NINE) {
    memcpy(yards + 9, course.tees[teeIndex].yards[selection.back], 9 * sizeof(uint16_t));
  }
  return true;
}

#endif
