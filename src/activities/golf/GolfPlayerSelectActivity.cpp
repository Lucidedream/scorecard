#include "GolfPlayerSelectActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <memory>

#include "GolfHistoryActivity.h"
#include "GolfPlayerSelectPolicy.h"
#include "GolfTrendsActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

}  // namespace

void GolfPlayerSelectActivity::onEnter() {
  UiListActivity::onEnter();
  refreshPending.store(true, std::memory_order_release);
  scanPlayers();
}

void GolfPlayerSelectActivity::loop() {
  if (refreshPending.load(std::memory_order_acquire)) {
    scanPlayers();
    return;
  }
  UiListActivity::loop();
}

void GolfPlayerSelectActivity::scanPlayers() {
  playerNames.reset();
  bool success = RoundArchive::recoverIndex(recovery);
  if (!success) {
    LOG_ERR("GOLF", "Player selector refused unrecovered index.csv");
  } else if (Storage.exists(INDEX_PATH)) {
    HalFile file;
    if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) {
      success = false;
    } else {
      while (file.available() > 0) {
        const int bytesRead = file.read(chunk, sizeof(chunk));
        if (bytesRead <= 0) {
          success = false;
          break;
        }
        playerNames.feed(chunk, static_cast<size_t>(bytesRead));
      }
      playerNames.finish();
    }
  }

  publishPlayers(success);
  requestUpdate();
}

void GolfPlayerSelectActivity::publishPlayers(const bool success) {
  if (!success) playerNames.reset();

  RenderLock lock(*this);
  presentMask = 0;
  rowCount = 0;
  loadError = !success;
  for (uint8_t slot = 0; slot < ROW_COUNT; ++slot) {
    memcpy(playerNamesSnapshot[slot], playerNames.name(slot), sizeof(playerNamesSnapshot[slot]));
    golfFormatPlayerLabel(slot, playerNamesSnapshot[slot], tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabels[slot],
                          sizeof(playerLabels[slot]));
    if (success && playerNames.present(slot)) presentMask |= static_cast<uint8_t>(1U << slot);

    if (!golfPlayerSelectSlotPresent(presentMask, slot)) continue;
    const uint8_t row = rowCount++;
    rowSlots[row] = slot;
    snprintf(roundCountLabels[row], sizeof(roundCountLabels[row]), tr(STR_GOLF_ROUND_COUNT_FORMAT),
             static_cast<unsigned long>(playerNames.roundCount(slot)));
    rows[row] = {};
    rows[row].label = playerLabels[slot];
    rows[row].subtitle = roundCountLabels[row];
    rows[row].actionValue = row;
  }

  if (nav.selected < 0 || nav.selected >= rowCount) nav.reset();
  nav.follow(rowCount);
  closeRouting();
  refreshPending.store(false, std::memory_order_release);
}

bool GolfPlayerSelectActivity::rowIsEnabled(const int index) const { return index >= 0 && index < rowCount; }

void GolfPlayerSelectActivity::onRowAction(const fui::ActionEvent& event) {
  if (!rowIsEnabled(event.value)) return;
  UiListActivity::onRowAction(event);
}

void GolfPlayerSelectActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (!rowIsEnabled(index)) return;
  const uint8_t slot = rowSlots[index];

  std::unique_ptr<Activity> child;
  if (mode == Mode::History) {
    auto history = makeUniqueNoThrow<GolfHistoryActivity>(renderer, mappedInput, slot, playerNamesSnapshot[slot]);
    if (!history) {
      LOG_ERR("GOLF", "OOM: history activity");
      return;
    }
    child = std::move(history);
  } else {
    auto trends = makeUniqueNoThrow<GolfTrendsActivity>(renderer, mappedInput, slot, playerNamesSnapshot[slot]);
    if (!trends) {
      LOG_ERR("GOLF", "OOM: trends activity");
      return;
    }
    child = std::move(trends);
  }

  {
    RenderLock lock(*this);
    refreshPending.store(true, std::memory_order_release);
    closeRouting();
  }
  startActivityForResult(std::move(child), nullptr);
}

const char* GolfPlayerSelectActivity::headerTitle() const {
  return mode == Mode::History ? tr(STR_GOLF_HISTORY) : tr(STR_GOLF_TRENDS);
}

void GolfPlayerSelectActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  if (refreshPending.load(std::memory_order_acquire)) {
    screen.centeredText(tr(STR_LOADING));
    return;
  }

  if (rowCount == 0) {
    const GolfPlayerSelectState state = golfPlayerSelectState(!loadError, presentMask);
    screen.centeredText(state == GolfPlayerSelectState::LoadError ? tr(STR_GOLF_ROUNDS_UNAVAILABLE_SHORT)
                                                                  : tr(STR_GOLF_NO_ROUNDS_YET));
    return;
  }

  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = rowCount;
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps, true);
  screen.list(listProps);
}

void GolfPlayerSelectActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

void GolfPlayerSelectActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), rowCount > 0 ? tr(STR_GOLF_OPEN) : "",
                                            rowCount > 1 ? tr(STR_GOLF_BUTTON_PREVIOUS) : "",
                                            rowCount > 1 ? tr(STR_GOLF_BUTTON_NEXT) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
