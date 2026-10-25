#include "GolfCoursePickerActivity.h"
#if defined(CROSSPOINT_GOLF)
#include <Arduino.h>
#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "GolfCourseInfoActivity.h"
#include "GolfCourseMapListActivity.h"
#include "GolfTiles.h"
#include "components/UITheme.h"
#include "components/icons/golfTileIcons.h"

namespace fui = freeink::ui;
void GolfCoursePickerActivity::onEnter() {
  // Course and streaming buffers exceed the task stack budget; allocate once per visit.
  scratch = makeUniqueNoThrow<Scratch>();
  if (!scratch) LOG_ERR("GOLF", "OOM: catalog picker scratch");
  hasCatalog = scratch && loadCatalog();
  if (scratch) loadRecent();
  // File entries exceed the C3 stack budget; release after the single count scan.
  {
    auto ownerFiles = makeUniqueNoThrow<GolfCourseFile[]>(GOLF_MAX_COURSES);
    if (ownerFiles)
      ownerCount = CourseStore::enumerate(ownerFiles.get(), GOLF_MAX_COURSES).count;
    else
      LOG_ERR("GOLF", "OOM: picker owner count");
  }
  UiListActivity::onEnter();
  app.on(
      ACTION_USER,
      [](const fui::ActionEvent& event, void* user) {
        auto* self = static_cast<GolfCoursePickerActivity*>(user);
        RenderLock lock(*self);
        self->byRating = event.value == 0;
        self->sortRegion();
        self->enterLevel(Level::Courses);
      },
      this);
  {
    RenderLock lock(*this);
    region = initial.region;
    byRating = initial.byRating;
    if (initial.level == Level::Courses) {
      if (hasCatalog && region < scratch->catalog.count)
        loadRegion();
      else
        initial.level = Level::Hub;
    } else if (initial.level == Level::Regions && !hasCatalog) {
      initial.level = Level::Hub;
    }
    enterLevel(initial.level == Level::MyCourses ? Level::Hub : initial.level);
    nav.reset(initial.level == Level::MyCourses ? initial.parentSelected : initial.selected);
  }
  if (initial.level == Level::MyCourses && mode == Mode::Browse)
    openMyCourses(initial.skipHub);
  else if (reopenInfo)
    openInfo(infoFile);
  else if (!hasCatalog && !recentCount)
    openMyCourses(true);
}
void GolfCoursePickerActivity::onExit() {
  indexFile.close();
  scratch.reset();
  GolfUiListActivity::onExit();
}
bool GolfCoursePickerActivity::loadCatalog() {
  HalFile file;
  if (!Storage.exists("/golf/catalog/regions.txt") ||
      !Storage.openFileForRead("GOLF", "/golf/catalog/regions.txt", file))
    return false;
  while (file.available()) {
    int size = file.read(scratch->chunk, sizeof(scratch->chunk));
    if (size <= 0) return false;
    scratch->catalog.feed(scratch->chunk, size);
  }
  if (!scratch->catalog.finish() || !Storage.openFileForRead("GOLF", "/golf/catalog/index.dat", indexFile))
    return false;
  if (indexFile.size() != static_cast<size_t>(scratch->catalog.courseCount) * GOLF_CATALOG_RECORD_SIZE) {
    LOG_ERR("GOLF", "Catalog index has wrong record length/count");
    indexFile.close();
    return false;
  }
  mapCount = 0;
  for (uint16_t i = 0; i < scratch->catalog.courseCount; ++i)
    if (readRecord(i, scratch->a) && scratch->a.hasMaps) ++mapCount;
  golfSortCatalogRegions(scratch->catalog.regions, scratch->catalog.count);
  return true;
}
bool GolfCoursePickerActivity::resolveRecent(const char* name, void* user) {
  auto* self = static_cast<GolfCoursePickerActivity*>(user);
  if (self->recentCount >= 5 || !CourseStore::findByName(name, self->recent[self->recentCount], self->scratch->course))
    return false;
  ++self->recentCount;
  return true;
}
void GolfCoursePickerActivity::loadRecent() {
  // The CSV reader owns a bounded line and five names, and is released after resolution.
  auto reader = makeUniqueNoThrow<GolfRecentReader>();
  if (!reader) {
    LOG_ERR("GOLF", "OOM: Recent reader");
    return;
  }
  HalFile file;
  if (!Storage.exists("/golf/rounds/index.csv") || !Storage.openFileForRead("GOLF", "/golf/rounds/index.csv", file))
    return;
  const uint32_t started = millis();
  size_t position = file.size();
  while (position && reader->count < 5) {
    const size_t size = position < sizeof(scratch->chunk) ? position : sizeof(scratch->chunk);
    position -= size;
    if (!file.seek(position) || file.read(scratch->chunk, size) != static_cast<int>(size)) {
      LOG_ERR("GOLF", "Recent index read failed");
      recentCount = 0;
      return;
    }
    reader->feedReverse(scratch->chunk, size, resolveRecent, this);
  }
  reader->finishReverse(resolveRecent, this);
  LOG_DBG("GOLF", "Recent loaded %u courses in %lu ms", recentCount, static_cast<unsigned long>(millis() - started));
}
void GolfCoursePickerActivity::enterLevel(Level next) {
  level = next;
  nav.reset();
  closeRouting();
  LOG_DBG("GOLF", "Catalog picker level %u: free heap %u", static_cast<unsigned>(level), ESP.getFreeHeap());
  requestUpdate();
}
int GolfCoursePickerActivity::listCount() const {
  if (!scratch) return 0;
  if (level == Level::Regions) return scratch->catalog.count;
  if (level == Level::Courses) return recordCount + 1;
  if (level == Level::Recent) return recentCount;
  return (recentCount ? 1 : 0) + 1 + (hasCatalog ? 1 : 0);
}
const char* GolfCoursePickerActivity::headerTitle() const {
  if (level == Level::Regions) return tr(STR_GOLF_BY_REGION);
  if (level == Level::Recent) return tr(STR_GOLF_RECENT);
  if (level == Level::Courses) return scratch->catalog.regions[region].name;
  return mode == Mode::Play ? tr(STR_GOLF_NEW_ROUND) : tr(STR_GOLF_COURSE_MAP);
}
bool GolfCoursePickerActivity::readRecord(uint16_t ordinal, GolfCatalogRecord& record) {
  if (!indexFile || !indexFile.seek(static_cast<size_t>(ordinal) * GOLF_CATALOG_RECORD_SIZE) ||
      indexFile.read(scratch->bytes, sizeof(scratch->bytes)) != sizeof(scratch->bytes)) {
    LOG_ERR("GOLF", "Catalog index read failed at %u", ordinal);
    return false;
  }
  if (!golfParseCatalogRecord(scratch->bytes, sizeof(scratch->bytes), record)) {
    LOG_ERR("GOLF", "Skipped malformed catalog record %u", ordinal);
    return false;
  }
  if ((++recordReads & 63) == 0) delay(1);
  return true;
}
void GolfCoursePickerActivity::loadRegion() {
  recordCount = 0;
  memset(ratingKeys, 255, sizeof(ratingKeys));
  const auto& r = scratch->catalog.regions[region];
  for (uint16_t i = 0; i < r.count; ++i) {
    if (!readRecord(r.firstRecord + i, scratch->a)) continue;
    if (scratch->a.region != r.index) {
      LOG_ERR("GOLF", "Catalog region mismatch at %u", r.firstRecord + i);
      continue;
    }
    records[recordCount++] = r.firstRecord + i;
    ratingKeys[i] = scratch->a.hasRating ? scratch->a.rating : 0;
  }
  sortRegion();
}
void GolfCoursePickerActivity::sortRegion() {
  const auto& r = scratch->catalog.regions[region];
  if (byRating) {
    golfSortCatalogOrdinals(records, recordCount, ratingKeys, r.firstRecord);
  } else {
    recordCount = 0;
    for (uint16_t i = 0; i < r.count; ++i)
      if (ratingKeys[i] != 255) records[recordCount++] = r.firstRecord + i;
  }
}
const char* GolfCoursePickerActivity::hubLabel(int index) const {
  if (recentCount && index == 0) return tr(STR_GOLF_RECENT);
  return index == (recentCount ? 1 : 0) ? tr(STR_GOLF_MY_COURSES) : tr(STR_GOLF_BY_REGION);
}
golfui::FooterLabels GolfCoursePickerActivity::golfFooterLabels() const {
  const bool card = level == Level::Recent || (level == Level::Courses && nav.selected > 0);
  return {tr(STR_BACK), card && mode == Mode::Play ? tr(STR_GOLF_START) : tr(STR_GOLF_OPEN),
          tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT)};
}
void GolfCoursePickerActivity::navigateButtons() {
  if (listCount() <= 0) return;
  const int delta =
      mappedInput.wasReleased(MappedInputManager::Button::Left)    ? (mappedInput.isNavDirectionSwapped() ? 1 : -1)
      : mappedInput.wasReleased(MappedInputManager::Button::Right) ? (mappedInput.isNavDirectionSwapped() ? -1 : 1)
                                                                   : 0;
  if (delta) {
    moveSelectionTo((nav.selected + listCount() + delta) % listCount());
    return;
  }
  UiListActivity::navigateButtons();
}
bool GolfCoursePickerActivity::handleCustomInput() {
  if (level != Level::Regions) return false;
  const auto swipe = mappedInput.wasSwipe();
  if (mappedInput.wasBackGesture()) return false;
  const int delta = swipe == MappedInputManager::SwipeDir::Left    ? 1
                    : swipe == MappedInputManager::SwipeDir::Right ? -1
                                                                   : 0;
  if (!delta) return false;
  const int pages = golfui::pageCount(listCount(), golfui::REGION_PAGE_SIZE);
  if (!pages) return false;
  moveSelectionTo(((nav.selected / 10 + pages + delta) % pages) * 10);
  return true;
}
void GolfCoursePickerActivity::fillRow(uint8_t slot, uint16_t absolute) {
  auto& row = rows[slot];
  row = {};
  row.actionValue = absolute;
  row.label = names[slot];
  names[slot][0] = details[slot][0] = figures[slot][0] = units[slot][0] = 0;
  if (level == Level::Recent) {
    if (!CourseStore::load(recent[absolute], scratch->course)) {
      row.enabled = false;
      return;
    }
    const auto& course = scratch->course;
    snprintf(names[slot], sizeof(names[slot]), "%s", course.courseName);
    uint16_t par = 0;
    for (uint8_t n = 0; n < course.nineCount && n < 2; ++n)
      for (auto p : course.nines[n].par) par += p;
    snprintf(details[slot], sizeof(details[slot]), tr(STR_GOLF_CARD_DETAIL_FORMAT), course.nineCount * 9, par,
             CourseStore::hasMaps(course) ? tr(STR_GOLF_MAPS_LABEL) : tr(STR_GOLF_NO_MAPS_LABEL));
    if (recent[absolute].source != GolfCourseSource::Catalog) {
      snprintf(figures[slot], sizeof(figures[slot]), "%u", par);
      snprintf(units[slot], sizeof(units[slot]), "%s", tr(STR_GOLF_PAR));
      return;
    }
    if (CourseStore::loadCatalogInfo(recent[absolute], scratch->info) && scratch->info.hasRating) {
      snprintf(figures[slot], sizeof(figures[slot]), "%u.%u", scratch->info.rating / 10, scratch->info.rating % 10);
      snprintf(units[slot], sizeof(units[slot]), tr(STR_GOLF_REVIEWS_FORMAT),
               static_cast<unsigned long>(scratch->info.reviews));
    } else
      snprintf(figures[slot], sizeof(figures[slot]), "%s", tr(STR_GOLF_EM_DASH));
    return;
  } else if (!readRecord(records[absolute - 1], scratch->a)) {
    row.enabled = false;
    return;
  }
  const auto& r = scratch->a;
  snprintf(names[slot], sizeof(names[slot]), "%s", r.name);
  snprintf(details[slot], sizeof(details[slot]), tr(STR_GOLF_CARD_DETAIL_FORMAT), r.nineCount * 9, r.par,
           r.hasMaps ? tr(STR_GOLF_MAPS_LABEL) : tr(STR_GOLF_NO_MAPS_LABEL));
  if (r.hasRating) {
    snprintf(figures[slot], sizeof(figures[slot]), "%u.%u", r.rating / 10, r.rating % 10);
    snprintf(units[slot], sizeof(units[slot]), tr(STR_GOLF_REVIEWS_FORMAT), static_cast<unsigned long>(r.reviews));
  } else
    snprintf(figures[slot], sizeof(figures[slot]), "%s", tr(STR_GOLF_EM_DASH));
}
void GolfCoursePickerActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  if (!scratch) return;
  if (nav.selected < 0 || nav.selected >= listCount()) nav.reset();
  if (level == Level::Hub) {
    const auto strip = screen.takeTop(190, metrics.verticalSpacing);
    nav.visibleRows = listCount();
    for (int i = 0; i < listCount(); ++i) {
      const bool recentTile = recentCount && i == 0;
      const bool ownerTile = i == (recentCount ? 1 : 0);
      snprintf(details[i], sizeof(details[i]),
               recentTile  ? tr(STR_GOLF_CATALOG_COUNT_FORMAT)
               : ownerTile ? tr(STR_GOLF_ON_CARD_FORMAT)
                           : tr(STR_GOLF_REGIONS_FORMAT),
               recentTile  ? recentCount
               : ownerTile ? ownerCount
                           : scratch->catalog.count);
      golfui::drawTile(screen, golfui::tileCell(strip, listCount(), i), ACTION_ROW, i, nav.selected == i,
                       fui::bitmapFromIcon(recentTile  ? icon_history_32
                                           : ownerTile ? icon_flag_32
                                                       : icon_map_pin_32),
                       hubLabel(i), details[i]);
    }
    hubDetail[0] = 0;
    if (recentCount && nav.selected == 0) {
      for (uint8_t i = 0; i < recentCount && i < 4; ++i) {
        if (!CourseStore::load(recent[i], scratch->course)) continue;
        snprintf(details[i], sizeof(details[i]), tr(STR_GOLF_RECENT_PREVIEW_FORMAT), scratch->course.courseName,
                 scratch->course.nineCount * 9);
        const size_t used = strlen(hubDetail);
        snprintf(hubDetail + used, sizeof(hubDetail) - used, "%s%s", used ? "\n" : "", details[i]);
      }
    } else if (nav.selected == (recentCount ? 1 : 0)) {
      snprintf(hubDetail, sizeof(hubDetail), tr(STR_GOLF_ON_CARD_FORMAT), ownerCount);
    } else
      snprintf(hubDetail, sizeof(hubDetail), tr(STR_GOLF_CATALOG_SUMMARY_FORMAT), scratch->catalog.courseCount,
               scratch->catalog.count, mapCount);
    golfui::drawDetail(screen, hubLabel(nav.selected), hubDetail, metrics.verticalSpacing, 4);
    return;
  }
  if (level == Level::Regions) {
    const auto pager = screen.takeBottom(40);
    const auto grid = screen.body();
    const int page = nav.selected / 10;
    nav.visibleRows = 10;
    for (int i = 0; i < 10 && page * 10 + i < listCount(); ++i) {
      const int row = page * 10 + i;
      const auto& region = scratch->catalog.regions[row];
      snprintf(figures[i], sizeof(figures[i]), "%u", region.count);
      golfui::drawRegion(screen, golfui::regionCell(grid, i), ACTION_ROW, row, row == nav.selected, region.name,
                         region.count);
    }
    snprintf(details[0], sizeof(details[0]), "%d / %d", page + 1, golfui::pageCount(listCount(), 10));
    auto style = screen.theme().smallText;
    style.align = fui::TextAlign::Center;
    const int pages = golfui::pageCount(listCount(), 10);
    screen.target().text({pager.x, pager.y, pager.width, 22}, details[0], style);
    const int start = pager.x + (pager.width - pages * 16) / 2;
    for (int i = 0; i < pages; ++i) {
      const fui::Rect pip{static_cast<int16_t>(start + i * 16), static_cast<int16_t>(pager.y + 26), 8, 8};
      if (i == page)
        screen.target().fill(pip, fui::Paint::solid(fui::Color::Black), 4);
      else
        screen.target().stroke(pip, fui::Paint::solid(fui::Color::Black), 1, 4);
    }
    return;
  }
  const bool sortVisible = level == Level::Courses && golfui::courseSortVisible(screen.body(), nav.selected, nav.top);
  if (sortVisible) {
    const auto segment = screen.takeTop(48);
    for (uint8_t i = 0; i < 2; ++i) {
      fui::ButtonProps button{};
      button.label = i ? tr(STR_GOLF_SORT_NAME_LABEL) : tr(STR_GOLF_SORT_RATING_LABEL);
      button.action = ACTION_USER;
      button.value = i;
      button.inputMask = fui::InputTouch;
      button.state = nav.selected == 0 ? fui::StateSelected : fui::StateNormal;
      const auto cell = golfui::sortSegmentCell(segment, i);
      button.text = screen.theme().bodyText;
      screen.button(button, cell);
      if (byRating == (i == 0))
        screen.target().line({static_cast<int16_t>(cell.x + 20), static_cast<int16_t>(cell.bottom() - 6)},
                             {static_cast<int16_t>(cell.right() - 20), static_cast<int16_t>(cell.bottom() - 6)}, 2,
                             fui::Paint::solid(nav.selected == 0 ? fui::Color::White : fui::Color::Black));
    }
  }
  const auto body = screen.body();
  nav.visibleRows = golfui::courseCardsPerPage(body);
  if (nav.followOnBuild) {
    nav.follow(listCount());
    nav.followOnBuild = false;
  }
  nav.drawnRows = nav.visibleRows;
  nav.followPending = false;
  const int first = level == Level::Courses ? golfui::maxValue(nav.top, 1) : nav.top;
  for (int offset = 0; offset < nav.visibleRows && first + offset < listCount(); ++offset) {
    fillRow(offset, first + offset);
    golfui::drawCourseCard(screen, golfui::courseCard(body, offset), ACTION_ROW, first + offset,
                           first + offset == nav.selected, names[offset], details[offset], figures[offset],
                           units[offset], rows[offset].enabled);
  }
}
void GolfCoursePickerActivity::drawChrome() {
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, UITheme::getInstance().getMetrics().topPadding);
  golfui::drawHeader(renderer, chrome.header, headerTitle());
}
GolfPickerContext GolfCoursePickerActivity::context(uint16_t selected) const {
  return {mode, level, region, byRating, selected, initial.parentSelected, initial.skipHub};
}
void GolfCoursePickerActivity::openInfo(const GolfCourseFile& file) {
  if (mode == Mode::Play) {
    if (!CourseStore::load(file, scratch->course)) {
      LOG_ERR("GOLF", "Play picker: failed to load course");
      return;
    }
    golfSetSetupReturnContext(context(nav.selected));
    startGolfCourse(activityManager, renderer, mappedInput, file, scratch->course.nineCount);
    return;
  }
  golfSetSetupReturnContext(context(nav.selected));
  auto info = makeUniqueNoThrow<GolfCourseInfoActivity>(renderer, mappedInput, file, reopenInfo ? infoRow : 0);
  if (!info) {
    LOG_ERR("GOLF", "OOM: course info activity");
    return;
  }
  reopenInfo = false;
  startActivityForResult(std::move(info), nullptr);
}
void GolfCoursePickerActivity::openMyCourses(bool finishOnReturn) {
  if (mode == Mode::Play) {
    auto saved = context(0);
    saved.level = Level::MyCourses;
    saved.parentSelected = nav.selected;
    saved.skipHub = finishOnReturn;
    golfSetSetupReturnContext(saved);
    openGolfSetup(activityManager, renderer, mappedInput);
    return;
  }
  auto list = makeUniqueNoThrow<GolfCourseMapListActivity>(renderer, mappedInput);
  if (!list) {
    LOG_ERR("GOLF", "OOM: My courses activity");
    return;
  }
  auto saved = context(initial.level == Level::MyCourses ? initial.selected : 0);
  saved.level = Level::MyCourses;
  saved.parentSelected = initial.level == Level::MyCourses ? initial.parentSelected : nav.selected;
  saved.skipHub = finishOnReturn;
  list->setReturnContext(saved);
  if (reopenInfo) list->restoreInfo(infoFile, infoRow);
  reopenInfo = false;
  startActivityForResult(std::move(list), [this, finishOnReturn](const ActivityResult&) {
    if (finishOnReturn) onBackButton();
  });
}
void GolfCoursePickerActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (!scratch || index < 0 || index >= listCount()) return;
  if (level == Level::Regions) {
    {
      RenderLock lock(*this);
      region = index;
      byRating = true;
      loadRegion();
      enterLevel(Level::Courses);
    }
  } else if (level == Level::Courses) {
    if (!index) {
      RenderLock lock(*this);
      byRating = !byRating;
      sortRegion();
      enterLevel(Level::Courses);
      return;
    }
    GolfCourseFile file{};
    {
      RenderLock lock(*this);
      if (!readRecord(records[index - 1], scratch->a)) return;
      file.source = GolfCourseSource::Catalog;
      snprintf(file.filename, sizeof(file.filename), "bg-%lu.json", static_cast<unsigned long>(scratch->a.id));
    }
    openInfo(file);
  } else if (level == Level::Recent) {
    openInfo(recent[index]);
  } else {
    if (recentCount && index == 0) {
      RenderLock lock(*this);
      enterLevel(Level::Recent);
    } else if (index == (recentCount ? 1 : 0))
      openMyCourses();
    else {
      RenderLock lock(*this);
      enterLevel(Level::Regions);
    }
  }
}
void GolfCoursePickerActivity::onBackButton() {
  if (level == Level::Hub) {
    if (mode == Mode::Play || standalone) {
      openGolfHome(activityManager, renderer, mappedInput);
      return;
    }
    finish();
    return;
  }
  RenderLock lock(*this);
  enterLevel(level == Level::Courses ? Level::Regions : Level::Hub);
}
#endif
