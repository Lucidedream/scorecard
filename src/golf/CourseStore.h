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

inline constexpr uint8_t GOLF_MAX_TEES = 6;

// One resolved tee of a course. `yards` is copied by value: the resolver borrows it from
// whichever GolfCourse supplied the row, which for resolveAllTees() is a scratch array
// that does not outlive the call, so a pointer cannot be kept here.
struct GolfCourseTee {
  char name[GOLF_TEE_CAPACITY]{};
  bool resolved = false;  // CourseStore::resolveTee() accepted this name (selection is valid)
  bool hasYards = false;
  uint16_t yards[GolfRound::MAX_HOLES]{};
};

// Every tee available for one course name, gathered across every enumerated course file,
// in file order and deduped by tee name (CONTRACTS-V2 §32.2). Two tees of the same course
// are commonly two separate files sharing the courseName (CONTRACTS-V2 §30), so each entry
// may come from a different file.
struct GolfCourseTeeSet {
  GolfCourseFile primaryFile{};  // whichever file supplied holeCount/par/si
  GolfCourse primary{};
  GolfCourseTee tees[GOLF_MAX_TEES]{};
  uint8_t teeCount = 0;
};

class CourseStore {
 public:
  static GolfCourseListResult enumerate(GolfCourseFile* files, uint8_t capacity);
  static bool load(const char* filename, GolfCourse& course);
  static bool load(const GolfCourseFile& file, GolfCourse& course);
  static bool findByName(const char* courseName, GolfCourse& course);
  // Scans every enumerated course file (built-ins + SD) for courseName and gathers every
  // tee it carries into `result.tees` (up to GOLF_MAX_TEES), in file order, deduped by tee
  // name. `result.primary` comes from the first matching file (holeCount/par/si should
  // agree across a course's tee files; if they don't, this trusts the primary rather than
  // reconciling). Returns false when no file matches courseName at all.
  static bool resolveAllTees(const char* courseName, GolfCourseTeeSet& result);
  // Matches `tee` case-sensitively against the file's `tees` string (and the built-in
  // flash alternates when this is an unmodified built-in). CONTRACTS-V2 §32.2.
  static bool resolveTee(const GolfCourseFile& file, const GolfCourse& course, const char* tee,
                         GolfTeeResolution& resolved) {
    // An override's builtInIndex is ordering metadata, not permission to borrow
    // flash-resident alternate tee rows from the course it replaced.
    return golfResolveTeeCourse(course, file.builtInIndex, file.filename[0] == '\0', tee, resolved);
  }
  // The first resolvable tee of this course file, in course-list order (CONTRACTS-V2
  // §32.2): built-in flash alternates first, then the file's own `tees` label, then the
  // historical Blue/White selection-only fallback for a label-less course.
  static const char* defaultTee(const GolfCourseFile& file, const GolfCourse& course) {
    GolfTeeResolution resolved{};
    if (file.filename[0] == '\0') {
      const char* const* names = nullptr;
      uint8_t nameCount = 0;
      golfBuiltInTeeNames(file.builtInIndex, names, nameCount);
      for (uint8_t i = 0; i < nameCount; ++i) {
        if (resolveTee(file, course, names[i], resolved)) return names[i];
      }
    }
    if (course.tees[0] != '\0' && resolveTee(file, course, course.tees, resolved)) return course.tees;
    if (resolveTee(file, course, "Blue", resolved)) return "Blue";
    if (resolveTee(file, course, "White", resolved)) return "White";
    return "";
  }
  static bool initializeGolfPlayerSelection(const GolfCourseFile& file, const GolfCourse& course, GolfRound& round) {
    for (GolfPlayer& player : round.players) player.tee[0] = '\0';
    golfSetTee(round.players[0], defaultTee(file, course));
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
