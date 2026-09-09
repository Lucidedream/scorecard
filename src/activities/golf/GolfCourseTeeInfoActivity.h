#pragma once

#if defined(CROSSPOINT_GOLF)

#include <cstdint>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/CourseStore.h"

// Read-only tee-info table for the course-map browser (CONTRACTS-V2 §34.2 / §35.1):
// opened with just a course name from GolfCourseMapBrowserActivity, it resolves every
// tee of the course once via CourseStore::resolveAllTees() (already longest-first, §35)
// and lays out par, stroke index (only when the course carries it) and each tee's
// per-hole yardage, with the nine's total in an OUT/IN column. The grid is the proven
// GolfCardActivity `fui::table` scorecard pattern. An 18-hole course splits Front/Back
// across two tabs driven by the rocker; a 9-hole course shows one table. No GolfRound
// is ever in scope; nothing here can start or mutate a round.
class GolfCourseTeeInfoActivity final : public Activity, protected UiAppHost {
 public:
  GolfCourseTeeInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* courseName);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t DATA_COLS = 10;                          // 9 holes + the nine's total
  static constexpr uint8_t MAX_TABLE_ROWS = 1 + 2 + GOLF_MAX_TEES;  // hole header + Par + SI + up to 6 tees
  static constexpr uint8_t CELL_CAP = 8;
  static constexpr int16_t LABEL_COLUMN_UNITS = 3;
  static constexpr freeink::ui::ActionId ACTION_TAB = 1;

  char courseName[40]{};
  GolfCourseTeeSet teeSet{};
  bool resolved = false;
  uint8_t holeCount = 0;
  uint8_t activeTab = 0;
  bool firstPaint = true;

  // Row model, rebuilt for the active nine each paint. Row 0 is the hole-number header;
  // row 1 is Par; the optional SI row and one row per tee follow.
  char dataCells[MAX_TABLE_ROWS][DATA_COLS][CELL_CAP]{};
  const char* dataPointers[DATA_COLS]{};
  const char* labelPointer[1]{};
  freeink::ui::TabItem segmentTabs[2]{};
  freeink::ui::TabBarProps tabProps{};
  freeink::ui::TableProps tableProps{};
  uint8_t tableRows = 0;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void tabTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void buildCard(UiScreen& screen, freeink::ui::Rect tableRect, uint8_t firstHole, const char* segmentLabel);
  void changeTab(int delta);
  void drawFooter() const;
};

#endif
