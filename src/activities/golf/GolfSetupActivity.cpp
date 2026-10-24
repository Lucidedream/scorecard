#include "GolfSetupActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfReviewFormat.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/CourseOrder.h"
#include "golf/GolfNinePicker.h"

namespace fui = freeink::ui;

void GolfSetupActivity::onEnter() {
  loadCourses();
  UiListActivity::onEnter();
}

void GolfSetupActivity::loadCourses() {
  auto files = makeUniqueNoThrow<GolfCourseFile[]>(GOLF_MAX_COURSES);
  auto scratch = makeUniqueNoThrow<GolfCourse>();
  courseCount = 0;
  if (!files || !scratch) {
    LOG_ERR("GOLF", "OOM: course list scratch");
    noCourses = true;
    return;
  }
  const auto result = CourseStore::enumerate(files.get(), GOLF_MAX_COURSES);
  overflow = result.overflow;
  for (uint8_t i = 0; i < result.count; ++i) {
    if (!CourseStore::load(files[i], *scratch)) continue;
    auto& summary = courses[courseCount++];
    summary = {};
    summary.file = files[i];
    memcpy(summary.courseName, scratch->courseName, sizeof(summary.courseName));
    summary.nineCount = scratch->nineCount;
    for (uint8_t n = 0; n < scratch->nineCount && n < 2; ++n)
      for (uint8_t par : scratch->nines[n].par) summary.defaultPar += par;
    golfFormatTeeList(*scratch, summary.teeList, sizeof(summary.teeList));
  }
  golfSortCourses(courses, courseCount);
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
  const GolfCourseSummary& course = courses[row];
  snprintf(courseDetails[row], sizeof(courseDetails[row]), tr(STR_GOLF_COURSE_ROW_FORMAT), 9 * course.nineCount,
           course.teeList);
  const uint16_t par = course.defaultPar;
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
  const auto& course = courses[index];
  const auto selection = golfDefaultNineSelection(course);
  if (course.nineCount >= 2) {
    openGolfNineSelect(activityManager, renderer, mappedInput, course.file, selection);
  } else {
    openGolfPlayerSetup(activityManager, renderer, mappedInput, course.file, selection);
  }
}

void GolfSetupActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
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
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
