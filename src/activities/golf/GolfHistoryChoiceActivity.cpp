#include "GolfHistoryChoiceActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "GolfHistoryActivity.h"
#include "GolfTrendsActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

GolfHistoryChoiceActivity::GolfHistoryChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const uint8_t playerSlot, const char* playerName)
    : UiListActivity("GolfHistoryChoice", renderer, mappedInput), playerSlot(playerSlot) {
  snprintf(this->playerName, sizeof(this->playerName), "%s", playerName != nullptr ? playerName : "");
}

void GolfHistoryChoiceActivity::onEnter() {
  golfFormatPlayerLabel(playerSlot, playerName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel, sizeof(playerLabel));
  rows[0].label = tr(STR_GOLF_TRENDS);
  rows[1].label = tr(STR_GOLF_ROUNDS);
  for (uint8_t i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = i;
  UiListActivity::onEnter();
}

const char* GolfHistoryChoiceActivity::headerTitle() const { return playerLabel; }

void GolfHistoryChoiceActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    auto trends = makeUniqueNoThrow<GolfTrendsActivity>(renderer, mappedInput, playerSlot, playerName);
    if (!trends) {
      LOG_ERR("GOLF", "OOM: trends activity");
      return;
    }
    startActivityForResult(std::move(trends), nullptr);
    return;
  }
  if (index == 1) {
    auto history = makeUniqueNoThrow<GolfHistoryActivity>(renderer, mappedInput, playerSlot, playerName);
    if (!history) {
      LOG_ERR("GOLF", "OOM: history activity");
      return;
    }
    startActivityForResult(std::move(history), nullptr);
    return;
  }
}

void GolfHistoryChoiceActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  listProps = {};
  listProps.items = rows;
  listProps.count = ROW_COUNT;
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  syncListViewport(screen, listProps);
  screen.list(listProps);
}

void GolfHistoryChoiceActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

void GolfHistoryChoiceActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
