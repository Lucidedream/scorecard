#include "GolfCourseMapListActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseInfoActivity.h"
#include "GolfReviewFormat.h"
#include "GolfTiles.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/CourseOrder.h"

namespace fui = freeink::ui;

void GolfCourseMapListActivity::onEnter() {
  LOG_DBG("GOLF", "Catalog picker My courses: free heap %u", ESP.getFreeHeap());
  loadCourses();
  UiListActivity::onEnter();
  nav.reset(context.selected);
  if (reopenInfo) openInfo(infoFile, infoRow);
}

void GolfCourseMapListActivity::loadCourses() {
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
    summary.hasMaps = CourseStore::hasMaps(*scratch);
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

void GolfCourseMapListActivity::formatCourseRow(const uint8_t row) {
  if (row >= courseCount) return;
  const GolfCourseSummary& course = courses[row];
  snprintf(courseDetails[row], sizeof(courseDetails[row]), tr(STR_GOLF_CARD_DETAIL_FORMAT), 9 * course.nineCount,
           course.defaultPar, course.hasMaps ? tr(STR_GOLF_MAPS_LABEL) : tr(STR_GOLF_NO_MAPS_LABEL));
  const uint16_t par = course.defaultPar;
  char parValue[8]{};
  if (par == 0) {
    snprintf(parValue, sizeof(parValue), "%s", tr(STR_GOLF_EM_DASH));
  } else {
    snprintf(parValue, sizeof(parValue), "%u", par);
  }
  snprintf(parLabels[row], sizeof(parLabels[row]), tr(STR_GOLF_PAR_VALUE_FORMAT), parValue);
}

golfui::FooterLabels GolfCourseMapListActivity::golfFooterLabels() const {
  return {tr(STR_BACK), tr(STR_GOLF_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN)};
}

int GolfCourseMapListActivity::listCount() const { return courseCount + (overflow ? 1 : 0); }

const char* GolfCourseMapListActivity::headerTitle() const {
  return noCourses ? tr(STR_GOLF_NO_COURSES) : tr(STR_GOLF_MY_COURSES);
}

void GolfCourseMapListActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index < 0 || index >= courseCount) return;
  context.selected = index;
  openInfo(courses[index].file);
}
void GolfCourseMapListActivity::openInfo(const GolfCourseFile& file, uint16_t selected) {
  golfSetSetupReturnContext(context);
  auto browser = makeUniqueNoThrow<GolfCourseInfoActivity>(renderer, mappedInput, file, selected);
  if (!browser) {
    LOG_ERR("GOLF", "OOM: course map browser activity");
    return;
  }
  startActivityForResult(std::move(browser), nullptr);
}

void GolfCourseMapListActivity::buildScreen(UiScreen& screen) {
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
  const auto body = screen.body();
  const int visible = golfui::courseCardsPerPage(body);
  nav.visibleRows = visible;
  if (nav.followOnBuild) {
    nav.follow(listCount());
    nav.followOnBuild = false;
  }
  nav.scrollBy(0, listCount());
  nav.drawnRows = visible;
  nav.followPending = false;
  for (int offset = 0; offset < visible && nav.top + offset < listCount(); ++offset) {
    const int row = nav.top + offset;
    if (row >= courseCount) {
      golfui::drawCourseCard(screen, golfui::courseCard(body, offset), ACTION_ROW, row, false,
                             tr(STR_GOLF_COURSE_OVERFLOW), "", "", "", false);
      continue;
    }
    char par[8];
    snprintf(par, sizeof(par), "%u", courses[row].defaultPar);
    golfui::drawCourseCard(screen, golfui::courseCard(body, offset), ACTION_ROW, row, row == nav.selected,
                           courses[row].courseName, courseDetails[row], par, tr(STR_GOLF_PAR));
  }
}

void GolfCourseMapListActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
