#include "GolfHomeActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Memory.h>

#include "GolfMessageActivity.h"
#include "GolfNavigation.h"
#include "GolfSetupActivity.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"

namespace fui = freeink::ui;

void GolfHomeActivity::onEnter() {
  const bool stateExists = Storage.exists(GolfRoundStore::getFilePath());
  const bool loaded = stateExists && GOLF_ROUND_STORE.loadFromFile();
  if (loaded && GOLF_ROUND_STORE.isArchived()) {
    GOLF_ROUND_STORE.clear();
  }
  hasOpenRound = loaded && !GOLF_ROUND_STORE.isArchived() &&
                 (GOLF_ROUND_STORE.getRound().holeCount == 9 || GOLF_ROUND_STORE.getRound().holeCount == 18);
  stateError = stateExists && !loaded;

  uint8_t row = 0;
  if (hasOpenRound) rows[row++].label = GolfStrings::RESUME_ROUND;
  rows[row++].label = GolfStrings::NEW_ROUND;
  rows[row++].label = GolfStrings::HISTORY;
  for (uint8_t i = 0; i < row; ++i) rows[i].actionValue = i;
  UiListActivity::onEnter();
}

const char* GolfHomeActivity::headerTitle() const {
  return stateError ? GolfStrings::STATE_ERROR : GolfStrings::APP_TITLE;
}

void GolfHomeActivity::activateIndex(const int index) {
  app.clearTapFlash();
  const int logical = index - (hasOpenRound ? 1 : 0);
  if (hasOpenRound && index == 0) {
    openGolfScoring(activityManager, renderer, mappedInput);
    return;
  }
  if (logical == 0) {
    openGolfSetup(activityManager, renderer, mappedInput);
    return;
  }
  auto message =
      makeUniqueNoThrow<GolfMessageActivity>(renderer, mappedInput, GolfStrings::HISTORY, GolfStrings::HISTORY_STUB);
  if (!message) {
    LOG_ERR("GOLF", "OOM: history message");
    return;
  }
  startActivityForResult(std::move(message), nullptr);
}

void GolfHomeActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

#endif
