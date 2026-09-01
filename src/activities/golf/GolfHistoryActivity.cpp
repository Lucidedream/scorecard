#include "GolfHistoryActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfHistoryRoundMenuActivity.h"
#include "GolfRoundSummaryActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/GolfRoundFile.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

void formatToPar(const int16_t value, char* output, const size_t size) {
  golfFormatReviewToPar(value, tr(STR_GOLF_EVEN), tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT),
                        tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), output, size);
}

}  // namespace

GolfHistoryActivity::GolfHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const uint8_t selectedPlayerSlot, const char* selectedFallbackName)
    : UiListActivity("GolfHistory", renderer, mappedInput), playerSlot(selectedPlayerSlot) {
  const char* source = selectedFallbackName;
  if (source == nullptr && playerSlot < GolfRound::MAX_PLAYERS) source = GOLF_DEFAULT_PLAYER_NAMES[playerSlot];
  if (source != nullptr) snprintf(fallbackName, sizeof(fallbackName), "%s", source);
}

void GolfHistoryActivity::onEnter() {
  golfFormatPlayerLabel(playerSlot, fallbackName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel,
                        sizeof(playerLabel));
  activeState = &residentState;
  lookupOwner = makeUniqueNoThrow<HistoryLookupScratch>();
  stagingState = lookupOwner ? &lookupOwner->staging : nullptr;
  if (!stagingState) {
    LOG_ERR("GOLF", "OOM: history lookup scratch (%u bytes)",
            static_cast<unsigned>(sizeof(HistoryLookupScratch)));
    showStagingError();
  } else {
    loadHistory();
  }
  UiListActivity::onEnter();
}

void GolfHistoryActivity::logMalformed(const uint32_t lineNumber, void*) {
  LOG_ERR("GOLF", "Malformed index row at line %lu", static_cast<unsigned long>(lineNumber));
}

bool GolfHistoryActivity::streamIndex(HistoryState& state) {
  if (!Storage.exists(INDEX_PATH)) return true;
  if (!lookupOwner) return false;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;
  bool success = true;
  while (file.available() > 0) {
    const int bytesRead = file.read(lookupOwner->chunk, sizeof(lookupOwner->chunk));
    if (bytesRead <= 0) {
      success = false;
      break;
    }
    state.history.feed(lookupOwner->chunk, static_cast<size_t>(bytesRead), &GolfHistoryActivity::logMalformed,
                       this);
  }
  state.history.finish(&GolfHistoryActivity::logMalformed, this);
  return success;
}

void GolfHistoryActivity::publishState() {
  RenderLock lock(*this);
  HistoryState* previous = activeState;
  activeState = stagingState;
  stagingState = previous;
  nav.reset();
  closeRouting();
}

void GolfHistoryActivity::showStagingError() {
  RenderLock lock(*this);
  activeState->history.reset(playerSlot);
  activeState->loadError = true;
  nav.reset();
  closeRouting();
}

void GolfHistoryActivity::loadHistory() {
  if (!stagingState) {
    showStagingError();
    return;
  }

  HistoryState& candidate = *stagingState;
  candidate.loadError = !candidate.history.reset(playerSlot);
  if (candidate.loadError) LOG_ERR("GOLF", "History received invalid player slot %u", playerSlot);
  if (!candidate.loadError && !RoundArchive::recoverIndex(lookupOwner->recovery)) {
    LOG_ERR("GOLF", "History refused unrecovered index.csv");
    candidate.loadError = true;
  }
  if (!candidate.loadError && !streamIndex(candidate)) candidate.loadError = true;
  publishState();
}

const char* GolfHistoryActivity::headerTitle() const {
  return activeState->history.overflowed() ? tr(STR_GOLF_HISTORY_LATEST) : tr(STR_GOLF_HISTORY);
}

