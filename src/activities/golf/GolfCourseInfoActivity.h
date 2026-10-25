#pragma once
#if defined(CROSSPOINT_GOLF)
#include <I18n.h>

#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"
#include "golf/GolfCourseCatalogInfo.h"

class GolfCourseInfoActivity final : public GolfUiListActivity {
 public:
  GolfCourseInfoActivity(GfxRenderer& renderer, MappedInputManager& input, const GolfCourseFile& file,
                         uint16_t selected = 0)
      : GolfUiListActivity("GolfCourseInfo", renderer, input), file(file), initialRow(selected) {}
  void onEnter() override;

 private:
  GolfCourseFile file;
  GolfCourse course{};
  GolfCourseCatalogInfo info{};
  uint16_t initialRow = 0;
  int8_t playRow = -1;
  bool resolved = false;
  uint8_t count = 0;
  uint8_t mapsRow = 0;
  uint16_t par = 0;
  uint32_t yards = 0;
  char facts[160]{};
  char nines[80]{};
  int listCount() const override { return count; }
  const char* headerTitle() const override { return resolved ? course.courseName : tr(STR_GOLF_NO_COURSES); }
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  void buildRows();
  golfui::FooterLabels golfFooterLabels() const override;
  void navigateButtons() override;
};
#endif
