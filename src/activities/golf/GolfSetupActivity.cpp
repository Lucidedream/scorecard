#include "GolfSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "GolfNavigation.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "golf/CourseBuiltIns.h"
#include "golf/CourseOrder.h"
#include "golf/GolfRoundStore.h"

namespace fui = freeink::ui;

void GolfSetupActivity::onEnter() {
  phase = Phase::Course;
  loadCourses();
  UiListActivity::onEnter();
}

void GolfSetupActivity::loadCourses() {
  const GolfCourseListResult result = CourseStore::enumerate(files, GOLF_MAX_COURSES);
  overflow = result.overflow;
  courseCount = 0;
  for (uint8_t i = 0; i < result.count; ++i) {
    GolfCourse course{};
    if (CourseStore::load(files[i], course)) {
      files[courseCount] = files[i];
      courses[courseCount++] = course;
    }
  }
  for (uint8_t i = 1; i < courseCount; ++i) {
    const GolfCourse value = courses[i];
    const GolfCourseFile valueFile = files[i];
    uint8_t position = i;
    while (position > 0 && golfCourseSortsBefore(valueFile, value, files[position - 1], courses[position - 1])) {
      courses[position] = courses[position - 1];
      files[position] = files[position - 1];
      --position;
    }
    courses[position] = value;
    files[position] = valueFile;
  }
  noCourses = courseCount == 0;
  uint8_t row = 0;
  for (; row < courseCount; ++row) {
    rows[row] = {};
    rows[row].label = courses[row].courseName;
    rows[row].actionValue = row;
  }
  if (overflow) {
    rows[row] = {};
    rows[row].label = GolfStrings::COURSE_OVERFLOW;
    rows[row].enabled = false;
    ++row;
  }
}

int GolfSetupActivity::listCount() const {
  if (phase == Phase::Tees) return 2;
  return courseCount + (overflow ? 1 : 0);
}

const char* GolfSetupActivity::headerTitle() const {
  if (saveFailed) return GolfStrings::SAVE_ERROR;
  if (phase == Phase::Tees) return GolfStrings::CHOOSE_TEES;
  return noCourses ? GolfStrings::NO_COURSES : GolfStrings::COURSES;
}

void GolfSetupActivity::onBackButton() {
  if (phase == Phase::Tees) {
    phase = Phase::Course;
    saveFailed = false;
    nav.reset();
    loadCourses();
    requestUpdate();
    return;
  }
  openGolfHome(activityManager, renderer, mappedInput);
}

void GolfSetupActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (phase == Phase::Tees) {
    if (index == 0) startRound(GolfStrings::BLUE);
    if (index == 1) startRound(GolfStrings::WHITE);
    return;
  }
  if (index < courseCount) {
    selectCourse(static_cast<uint8_t>(index));
    return;
  }
}

void GolfSetupActivity::selectCourse(const uint8_t index) {
  selectedCourse = index;
  phase = Phase::Tees;
  saveFailed = false;
  nav.reset();
  rows[0] = {};
  rows[0].label = GolfStrings::BLUE_TEES;
  rows[0].actionValue = 0;
  rows[1] = {};
  rows[1].label = GolfStrings::WHITE_TEES;
  rows[1].actionValue = 1;
  requestUpdate();
}

void GolfSetupActivity::startRound(const char* tees) {
  GolfCourse& chosenCourse = courses[selectedCourse];
  const GolfCourseFile& selectedFile = files[selectedCourse];
  // Tee-specific yardages are a built-in-only mechanism. An SD file (even one that
  // overrides a built-in, CONTRACTS-V2 §7.1) carries its own data and is left as loaded.
  if (selectedFile.filename[0] == '\0') {
    applyBuiltInTeeYardages(selectedFile.builtInIndex, tees, chosenCourse);
  }
  snprintf(chosenCourse.tees, sizeof(chosenCourse.tees), "%s", tees);
  GolfRound round{};
  CourseStore::applyGolfCourse(chosenCourse, round, 0);
  GOLF_ROUND_STORE.setRound(round);
  if (!GOLF_ROUND_STORE.saveToFile()) {
    saveFailed = true;
    requestUpdate();
    return;
  }
  openGolfScoring(activityManager, renderer, mappedInput);
}

void GolfSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.subtitleText = screen.theme().smallText;
  syncListViewport(screen, props, phase == Phase::Course);
  screen.list(props);
}

#endif
