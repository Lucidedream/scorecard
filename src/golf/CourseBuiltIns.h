#pragma once
#include "GolfCourse.h"
inline constexpr uint8_t SANYANG_BUILT_IN_INDEX = 0;
inline constexpr uint8_t MOGANSHAN_BUILT_IN_INDEX = 1;
inline constexpr GolfCourse GOLF_BUILT_IN_COURSES[] = {
    {"Sanyang Golf Club",
     2,
     2,
     {{"Out", {4, 5, 3, 4, 5, 3, 4, 4, 4}, {15, 7, 13, 11, 9, 17, 3, 1, 5}, true},
      {"In", {4, 4, 5, 3, 4, 4, 4, 3, 5}, {6, 12, 18, 16, 10, 14, 2, 4, 8}, true}},
     {{"Blue", true, {{325, 510, 144, 290, 510, 170, 427, 430, 390}, {400, 395, 520, 175, 365, 375, 385, 165, 490}}},
      {"White", true, {{310, 470, 122, 265, 490, 153, 389, 371, 340}, {360, 360, 495, 150, 332, 350, 370, 156, 470}}}}},
    {"MoganShan Gowin",
     2,
     1,
     {{"Out", {4, 3, 4, 4, 5, 3, 4, 5, 4}, {}, false}, {"In", {4, 5, 3, 4, 4, 4, 3, 4, 5}, {}, false}},
     {{"Blue", true, {{318, 137, 326, 403, 512, 168, 326, 530, 380}, {416, 502, 150, 285, 373, 388, 135, 352, 531}}}}},
    {"Pebble Beach",
     2,
     1,
     {{"Out", {4, 5, 4, 4, 3, 5, 3, 4, 4}, {}, false}, {"In", {4, 4, 3, 4, 5, 4, 4, 3, 5}, {}, false}},
     {{"Blue", false, {}}}},
    {"Template course",
     2,
     2,
     {{"Out", {0, 0, 0, 0, 0, 0, 0, 0, 0}, {}, false}, {"In", {0, 0, 0, 0, 0, 0, 0, 0, 0}, {}, false}},
     {{"Blue", false, {}}, {"White", false, {}}}},
};
inline constexpr uint8_t GOLF_BUILT_IN_COURSE_COUNT = sizeof(GOLF_BUILT_IN_COURSES) / sizeof(GOLF_BUILT_IN_COURSES[0]);
template <typename T>
constexpr uint16_t golfBuiltInSum(const T* values, uint8_t begin, uint8_t end) {
  uint16_t total = 0;
  for (uint8_t i = begin; i < end; ++i) total += values[i];
  return total;
}
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].nines[0].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].nines[1].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].nines[0].par, 0, 9) +
                  golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].nines[1].par, 0, 9) ==
              72);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[0].yards[0], 0, 9) == 3196);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[0].yards[1], 0, 9) == 3270);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[0].yards[0], 0, 9) +
                  golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[0].yards[1], 0, 9) ==
              6466);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[1].yards[0], 0, 9) == 2910);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[1].yards[1], 0, 9) == 3043);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[1].yards[0], 0, 9) +
                  golfBuiltInSum(GOLF_BUILT_IN_COURSES[0].tees[1].yards[1], 0, 9) ==
              5953);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].nines[0].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].nines[1].par, 0, 9) == 36);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].nines[0].par, 0, 9) +
                  golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].nines[1].par, 0, 9) ==
              72);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].tees[0].yards[0], 0, 9) == 3100);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].tees[0].yards[1], 0, 9) == 3132);
static_assert(golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].tees[0].yards[0], 0, 9) +
                  golfBuiltInSum(GOLF_BUILT_IN_COURSES[1].tees[0].yards[1], 0, 9) ==
              6232);
