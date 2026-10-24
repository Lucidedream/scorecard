#include "GolfCoursePickerActivity.h"
#if defined(CROSSPOINT_GOLF)
#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfCourseInfoActivity.h"
#include "GolfCourseMapListActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;
void GolfCoursePickerActivity::onEnter() {
  // Course and streaming buffers exceed the task stack budget; allocate once per visit.
  scratch = makeUniqueNoThrow<Scratch>();
  if (!scratch) LOG_ERR("GOLF", "OOM: catalog picker scratch");
  hasCatalog = scratch && loadCatalog();
  if (scratch) loadRecent();
  UiListActivity::onEnter();
  {
    RenderLock lock(*this);
    enterLevel(Level::Hub);
  }
  if (!hasCatalog && !recentCount) openMyCourses(true);
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
  return (recentCount ? recentCount + 1 : 0) + 1 + (hasCatalog ? 1 : 0);
}
const char* GolfCoursePickerActivity::headerTitle() const {
  if (level == Level::Regions) return tr(STR_GOLF_BY_REGION);
  if (level == Level::Courses) return scratch->catalog.regions[region].name;
  return tr(STR_GOLF_COURSE_MAP);
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
void GolfCoursePickerActivity::fillRow(uint8_t slot, uint16_t absolute) {
  auto& row = rows[slot];
  row = {};
  row.actionValue = absolute;
  row.label = names[slot];
  row.subtitle = details[slot];
  names[slot][0] = details[slot][0] = 0;
  if (level == Level::Regions) {
    const auto& r = scratch->catalog.regions[absolute];
    snprintf(names[slot], sizeof(names[slot]), "%s", r.name);
    snprintf(details[slot], sizeof(details[slot]), tr(STR_GOLF_CATALOG_COUNT_FORMAT), r.count);
  } else if (level == Level::Courses) {
    if (!absolute) {
      row.label = byRating ? tr(STR_GOLF_SORT_RATING) : tr(STR_GOLF_SORT_NAME);
      return;
    }
    if (!readRecord(records[absolute - 1], scratch->a)) {
      row.enabled = false;
      return;
    }
    const auto& r = scratch->a;
    memcpy(names[slot], r.name, sizeof(r.name));
    char rating[40];
    if (r.hasRating)
      snprintf(rating, sizeof(rating), tr(STR_GOLF_CATALOG_RATING_FORMAT), r.rating / 10, r.rating % 10,
               static_cast<unsigned long>(r.reviews));
    else
      snprintf(rating, sizeof(rating), "%s", tr(STR_GOLF_NO_RATING));
    snprintf(details[slot], sizeof(details[slot]), tr(STR_GOLF_CATALOG_ROW_FORMAT), rating, r.nineCount * 9,
             r.hasMaps ? "" : tr(STR_GOLF_NO_MAPS));
  } else {
    if (recentCount && !absolute) {
      row.label = tr(STR_GOLF_RECENT);
      row.isHeader = true;
      row.enabled = false;
      return;
    }
    if (recentCount && absolute <= recentCount) {
      if (CourseStore::load(recent[absolute - 1], scratch->course))
        memcpy(names[slot], scratch->course.courseName, sizeof(names[slot]));
      else
        row.enabled = false;
      return;
    }
    const int offset = absolute - (recentCount ? recentCount + 1 : 0);
    row.label = offset ? tr(STR_GOLF_BY_REGION) : tr(STR_GOLF_MY_COURSES);
  }
}
void GolfCoursePickerActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  if (hasCatalog) {
    const auto credit = screen.takeBottom(screen.target().lineHeight(screen.theme().smallText.font));
    screen.target().text(credit, tr(STR_GOLF_DATA_BAIGOLF), screen.theme().smallText);
  }
  props = {};
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.rowHeight = 96;
  syncListViewport(screen, props, true);
  props.itemsWindowFirst = props.topIndex;
  props.itemsWindowCount = static_cast<uint16_t>(
      golfui::minValue(PAGE_ROWS, golfui::minValue(nav.visibleRows, listCount() - props.topIndex)));
  memset(names, 0, sizeof(names));
  memset(details, 0, sizeof(details));
  for (uint8_t i = 0; i < props.itemsWindowCount; ++i) fillRow(i, props.topIndex + i);
  props.items = rows;
  screen.list(props);
}
void GolfCoursePickerActivity::drawChrome() {
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, UITheme::getInstance().getMetrics().topPadding);
  golfui::drawHeader(renderer, chrome.header, headerTitle());
}
void GolfCoursePickerActivity::openInfo(const GolfCourseFile& file) {
  auto info = makeUniqueNoThrow<GolfCourseInfoActivity>(renderer, mappedInput, file);
  if (!info) {
    LOG_ERR("GOLF", "OOM: course info activity");
    return;
  }
  startActivityForResult(std::move(info), nullptr);
}
void GolfCoursePickerActivity::openMyCourses(bool finishOnReturn) {
  auto list = makeUniqueNoThrow<GolfCourseMapListActivity>(renderer, mappedInput);
  if (!list) {
    LOG_ERR("GOLF", "OOM: My courses activity");
    return;
  }
  startActivityForResult(std::move(list), [this, finishOnReturn](const ActivityResult&) {
    if (finishOnReturn) finish();
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
  } else {
    if (recentCount && !index) return;
    if (recentCount && index <= recentCount) {
      openInfo(recent[index - 1]);
      return;
    }
    if (index == (recentCount ? recentCount + 1 : 0))
      openMyCourses();
    else {
      RenderLock lock(*this);
      enterLevel(Level::Regions);
    }
  }
}
void GolfCoursePickerActivity::onBackButton() {
  if (level == Level::Hub) {
    finish();
    return;
  }
  RenderLock lock(*this);
  enterLevel(level == Level::Courses ? Level::Regions : Level::Hub);
}
#endif
