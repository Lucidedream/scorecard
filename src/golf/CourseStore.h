#pragma once

#include <cstdint>
#include <cstring>

#include "CourseBuiltIns.h"
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

// One tee's yardage, copied by value: GolfTeeResolution::yards points into whichever
// GolfCourse supplied it, which for resolveAllTees() is a scratch array that does not
// outlive the call, so it cannot be borrowed by pointer here.
struct GolfCourseTeeYardage {
  uint16_t yards[GolfRound::MAX_HOLES]{};
  bool hasYards = false;
};

// Every tee available for one course name, gathered across every enumerated course file.
// Two tees of the same course are commonly two separate files with the same courseName
// (CONTRACTS-V2 §30), so blue/white may each come from a different file entirely.
struct GolfCourseTeeSet {
  GolfCourseFile primaryFile{};  // whichever file supplied holeCount/par/si
  GolfCourse primary{};
  GolfCourseTeeYardage blue{};
  GolfCourseTeeYardage white{};
  bool hasBlue = false;
  bool hasWhite = false;
};

class CourseStore {
 public:
  static GolfCourseListResult enumerate(GolfCourseFile* files, uint8_t capacity);
  static bool load(const char* filename, GolfCourse& course);
  static bool load(const GolfCourseFile& file, GolfCourse& course);
  static bool findByName(const char* courseName, GolfCourse& course);
  // Scans every enumerated course file (built-ins + SD) for courseName, resolving Blue and
  // White independently against whichever file(s) actually carry that tee. `result.primary`
  // comes from the first matching file (holeCount/par/si should agree across a course's tee
  // files; if they don't, this trusts the primary rather than reconciling). Returns false
  // when no file matches courseName at all.
  static bool resolveAllTees(const char* courseName, GolfCourseTeeSet& result);
  static bool resolveTee(const GolfCourseFile& file, const GolfCourse& course, TeeSelection tee,
                         GolfTeeResolution& resolved) {
    // An override's builtInIndex is ordering metadata, not permission to borrow
    // flash-resident alternate tee rows from the course it replaced.
    return golfResolveTeeCourse(course, file.builtInIndex, file.filename[0] == '\0', tee, resolved);
  }
  static TeeSelection defaultTee(const GolfCourseFile& file, const GolfCourse& course) {
    GolfTeeResolution resolved{};
    if (resolveTee(file, course, TeeSelection::Blue, resolved)) return TeeSelection::Blue;
    if (resolveTee(file, course, TeeSelection::White, resolved)) return TeeSelection::White;
    return TeeSelection::NotPlay;
  }
  static bool initializeGolfPlayerSelection(const GolfCourseFile& file, const GolfCourse& course, GolfRound& round) {
    for (GolfPlayer& player : round.players) player.tee = TeeSelection::NotPlay;
    round.players[0].tee = defaultTee(file, course);
    return golfPlayerIsEnabled(round.players[0]);
  }
  static void applyGolfCourse(const GolfCourse& course, GolfRound& round, uint16_t dateYmd) {
    round = {};
    initializeGolfPlayerDefaults(round);
    memcpy(round.courseName, course.courseName, sizeof(round.courseName));
    round.dateYmd = dateYmd;
    round.holeCount = course.holeCount;
    memcpy(round.par, course.par, sizeof(round.par));
    round.hasSi = course.hasSi;
    if (round.hasSi) memcpy(round.si, course.si, sizeof(round.si));
  }
};
