#pragma once

#if defined(CROSSPOINT_GOLF)

#include "GolfNavigation.h"
#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"

// Read-only course list; each row opens Course info.
class GolfCourseMapListActivity final : public GolfUiListActivity {
 public:
  GolfCourseMapListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : GolfUiListActivity("GolfCourseMapList", renderer, mappedInput) {}

  void onEnter() override;
  void setReturnContext(GolfPickerContext value) { context = value; }
  void restoreInfo(const GolfCourseFile& file, uint16_t row) {
    infoFile = file;
    infoRow = row;
    reopenInfo = true;
  }

 private:
  GolfPickerContext context{};
  GolfCourseFile infoFile{};
  uint16_t infoRow = 0;
  bool reopenInfo = false;
  void openInfo(const GolfCourseFile& file, uint16_t selected = 0);
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
