#include "GolfSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/CourseOrder.h"

namespace fui = freeink::ui;

void GolfSetupActivity::onEnter() {
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
    formatCourseRow(row);
    rows[row].subtitle = courseDetails[row];
    rows[row].value = parLabels[row];
    rows[row].actionValue = row;
  }
  if (overflow) {
    rows[row] = {};
    rows[row].label = tr(STR_GOLF_COURSE_OVERFLOW);
    rows[row].enabled = false;
  }
}

void GolfSetupActivity::formatCourseRow(const uint8_t row) {
  if (row >= courseCount) return;
  const GolfCourse& course = courses[row];
  GolfTeeResolution resolved{};
  const bool hasBlue = CourseStore::resolveTee(files[row], course, TeeSelection::Blue, resolved);
  const bool hasWhite = CourseStore::resolveTee(files[row], course, TeeSelection::White, resolved);
  char tees[24]{};
  if (hasBlue && hasWhite) {
    snprintf(tees, sizeof(tees), tr(STR_GOLF_TEE_PAIR_FORMAT), tr(STR_GOLF_BLUE), tr(STR_GOLF_WHITE));
  } else {
    snprintf(tees, sizeof(tees), "%s",
             hasBlue    ? tr(STR_GOLF_BLUE)
             : hasWhite ? tr(STR_GOLF_WHITE)
                        : tr(STR_GOLF_EM_DASH));
  }
  snprintf(courseDetails[row], sizeof(courseDetails[row]), tr(STR_GOLF_COURSE_ROW_FORMAT), course.holeCount, tees);

  uint16_t par = 0;
  for (uint8_t hole = 0; hole < course.holeCount; ++hole) par += course.par[hole];
  char parValue[8]{};
  if (par == 0) {
    snprintf(parValue, sizeof(parValue), "%s", tr(STR_GOLF_EM_DASH));
  } else {
    snprintf(parValue, sizeof(parValue), "%u", par);
  }
  snprintf(parLabels[row], sizeof(parLabels[row]), tr(STR_GOLF_PAR_VALUE_FORMAT), parValue);
}

int GolfSetupActivity::listCount() const { return courseCount + (overflow ? 1 : 0); }

const char* GolfSetupActivity::headerTitle() const {
  return noCourses ? tr(STR_GOLF_NO_COURSES) : tr(STR_GOLF_CHOOSE_COURSE);
}

void GolfSetupActivity::onBackButton() { openGolfHome(activityManager, renderer, mappedInput); }

void GolfSetupActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index < 0 || index >= courseCount) return;
  openGolfPlayerSetup(activityManager, renderer, mappedInput, files[index], courses[index]);
}

void GolfSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = static_cast<uint16_t>(listCount());
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  listProps.rowHeight = 96;
  syncListViewport(screen, listProps, true);
  screen.list(listProps);
}

void GolfSetupActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