bool GolfHistoryActivity::loadArchivedRound(const uint8_t newestIndex) {
  if (!lookupOwner || !RoundArchive::recoverIndex(lookupOwner->recovery)) {
    LOG_ERR("GOLF", "History selection refused unrecovered index.csv");
    {
      RenderLock lock(*this);
      activeState->loadError = true;
      closeRouting();
    }
    requestUpdate();
    return false;
  }
  if (!Storage.exists(INDEX_PATH)) return false;
  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;

  if (!lookupOwner->locator.reset(playerSlot, newestIndex, activeState->history.totalValidRows())) return false;
  while (file.available() > 0) {
    const int bytesRead = file.read(lookupOwner->chunk, sizeof(lookupOwner->chunk));
    if (bytesRead <= 0) break;
    lookupOwner->locator.feed(lookupOwner->chunk, static_cast<size_t>(bytesRead));
  }
  if (!lookupOwner->locator.finish() ||
      snprintf(lookupOwner->filename, sizeof(lookupOwner->filename), "%s", lookupOwner->locator.filename()) >=
          static_cast<int>(sizeof(lookupOwner->filename))) {
    return false;
  }

  snprintf(lookupOwner->path, sizeof(lookupOwner->path), "/golf/rounds/%s", lookupOwner->filename);
  if (!loadGolfRoundFile(lookupOwner->path, lookupOwner->round)) return false;
  return playerSlot < GolfRound::MAX_PLAYERS && golfPlayerIsEnabled(lookupOwner->round.players[playerSlot]);
}

void GolfHistoryActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index < 0 || index >= activeState->history.count()) return;
  const uint8_t newestIndex = static_cast<uint8_t>(index);
  const GolfHistoryEntry& selectedEntry = activeState->history.newest(newestIndex);

  if (loadArchivedRound(newestIndex)) {
    auto menu = makeUniqueNoThrow<GolfHistoryRoundMenuActivity>(
        renderer, mappedInput, lookupOwner->round, lookupOwner->filename, playerSlot);
    if (!menu) {
      LOG_ERR("GOLF", "OOM: history round menu activity");
      return;
    }
    startActivityForResult(std::move(menu), [this](const ActivityResult&) {
      loadHistory();
      requestUpdate();
    });
    return;
  }

  auto summary = makeUniqueNoThrow<GolfRoundSummaryActivity>(renderer, mappedInput, selectedEntry);
  if (!summary) {
    LOG_ERR("GOLF", "OOM: round summary");
    return;
  }
  startActivityForResult(std::move(summary), nullptr);
}

void GolfHistoryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding,
                                            metrics.headerHeight);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (activeState->loadError || activeState->history.count() == 0) {
    screen.centeredText(activeState->loadError ? tr(STR_GOLF_HISTORY_ERROR) : tr(STR_GOLF_NO_ROUNDS));
    return;
  }

  listProps = {};
  listProps.count = activeState->history.count();
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps, /*hasSubtitle=*/true);
  const uint16_t first = listProps.topIndex;
  const uint16_t remaining = static_cast<uint16_t>(activeState->history.count() - first);
  const uint16_t count = remaining < WINDOW_ROWS ? remaining : WINDOW_ROWS;
  for (uint16_t windowIndex = 0; windowIndex < count; ++windowIndex) {
    const GolfHistoryEntry& entry = activeState->history.newest(static_cast<uint8_t>(first + windowIndex));
    visibleRows[windowIndex] = {};
    visibleRows[windowIndex].label = entry.course;
    visibleDates[windowIndex][0] = '\0';
    if (golfFormatDate(entry.dateYmd, visibleDates[windowIndex], sizeof(visibleDates[windowIndex]))) {
      visibleRows[windowIndex].subtitle = visibleDates[windowIndex];
    }
    visibleRows[windowIndex].actionValue = static_cast<int16_t>(first + windowIndex);
    if (golfHistoryShowsToPar(entry)) {
      char toPar[8];
      formatToPar(static_cast<int16_t>(entry.strokes) - entry.par, toPar, sizeof(toPar));
      snprintf(visibleValues[windowIndex], sizeof(visibleValues[windowIndex]),
               tr(STR_GOLF_SCORE_TO_PAR_COMPACT_FORMAT), static_cast<unsigned>(entry.strokes), toPar);
    } else {
      snprintf(visibleValues[windowIndex], sizeof(visibleValues[windowIndex]), "%u",
               static_cast<unsigned>(entry.strokes));
    }
    visibleRows[windowIndex].value = visibleValues[windowIndex];
  }
  listProps.items = visibleRows;
  listProps.itemsWindowFirst = first;
  listProps.itemsWindowCount = count;
  screen.list(listProps);
}

void GolfHistoryActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding, metrics.headerHeight);
  GUI.drawHeader(renderer, Rect{layout.header.x, layout.header.y, layout.header.width, layout.header.height},
                 headerTitle(), playerLabel);
}

void GolfHistoryActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), tr(STR_GOLF_BUTTON_SELECT),
                                            tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
