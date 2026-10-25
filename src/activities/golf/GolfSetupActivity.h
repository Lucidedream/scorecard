#pragma once

#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"

class GolfSetupActivity final : public GolfUiListActivity {
 public:
  GolfSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : GolfUiListActivity("GolfSetup", renderer, mappedInput) {}

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

  golfui::FooterLabels golfFooterLabels() const override;
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawChrome() override;

  void loadCourses();
  void formatCourseRow(uint8_t row);
};
