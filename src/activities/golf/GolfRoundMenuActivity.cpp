#include "GolfRoundMenuActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Memory.h>

#include "GolfCardActivity.h"
#include "GolfNavigation.h"
#include "GolfRoundExportActivity.h"
#include "GolfTipListActivity.h"
#include "GolfUiLayout.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"

namespace fui = freeink::ui;

void GolfRoundMenuActivity::onEnter() {
  rows[0].label = tr(STR_GOLF_VIEW_CARD);
  rows[1].label = tr(STR_GOLF_ABANDON_ROUND);
  rows[2].label = tr(STR_GOLF_FINISH_ROUND);
  rows[3].label = tr(STR_GOLF_EXPORT_SEND);
  rows[4].label = tr(STR_GOLF_TIPS);
  for (uint8_t i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = i;
  UiListActivity::onEnter();
}

const char* GolfRoundMenuActivity::headerTitle() const {
  return errorMessage == nullptr ? tr(STR_GOLF_APP_TITLE) : errorMessage;
}

void GolfRoundMenuActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    auto card = makeUniqueNoThrow<GolfCardActivity>(renderer, mappedInput);
    if (!card) {
      LOG_ERR("GOLF", "OOM: card activity");
      return;
    }
    startActivityForResult(std::move(card), nullptr);
    return;
  }
  if (index == 3) {
    const auto& round = GOLF_ROUND_STORE.getRound();
    auto transfer =
        makeUniqueNoThrow<GolfRoundExportActivity>(renderer, mappedInput, round, round.currentPlayer, false);
    if (!transfer) {
      LOG_ERR("GOLF", "OOM: export activity");
      {
        RenderLock lock(*this);
        errorMessage = tr(STR_GOLF_EXPORT_ERROR);
      }
      requestUpdate();
      return;
    }
    startActivityForResult(std::move(transfer), nullptr);
    return;
  }
  if (index == 4) {
    auto tips = makeUniqueNoThrow<GolfTipListActivity>(renderer, mappedInput);
    if (!tips) {
      LOG_ERR("GOLF", "OOM: tip list activity");
      return;
    }
    startActivityForResult(std::move(tips), nullptr);
    return;
  }
  confirmAction(index == 2 ? PendingAction::Finish : PendingAction::Abandon);
}

void GolfRoundMenuActivity::confirmAction(const PendingAction action) {
  pendingAction = action;
  const char* heading = action == PendingAction::Finish ? tr(STR_GOLF_FINISH_ROUND) : tr(STR_GOLF_ABANDON_ROUND);
  const char* body = action == PendingAction::Finish ? tr(STR_GOLF_FINISH_PROMPT) : tr(STR_GOLF_ABANDON_PROMPT);
  auto confirmation = makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, heading, body);
  if (!confirmation) {
    LOG_ERR("GOLF", "OOM: round confirmation");
    pendingAction = PendingAction::None;
    return;
  }
  startActivityForResult(std::move(confirmation),
                         [this](const ActivityResult& result) { completeAction(!result.isCancelled); });
}

void GolfRoundMenuActivity::completeAction(const bool confirmed) {
  if (!confirmed) {
    pendingAction = PendingAction::None;
    return;
  }

  if (pendingAction == PendingAction::Finish) {
    pendingAction = PendingAction::None;
    if (!finishGolfRound(activityManager, renderer, mappedInput)) {
      errorMessage = tr(STR_GOLF_ARCHIVE_ERROR);
      requestUpdate();
    }
    return;
  }

  const bool success = pendingAction == PendingAction::Abandon && GOLF_ROUND_STORE.clear();
  pendingAction = PendingAction::None;
  errorMessage = success ? nullptr : tr(STR_GOLF_ABANDON_ERROR);
  if (success) {
    clearGolfRoundDirty();
    openGolfHome(activityManager, renderer, mappedInput);
  } else {
    requestUpdate();
  }
}

void GolfRoundMenuActivity::buildScreen(UiScreen& screen) {
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

void GolfRoundMenuActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, headerTitle());
}

#endif
