#pragma once
#if defined(CROSSPOINT_GOLF)
#include "CourseStore.h"
int golfCompareCourseNames(const char* left, const char* right);
void golfSortCourses(GolfCourseSummary* courses, uint8_t count);
bool golfTeeSortsBefore(const GolfCourseTee& a, const GolfCourseTee& b);
const char* golfDefaultTee(const GolfCourse& course);
constexpr GolfNineSelection golfDefaultNineSelection(const uint8_t nineCount) {
  return {0, nineCount >= 2 ? uint8_t{1} : GOLF_NO_NINE};
}
GolfNineSelection golfDefaultNineSelection(const GolfCourse& course);
bool golfNineSelectionValid(const GolfCourse& course, GolfNineSelection selection);
bool golfComposeRound(const GolfCourse& course, GolfNineSelection selection, uint16_t dateYmd, GolfRound& round);
bool golfComposeTeeYards(const GolfCourse& course, uint8_t teeIndex, GolfNineSelection selection,
                         uint16_t (&yards)[GOLF_MAX_HOLES]);
#endif
