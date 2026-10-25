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
  static constexpr uint8_t MAX_ROWS = 16;
  GolfCourseFile file;
  GolfCourse course{};
  GolfCourseCatalogInfo info{};
  uint16_t initialRow = 0;
  int8_t playRow = -1;
  bool resolved = false;
  uint8_t count = 0;
  uint8_t mapsRow = 0;
  int8_t scoresRow = -1;
  freeink::ui::ListItem rows[MAX_ROWS]{};
  char values[MAX_ROWS][96]{};
  char facts[96]{};
  freeink::ui::ListProps props{};
  int listCount() const override { return count; }
  const char* headerTitle() const override { return resolved ? course.courseName : tr(STR_GOLF_NO_COURSES); }
  void buildScreen(UiScreen& screen) override;
  void drawChrome() override;
  void activateIndex(int index) override;
  void buildRows();
  uint8_t addFact(const char* label, const char* value = nullptr);
};
#endif
