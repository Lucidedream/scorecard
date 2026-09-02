#pragma once

#include "activities/UiListActivity.h"
#include "golf/CourseStore.h"

class GolfSetupActivity final : public UiListActivity {
 public:
  GolfSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfSetup", renderer, mappedInput) {}

  void onEnter() override;

 private:
  static constexpr uint8_t MAX_ROWS = GOLF_MAX_COURSES + 1;

  GolfCourseFile files[GOLF_MAX_COURSES]{};
  GolfCourse courses[GOLF_MAX_COURSES]{};
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
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawChrome() override;

  void loadCourses();
  void formatCourseRow(uint8_t row);
};
