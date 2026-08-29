#include "GolfRoundMenuActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <Memory.h>

#include "GolfMessageActivity.h"
#include "GolfNavigation.h"
#include "GolfStrings.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "golf/GolfRoundStore.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

void GolfRoundMenuActivity::onEnter() {
  rows[0].label = GolfStrings::VIEW_CARD;
  rows[1].label = GolfStrings::FINISH_ROUND;
  rows[2].label = GolfStrings::ABANDON_ROUND;
  for (uint8_t i = 0; i < 3; ++i) rows[i].actionValue = i;
  UiListActivity::onEnter();
}

const char* GolfRoundMenuActivity::headerTitle() const {
  return errorMessage == nullptr ? GolfStrings::APP_TITLE : errorMessage;
}

void GolfRoundMenuActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    auto message =
        makeUniqueNoThrow<GolfMessageActivity>(renderer, mappedInput, GolfStrings::VIEW_CARD, GolfStrings::CARD_STUB);
    if (!message) {
      LOG_ERR("GOLF", "OOM: card message");
      return;
    }
    startActivityForResult(std::move(message), nullptr);
    return;
  }
  confirmAction(index == 1 ? PendingAction::Finish : PendingAction::Abandon);
}

void GolfRoundMenuActivity::confirmAction(const PendingAction action) {
  pendingAction = action;
  const char* heading = action == PendingAction::Finish ? GolfStrings::FINISH_ROUND : GolfStrings::ABANDON_ROUND;
  const char* body = action == PendingAction::Finish ? GolfStrings::FINISH_PROMPT : GolfStrings::ABANDON_PROMPT;
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
  bool success = false;
  if (pendingAction == PendingAction::Finish) {
    success = RoundArchive::archive(GOLF_ROUND_STORE.getRound());
    errorMessage = success ? nullptr : GolfStrings::ARCHIVE_ERROR;
  } else if (pendingAction == PendingAction::Abandon) {
    success = GOLF_ROUND_STORE.clear();
    errorMessage = success ? nullptr : GolfStrings::ABANDON_ERROR;
  }
  pendingAction = PendingAction::None;
  if (success) {
    openGolfHome(activityManager, renderer, mappedInput);
  } else {
    requestUpdate();
  }
}

void GolfRoundMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = 3;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

#endif
