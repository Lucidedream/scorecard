#include "GolfHistoryActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCardActivity.h"
#include "GolfRoundSummaryActivity.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "golf/GolfRoundFile.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

void formatToPar(const int16_t value, char* output, const size_t size) {
  if (value == 0) {
    snprintf(output, size, "%s", GolfStrings::EVEN);
  } else {
    snprintf(output, size, value > 0 ? "+%d" : "%d", value);
  }
}

}  // namespace

void GolfHistoryActivity::onEnter() {
  loadHistory();
  UiListActivity::onEnter();
}

void GolfHistoryActivity::logMalformed(const uint32_t lineNumber, void*) {
  LOG_ERR("GOLF", "Malformed index row at line %lu", static_cast<unsigned long>(lineNumber));
}

void GolfHistoryActivity::loadHistory() {
  history.reset();
  loadError = false;
  if (!Storage.exists(INDEX_PATH)) return;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) {
    loadError = true;
    return;
  }
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      loadError = true;
      break;
    }
    history.feed(chunk, static_cast<size_t>(bytesRead), &GolfHistoryActivity::logMalformed, this);
  }
  history.finish(&GolfHistoryActivity::logMalformed, this);
}

const char* GolfHistoryActivity::headerTitle() const {
  return history.overflowed() ? GolfStrings::HISTORY_LATEST : GolfStrings::HISTORY;
}

bool GolfHistoryActivity::loadArchivedRound(const uint8_t newestIndex, GolfRound& out) {
  if (!Storage.exists(INDEX_PATH)) return false;
  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;

  GolfIndexFileLocator locator;
  locator.reset(newestIndex, history.totalValidRows());
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) break;
    locator.feed(chunk, static_cast<size_t>(bytesRead));
  }
  if (!locator.finish()) return false;

  char path[sizeof("/golf/rounds/") + GOLF_ROUND_FILENAME_BUFFER_SIZE];
  snprintf(path, sizeof(path), "/golf/rounds/%s", locator.filename());
  return loadGolfRoundFile(path, out);
}

void GolfHistoryActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index < 0 || index >= history.count()) return;
  const uint8_t newestIndex = static_cast<uint8_t>(index);

  GolfRound round{};
  if (loadArchivedRound(newestIndex, round)) {
    auto card = makeUniqueNoThrow<GolfCardActivity>(renderer, mappedInput, round);
    if (!card) {
      LOG_ERR("GOLF", "OOM: card activity");
      return;
    }
    startActivityForResult(std::move(card), nullptr);
    return;
  }

  auto summary = makeUniqueNoThrow<GolfRoundSummaryActivity>(renderer, mappedInput, history.newest(newestIndex));
  if (!summary) {
    LOG_ERR("GOLF", "OOM: round summary");
    return;
  }
  startActivityForResult(std::move(summary), nullptr);
}

void GolfHistoryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (loadError || history.count() == 0) {
    screen.centeredText(loadError ? GolfStrings::HISTORY_ERROR : GolfStrings::NO_ROUNDS);
    return;
  }

  fui::ListProps props;
  props.count = history.count();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  const uint16_t first = props.topIndex;
  const uint16_t remaining = static_cast<uint16_t>(history.count() - first);
  const uint16_t count = remaining < WINDOW_ROWS ? remaining : WINDOW_ROWS;
  for (uint16_t windowIndex = 0; windowIndex < count; ++windowIndex) {
    const GolfHistoryEntry& entry = history.newest(static_cast<uint8_t>(first + windowIndex));
    visibleRows[windowIndex] = {};
    visibleRows[windowIndex].label = entry.course;
    visibleRows[windowIndex].actionValue = static_cast<int16_t>(first + windowIndex);
    if (golfHistoryShowsToPar(entry)) {
      char toPar[8];
      formatToPar(static_cast<int16_t>(entry.strokes) - entry.par, toPar, sizeof(toPar));
      snprintf(visibleValues[windowIndex], sizeof(visibleValues[windowIndex]), "%u  %s", entry.strokes, toPar);
    } else {
      snprintf(visibleValues[windowIndex], sizeof(visibleValues[windowIndex]), "%u", entry.strokes);
    }
    visibleRows[windowIndex].value = visibleValues[windowIndex];
  }
  props.items = visibleRows;
  props.itemsWindowFirst = first;
  props.itemsWindowCount = count;
  screen.list(props);
}

#endif
