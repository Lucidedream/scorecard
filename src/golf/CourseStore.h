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

class CourseStore {
 public:
  static GolfCourseListResult enumerate(GolfCourseFile* files, uint8_t capacity);
  static bool load(const char* filename, GolfCourse& course);
  static bool load(const GolfCourseFile& file, GolfCourse& course);
  static bool findByName(const char* courseName, GolfCourse& course);
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
  static bool initializeGolfPlayerSelection(const GolfCourseFile& file, const GolfCourse& course,
                                            GolfRound& round) {
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
