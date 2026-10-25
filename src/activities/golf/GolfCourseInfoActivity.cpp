#include "GolfCourseInfoActivity.h"
#if defined(CROSSPOINT_GOLF)
#include <Arduino.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseMapBrowserActivity.h"
#include "GolfCourseTeeInfoActivity.h"
#include "GolfNavigation.h"
#include "components/UITheme.h"
namespace fui = freeink::ui;
namespace {
const char* categoryLabel(uint8_t category) {
  switch (category) {
    case 0:
      return tr(STR_GOLF_GREEN_FAIRWAY);
    case 1:
      return tr(STR_GOLF_COURSE_DIFFICULTY);
    case 2:
      return tr(STR_GOLF_CADDIE_SERVICE);
    case 3:
      return tr(STR_GOLF_VALUE_FOR_MONEY);
    case 4:
      return tr(STR_GOLF_CLUB_FACILITIES);
    default:
      return tr(STR_GOLF_CATERING);
  }
}
}  // namespace
void GolfCourseInfoActivity::onEnter() {
  resolved = CourseStore::load(file, course);
  if (resolved)
    CourseStore::loadCatalogInfo(file, info);
  else
    LOG_ERR("GOLF", "Course info: failed to load course");
  buildRows();
  LOG_DBG("GOLF", "Catalog Course info: free heap %u", ESP.getFreeHeap());
  UiListActivity::onEnter();
  nav.reset(initialRow);
}
uint8_t GolfCourseInfoActivity::addFact(const char* label, const char* value) {
  const uint8_t row = count++;
  rows[row] = {};
  rows[row].label = label;
  rows[row].value = value;
  rows[row].enabled = false;
  rows[row].actionValue = row;
  return row;
}
void GolfCourseInfoActivity::buildRows() {
  count = 0;
  if (!resolved) return;
  uint16_t par = 0;
  uint32_t yards = 0;
  for (uint8_t n = 0; n < course.nineCount && n < 2; ++n) {
    for (uint8_t h = 0; h < 9; ++h) {
      par += course.nines[n].par[h];
    }
  }
  for (uint8_t tee = 0; tee < course.teeCount; ++tee) {
    if (!course.tees[tee].hasYards) continue;
    uint32_t sum = 0;
    for (uint8_t n = 0; n < course.nineCount && n < 2; ++n)
      for (uint16_t value : course.tees[tee].yards[n]) sum += value;
    if (sum > yards) yards = sum;
  }
  if (info.hasPar) par = info.par;
  if (info.hasYards) yards = info.yards;
  snprintf(facts, sizeof(facts), tr(STR_GOLF_COURSE_HOLES_FORMAT), info.hasHoles ? info.holes : course.nineCount * 9);
  addFact(info.hasRegion ? info.region : tr(STR_GOLF_COURSE_DETAILS), facts);
  if (par) {
    snprintf(values[count], sizeof(values[count]), "%u", par);
    addFact(tr(STR_GOLF_PAR), values[count]);
  }
  if (yards) {
    snprintf(values[count], sizeof(values[count]), "%lu", static_cast<unsigned long>(yards));
    addFact(tr(STR_GOLF_YARDS_UNIT), values[count]);
  }
  if (info.hasRating) {
    snprintf(values[count], sizeof(values[count]), tr(STR_GOLF_RATING_LINE_FORMAT), info.rating / 10, info.rating % 10,
             static_cast<unsigned long>(info.reviews));
    addFact(values[count]);
  }
  if (info.hasScores) {
    scoresRow = count;
    for (uint8_t i = 0; i < 6; ++i) {
      snprintf(values[count], sizeof(values[count]), tr(STR_GOLF_SCORE_VALUE_FORMAT), info.scores[i] / 10,
               info.scores[i] % 10);
      const uint8_t row = addFact(categoryLabel(i), values[count]);
      rows[row].subtitle = " ";
    }
  }
  if (info.hasDesigner) addFact(info.designer);
  if (info.hasOpened) {
    snprintf(values[count], sizeof(values[count]), tr(STR_GOLF_OPENED_FORMAT), info.opened);
    addFact(values[count]);
  }
  mapsRow = count;
  addFact(tr(STR_GOLF_HOLE_MAPS));
  rows[mapsRow].enabled = true;
  addFact(tr(STR_GOLF_TEE_INFO));
  rows[mapsRow + 1].enabled = true;
  if (golfShowPlayCourse(golfReadHomeEntry())) {
    playRow = addFact(tr(STR_GOLF_PLAY_THIS_COURSE));
    rows[playRow].enabled = true;
  }
}
void GolfCourseInfoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  props = {};
  props.items = rows;
  props.count = count;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.rowHeight = 80;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 1;
  props.subtitleText = screen.theme().smallText;
  props.subtitleText.maxLines = 1;
  syncListViewport(screen, props, true);
  screen.list(props);
  if (scoresRow < 0) return;
  const auto body = screen.body();
  const int rowGap = screen.theme().listRowGap;
  const int visible = fui::listVisibleRows(body, props.rowHeight, rowGap);
  for (int offset = 0; offset < visible; ++offset) {
    const int category = props.topIndex + offset - scoresRow;
    if (category < 0 || category >= 6) continue;
    fui::ProgressBarProps bar{};
    bar.value = info.scores[category];
    bar.max = 50;
    bar.fill = fui::Paint::solid(screen.theme().smallText.color);
    bar.border = bar.fill;
    bar.borderWidth = 1;
    const int16_t inset = static_cast<int16_t>(screen.theme().spaceMd);
    fui::progressBar(
        screen.frame(),
        fui::Rect{static_cast<int16_t>(body.x + inset),
                  static_cast<int16_t>(body.y + offset * (props.rowHeight + rowGap) + props.rowHeight * 3 / 4),
                  static_cast<int16_t>(body.width - 2 * inset), static_cast<int16_t>(screen.theme().spaceSm)},
        bar);
  }
}
void GolfCourseInfoActivity::drawChrome() {
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, UITheme::getInstance().getMetrics().topPadding);
  golfui::drawHeader(renderer, chrome.header, headerTitle());
}
void GolfCourseInfoActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (!resolved) return;
  if (index == playRow) {
    if (!golfShowPlayCourse(golfReadHomeEntry())) return;
    golfSetInfoReturn(file, index);
    startGolfCourse(activityManager, renderer, mappedInput, file, course.nineCount);
  } else if (index == mapsRow) {
    auto browser = makeUniqueNoThrow<GolfCourseMapBrowserActivity>(renderer, mappedInput, file);
    if (!browser) {
      LOG_ERR("GOLF", "OOM: hole maps activity");
      return;
    }
    startActivityForResult(std::move(browser), nullptr);
  } else if (index == mapsRow + 1) {
    auto tees = makeUniqueNoThrow<GolfCourseTeeInfoActivity>(renderer, mappedInput, file);
    if (!tees) {
      LOG_ERR("GOLF", "OOM: tee info activity");
      return;
    }
    startActivityForResult(std::move(tees), nullptr);
  }
}
#endif
