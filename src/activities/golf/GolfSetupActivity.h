#pragma once

#include "activities/UiListActivity.h"
#include "golf/CourseStore.h"

class GolfSetupActivity final : public UiListActivity {
 public:
  GolfSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfSetup", renderer, mappedInput) {}

  void onEnter() override;

 private:
  enum class Phase : uint8_t { Course, Tees };
  static constexpr uint8_t MAX_ROWS = GOLF_MAX_COURSES + 1;

  GolfCourseFile files[GOLF_MAX_COURSES]{};
  GolfCourse courses[GOLF_MAX_COURSES]{};
  freeink::ui::ListItem rows[MAX_ROWS]{};
  uint8_t courseCount = 0;
  uint8_t selectedCourse = 0;
  bool overflow = false;
  bool noCourses = false;
  bool saveFailed = false;
  Phase phase = Phase::Course;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;

  void loadCourses();
  void selectCourse(uint8_t index);
  void startRound(const char* tees);
};
