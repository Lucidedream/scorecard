#include "GolfCourseInfoActivity.h"
#if defined(CROSSPOINT_GOLF)
#include <Arduino.h>
#include <FreeInkUIIcon.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseMapBrowserActivity.h"
#include "GolfCourseTeeInfoActivity.h"
#include "GolfNavigation.h"
#include "GolfTiles.h"
#include "components/UITheme.h"
#include "components/icons/golfTileIcons.h"
namespace fui = freeink::ui;
namespace {
const char* categoryLabel(uint8_t category) {
  switch (category) {
    case 0:
      return tr(STR_GOLF_GREENS_LABEL);
    case 1:
      return tr(STR_GOLF_DIFFICULTY_LABEL);
    case 2:
      return tr(STR_GOLF_CADDIES_LABEL);
    case 3:
      return tr(STR_GOLF_VALUE_LABEL);
    case 4:
      return tr(STR_GOLF_CLUBHOUSE_LABEL);
    default:
      return tr(STR_GOLF_FOOD_LABEL);
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
void GolfCourseInfoActivity::buildRows() {
  count = 0;
  if (!resolved) return;
  if (golfShowPlayCourse(golfReadHomeEntry())) playRow = count++;
  mapsRow = count++;
  ++count;
  par = 0;
  yards = 0;
  for (uint8_t n = 0; n < course.nineCount && n < 2; ++n)
    for (auto value : course.nines[n].par) par += value;
  for (uint8_t t = 0; t < course.teeCount; ++t) {
    if (!course.tees[t].hasYards) continue;
    uint32_t sum = 0;
    for (uint8_t n = 0; n < course.nineCount && n < 2; ++n)
      for (auto value : course.tees[t].yards[n]) sum += value;
    if (sum > yards) yards = sum;
  }
  if (info.hasPar) par = info.par;
  if (info.hasYards) yards = info.yards;
  nines[0] = 0;
  for (uint8_t n = 0; n < course.nineCount; ++n) {
    const size_t used = strlen(nines);
    snprintf(nines + used, sizeof(nines) - used, "%s%s", n ? " · " : "", course.nines[n].name);
  }
}
golfui::FooterLabels GolfCourseInfoActivity::golfFooterLabels() const {
  return {tr(STR_BACK), nav.selected == playRow ? tr(STR_GOLF_START) : tr(STR_GOLF_OPEN), tr(STR_GOLF_BUTTON_PREVIOUS),
          tr(STR_GOLF_BUTTON_NEXT)};
}
void GolfCourseInfoActivity::navigateButtons() {
  const int delta =
      mappedInput.wasReleased(MappedInputManager::Button::Left)    ? (mappedInput.isNavDirectionSwapped() ? 1 : -1)
      : mappedInput.wasReleased(MappedInputManager::Button::Right) ? (mappedInput.isNavDirectionSwapped() ? -1 : 1)
                                                                   : 0;
  if (delta && count) {
    moveSelectionTo((nav.selected + count + delta) % count);
    return;
  }
  UiListActivity::navigateButtons();
}
void GolfCourseInfoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  if (!resolved) return;
  auto title = screen.theme().titleText;
  title.bold = true;
  title.maxLines = 1;
  screen.target().text(screen.takeTop(42), course.courseName, title);
  snprintf(facts, sizeof(facts), tr(STR_GOLF_INFO_HEADER_FORMAT),
           info.hasRegion ? info.region : tr(STR_GOLF_COURSE_DETAILS),
           info.hasHoles ? info.holes : course.nineCount * 9, par, static_cast<unsigned long>(yards));
  auto small = screen.theme().smallText;
  small.maxLines = 1;
  screen.target().text(screen.takeTop(32), facts, small);
  const auto strip = golfui::takeTileStrip(screen, static_cast<int16_t>(metrics.verticalSpacing));
  nav.visibleRows = count;
  for (uint8_t i = 0; i < count; ++i) {
    const bool play = i == playRow;
    golfui::drawTile(screen, golfui::tileCell(strip, count, i), ACTION_ROW, i, i == nav.selected,
                     fui::bitmapFromIcon(play           ? icon_flag_32
                                         : i == mapsRow ? icon_map_32
                                                        : icon_table_32),
                     play           ? tr(STR_GOLF_PLAY_TILE)
                     : i == mapsRow ? tr(STR_GOLF_HOLE_MAPS)
                                    : tr(STR_GOLF_TEE_INFO));
  }
  if (info.hasRating) {
    const auto rating = screen.takeTop(52);
    snprintf(facts, sizeof(facts), "%u.%u", info.rating / 10, info.rating % 10);
    golfui::drawFigure(screen.target(), {rating.x, rating.y, 82, 38}, facts, fui::Color::Black);
    screen.target().text({static_cast<int16_t>(rating.x + 86), rating.y, 40, 38}, tr(STR_GOLF_OUT_OF_FIVE), small);
    snprintf(facts, sizeof(facts), tr(STR_GOLF_FROM_REVIEWS_FORMAT), static_cast<unsigned long>(info.reviews));
    screen.target().text({static_cast<int16_t>(rating.x + 132), rating.y, static_cast<int16_t>(rating.width - 132), 38},
                         facts, small);
  }
  if (info.hasScores) {
    for (uint8_t i = 0; i < 6; ++i) {
      const auto row = screen.takeTop(28);
      screen.target().text({row.x, row.y, 116, row.height}, categoryLabel(i), small);
      fui::ProgressBarProps bar{};
      bar.value = info.scores[i];
      bar.max = 50;
      bar.fill = fui::Paint::solid(fui::Color::Black);
      bar.border = bar.fill;
      bar.borderWidth = 1;
      fui::progressBar(screen.frame(),
                       {static_cast<int16_t>(row.x + 124), static_cast<int16_t>(row.y + 8),
                        static_cast<int16_t>(golfui::maxValue(row.width - 178, 1)), 10},
                       bar);
      snprintf(facts, sizeof(facts), "%u.%u", info.scores[i] / 10, info.scores[i] % 10);
      screen.target().text({static_cast<int16_t>(row.right() - 42), row.y, 42, row.height}, facts, small);
    }
  }
  snprintf(facts, sizeof(facts), tr(STR_GOLF_NINES_FORMAT), nines);
  screen.target().text(screen.takeTop(32), facts, small);
  if (info.hasDesigner && info.hasOpened) {
    snprintf(facts, sizeof(facts), tr(STR_GOLF_DESIGNER_OPENED_FORMAT), info.designer, info.opened);
  } else if (info.hasDesigner) {
    snprintf(facts, sizeof(facts), tr(STR_GOLF_DESIGNER_FORMAT), info.designer);
  } else if (info.hasOpened) {
    snprintf(facts, sizeof(facts), tr(STR_GOLF_OPENED_FORMAT), info.opened);
  } else
    return;
  screen.target().text(screen.takeTop(28), facts, small);
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
