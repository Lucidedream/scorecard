#pragma once

#if defined(CROSSPOINT_GOLF)

#include "activities/UiListActivity.h"
#include "golf/CourseStore.h"

// Read-only course browser entry point (CONTRACTS-V2 §30): one row per course *name*,
// deduped across every file that shares it, so a course split into separate tee files
// (docs/golf/examples/sanyang-suzhou.json / -white.json) shows once. Picking a row opens
// GolfCourseMapBrowserActivity; unlike GolfSetupActivity, nothing here starts a round.
class GolfCourseMapListActivity final : public UiListActivity {
 public:
  GolfCourseMapListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfCourseMapList", renderer, mappedInput) {}

  void onEnter() override;

 private:
  static constexpr uint8_t MAX_ROWS = GOLF_MAX_COURSES + 1;

  GolfCourseFile files[GOLF_MAX_COURSES]{};
  GolfCourse courses[GOLF_MAX_COURSES]{};
  uint8_t primaryIndex[GOLF_MAX_COURSES]{};  // row -> index of that name's first entry in files/courses
  freeink::ui::ListItem rows[MAX_ROWS]{};
  freeink::ui::ListProps listProps{};
  char courseDetails[GOLF_MAX_COURSES][48]{};
  char parLabels[GOLF_MAX_COURSES][16]{};
  uint8_t loadedCount = 0;
  uint8_t courseCount = 0;
  bool overflow = false;
  bool noCourses = false;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;

  void loadCourses();
  void formatCourseRow(uint8_t row);
};

#endif
