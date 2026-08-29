#pragma once

#include "activities/UiListActivity.h"
#include "golf/CourseStore.h"

class GolfSetupActivity final : public UiListActivity {
 public:
  GolfSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfSetup", renderer, mappedInput) {}

  void onEnter() override;

 private:
  enum class Phase : uint8_t { Course, Date };
  static constexpr uint8_t MAX_ROWS = GOLF_MAX_COURSES + 2;
  static constexpr uint16_t DEFAULT_DATE = (26u << 9) | (1u << 5) | 1u;

  GolfCourseFile files[GOLF_MAX_COURSES]{};
  GolfCourse courses[GOLF_MAX_COURSES]{};
  freeink::ui::ListItem rows[MAX_ROWS]{};
  GolfCourse chosenCourse{};
  char dateValues[3][8]{};
  uint8_t courseCount = 0;
  bool overflow = false;
  bool noCourses = false;
  bool saveFailed = false;
  Phase phase = Phase::Course;
  uint16_t dateYmd = DEFAULT_DATE;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawFooter() override;

  void loadCourses();
  void selectCourse(const GolfCourse& course);
  void buildDateRows();
  void adjustDate(int delta);
  void startRound();
};
