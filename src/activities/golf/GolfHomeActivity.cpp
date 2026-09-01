#include "GolfHomeActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include "GolfNavigation.h"
#include "GolfPlayerSelectActivity.h"
#include "GolfSetupActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"

namespace fui = freeink::ui;

void GolfHomeActivity::onEnter() {
  // The RAM marker is the commit authority. Never read an older unmarked
  // state.json over it; a failed cleanup may only be retried.
  const bool archiveMarkedAtEntry = GOLF_ROUND_STORE.isArchived();
  bool archiveMarkerSeen = archiveMarkedAtEntry;
  bool cleanupSucceeded = true;
  bool stateExists = false;
  bool loaded = false;
  if (archiveMarkedAtEntry) {
    cleanupSucceeded = GOLF_ROUND_STORE.clear();
  } else {
    stateExists = Storage.exists(GolfRoundStore::getFilePath());
    loaded = stateExists && GOLF_ROUND_STORE.loadFromFile();
    archiveMarkerSeen = GOLF_ROUND_STORE.isArchived();
    if (archiveMarkerSeen) cleanupSucceeded = GOLF_ROUND_STORE.clear();
  }

  const GolfHomeEntryDecision decision = golfDecideHomeEntry(archiveMarkerSeen, cleanupSucceeded, stateExists, loaded,
                                                             loaded ? GOLF_ROUND_STORE.getRound().holeCount : 0);
  hasOpenRound = decision.showResume;
  showNewRound = decision.showNew;
  stateError = decision.stateError;
  cleanupError = decision.cleanupOnly;

  uint8_t row = 0;
  if (hasOpenRound) rows[row++].label = tr(STR_GOLF_RESUME_ROUND);
  if (showNewRound) rows[row++].label = tr(STR_GOLF_NEW_ROUND);
  rows[row++].label = tr(STR_GOLF_HISTORY);
  rows[row++].label = tr(STR_GOLF_TRENDS);
  for (uint8_t i = 0; i < row; ++i) rows[i].actionValue = i;
  UiListActivity::onEnter();
}

const char* GolfHomeActivity::headerTitle() const {
  if (cleanupError) return tr(STR_GOLF_ARCHIVE_CLEANUP_ERROR);
  return stateError ? tr(STR_GOLF_STATE_ERROR) : tr(STR_GOLF_APP_TITLE);
}

void GolfHomeActivity::activateIndex(const int index) {
  app.clearTapFlash();
  int logical = index;
  if (hasOpenRound) {
    if (logical == 0) {
      openGolfScoring(activityManager, renderer, mappedInput);
      return;
    }
    --logical;
  }
  if (showNewRound) {
    if (logical == 0) {
      openGolfSetup(activityManager, renderer, mappedInput);
      return;
    }
    --logical;
  }
  if (logical < 0 || logical > 1) return;
  const auto mode = logical == 0 ? GolfPlayerSelectActivity::Mode::History : GolfPlayerSelectActivity::Mode::Trends;
  auto selector = makeUniqueNoThrow<GolfPlayerSelectActivity>(renderer, mappedInput, mode);
  if (!selector) {
    LOG_ERR("GOLF", "OOM: player selector activity");
    return;
  }
  startActivityForResult(std::move(selector), nullptr);
}

void GolfHomeActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = static_cast<uint16_t>(listCount());
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps);
  screen.list(listProps);
}

void GolfHomeActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
