#include "GolfPlayerSelectActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

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
  loadError = !success;
  for (uint8_t slot = 0; slot < ROW_COUNT; ++slot) {
    memcpy(playerNamesSnapshot[slot], playerNames.name(slot), sizeof(playerNamesSnapshot[slot]));
    golfFormatPlayerLabel(slot, playerNamesSnapshot[slot], tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabels[slot],
                          sizeof(playerLabels[slot]));
    if (success && playerNames.present(slot)) presentMask |= static_cast<uint8_t>(1U << slot);

    rows[slot] = {};
    rows[slot].label = playerLabels[slot];
    rows[slot].value = golfPlayerSelectSlotPresent(presentMask, slot)
                           ? nullptr
                           : (loadError ? tr(STR_GOLF_ROUNDS_UNAVAILABLE_SHORT) : tr(STR_GOLF_NO_ROUNDS_SHORT));
    rows[slot].actionValue = slot;
    rows[slot].enabled = golfPlayerSelectSlotPresent(presentMask, slot);
  }

  if (!golfPlayerSelectSlotPresent(presentMask, nav.selected)) {
    nav.reset(golfPlayerSelectFirstPresent(presentMask));
  } else {
    nav.follow(ROW_COUNT);
  }
  closeRouting();
  refreshPending.store(false, std::memory_order_release);
}

bool GolfPlayerSelectActivity::rowIsEnabled(const int index) const {
  return golfPlayerSelectSlotPresent(presentMask, index);
}

void GolfPlayerSelectActivity::onRowAction(const fui::ActionEvent& event) {
  if (!rowIsEnabled(event.value)) return;
  UiListActivity::onRowAction(event);
}

void GolfPlayerSelectActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (!rowIsEnabled(index)) return;

  std::unique_ptr<Activity> child;
  if (mode == Mode::History) {
    auto history = makeUniqueNoThrow<GolfHistoryActivity>(renderer, mappedInput, static_cast<uint8_t>(index),
                                                          playerNamesSnapshot[index]);
    if (!history) {
      LOG_ERR("GOLF", "OOM: history activity");
      return;
    }
    child = std::move(history);
  } else {
    auto trends = makeUniqueNoThrow<GolfTrendsActivity>(renderer, mappedInput, static_cast<uint8_t>(index),
                                                        playerNamesSnapshot[index]);
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

void GolfPlayerSelectActivity::navigateButtons() {
  int target = nav.selected;
  if (mappedInput.wasReleased(MappedInputManager::Button::NavNext)) {
    target = golfPlayerSelectNextPresent(presentMask, nav.selected, 1);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious)) {
    target = golfPlayerSelectNextPresent(presentMask, nav.selected, -1);
  }
  if (target != nav.selected) moveSelectionTo(target);
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

  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = ROW_COUNT;
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps);
  screen.list(listProps);
}

void GolfPlayerSelectActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

void GolfPlayerSelectActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), tr(STR_GOLF_BUTTON_SELECT),
                                            tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
