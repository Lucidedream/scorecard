#pragma once

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUICore.h>

#include <cstddef>
#include <cstdint>

#include "activities/Activity.h"
#include "golf/CourseStore.h"

// Read-only tee-info table for the course-map browser (CONTRACTS-V2 §34.2):
// opened with just a course name from GolfCourseMapBrowserActivity, it resolves
// every tee of the course once via CourseStore::resolveAllTees() and lays out
// par, stroke index (only when the course carries it) and each tee's per-hole
// yardage, with the nine's totals in an OUT/IN column. An 18-hole course splits
// Front/Back across two tabs driven by the rocker; a 9-hole course shows one
// table. No GolfRound is ever in scope; nothing here can start or mutate a round.
class GolfCourseTeeInfoActivity final : public Activity {
 public:
  GolfCourseTeeInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* courseName);

  void onEnter() override;
  void loop() override;

 private:
  static constexpr uint8_t DATA_COLS = 10;                    // 9 holes + the nine's total
  static constexpr uint8_t MAX_ROWS = 1 + 2 + GOLF_MAX_TEES;  // hole header + Par + SI + up to 6 tees
  static constexpr uint8_t CELL_CAP = 8;
  static constexpr int16_t LABEL_COLUMN_UNITS = 3;

  char courseName[40]{};
  GolfCourseTeeSet teeSet{};
  bool resolved = false;
  uint8_t holeCount = 0;
  uint8_t activeTab = 0;
  bool firstPaint = true;

  // Row model, rebuilt for the active nine on each paint. Row 0 is the hole
  // header; rowLabels[row] is the left label column.
  char dataCells[MAX_ROWS][DATA_COLS][CELL_CAP]{};
  const char* rowLabels[MAX_ROWS]{};
  uint8_t rowCount = 0;

  void changeTab(int delta);
  void buildRows(uint8_t firstHole);
  void drawTabs(freeink::ui::Rect rect) const;
  void drawTable(freeink::ui::Rect rect) const;
  void drawFooter() const;
  void renderScreen();
};

#endif
