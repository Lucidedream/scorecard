#pragma once

#if defined(CROSSPOINT_GOLF)

#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"

// Read-only course list; each row opens the course-map browser.
class GolfCourseMapListActivity final : public GolfUiListActivity {
 public:
  GolfCourseMapListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : GolfUiListActivity("GolfCourseMapList", renderer, mappedInput) {}

  void onEnter() override;

 private:
  static constexpr uint8_t MAX_ROWS = GOLF_MAX_COURSES + 1;

  GolfCourseSummary courses[GOLF_MAX_COURSES]{};
  freeink::ui::ListItem rows[MAX_ROWS]{};
  freeink::ui::ListProps listProps{};
  char courseDetails[GOLF_MAX_COURSES][48]{};
  char parLabels[GOLF_MAX_COURSES][16]{};
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
