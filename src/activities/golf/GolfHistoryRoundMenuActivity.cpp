#include "GolfHistoryRoundMenuActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCardActivity.h"
#include "GolfHoleReviewActivity.h"
#include "GolfReviewFormat.h"
#include "GolfStatisticsActivity.h"
#include "GolfStrings.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfStats.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

GolfHistoryRoundMenuActivity::GolfHistoryRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                           const GolfRound& archivedRound, const char* filename)
    : UiListActivity("GolfHistoryRoundMenu", renderer, mappedInput), round(archivedRound) {
  if (filename != nullptr) snprintf(archiveFilename, sizeof(archiveFilename), "%s", filename);
}

void GolfHistoryRoundMenuActivity::onEnter() {
  rows[0].label = GolfStrings::SCORECARD;
  rows[1].label = GolfStrings::HOLE_BY_HOLE;
  rows[2].label = GolfStrings::STATISTICS;
  rows[3].label = GolfStrings::DELETE_ROUND;
  for (uint8_t i = 0; i < ROW_COUNT; ++i) {
    rows[i].value = GolfStrings::CHEVRON;
    rows[i].actionValue = i;
  }
  golfFormatRoundStatus(round, status, sizeof(status));
  snprintf(infoLine1, sizeof(infoLine1), GolfStrings::ROUND_INFO_HOLES_FORMAT, round.holeCount, round.tees);
  snprintf(infoLine2, sizeof(infoLine2), GolfStrings::ROUND_INFO_STATS_FORMAT, golfPuttsTotal(round),
           golfPenaltyStrokesForRound(round));
  UiListActivity::onEnter();
}

void GolfHistoryRoundMenuActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == 0) {
    auto card = makeUniqueNoThrow<GolfCardActivity>(renderer, mappedInput, round);
    if (!card) {
      LOG_ERR("GOLF", "OOM: archived card activity");
      return;
    }
    startActivityForResult(std::move(card), nullptr);
    return;
  }
  if (index == 1) {
    auto holes = makeUniqueNoThrow<GolfHoleReviewActivity>(renderer, mappedInput, round);
    if (!holes) {
      LOG_ERR("GOLF", "OOM: hole review activity");
      return;
    }
    startActivityForResult(std::move(holes), nullptr);
    return;
  }
  if (index == 2) {
    auto statistics = makeUniqueNoThrow<GolfStatisticsActivity>(renderer, mappedInput, round);
    if (!statistics) {
      LOG_ERR("GOLF", "OOM: statistics activity");
      return;
    }
    startActivityForResult(std::move(statistics), nullptr);
    return;
  }
  if (index == 3) confirmDelete();
}

void GolfHistoryRoundMenuActivity::confirmDelete() {
  deleteFailed = false;
  char date[GOLF_DATE_BUFFER_SIZE];
  if (golfFormatDate(round.dateYmd, date, sizeof(date))) {
    snprintf(deletePrompt, sizeof(deletePrompt), GolfStrings::DELETE_DATED_PROMPT, round.courseName, date);
  } else {
    snprintf(deletePrompt, sizeof(deletePrompt), GolfStrings::DELETE_UNDATED_PROMPT, round.courseName);
  }
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, GolfStrings::DELETE_ROUND, deletePrompt);
  if (!confirmation) {
    LOG_ERR("GOLF", "OOM: delete confirmation");
    return;
  }
  startActivityForResult(std::move(confirmation),
                         [this](const ActivityResult& result) { completeDelete(!result.isCancelled); });
}

void GolfHistoryRoundMenuActivity::completeDelete(const bool confirmed) {
  if (!confirmed) return;
  if (RoundArchive::remove(archiveFilename)) {
    finish();
    return;
  }
  deleteFailed = true;
  requestUpdate();
}

void GolfHistoryRoundMenuActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  fui::ListProps props;
  props.items = rows;
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  props.rowHeight = MENU_ROW_HEIGHT;
  props.rowGap = 0;
  props.scrollIndicator = false;
  screen.list(props, static_cast<int16_t>(ROW_COUNT * MENU_ROW_HEIGHT));

  auto infoStyle = screen.theme().smallText;
  infoStyle.align = fui::TextAlign::Left;
  const fui::Rect band = screen.takeTop(INFO_BAND_HEIGHT).inset(fui::Insets{8, 18, 8, 18});
  if (deleteFailed) {
    screen.target().text(band, GolfStrings::DELETE_ERROR, infoStyle);
    return;
  }
  const int16_t lineHeight = static_cast<int16_t>(band.height / 2);
  screen.target().text(fui::Rect{band.x, band.y, band.width, lineHeight}, infoLine1, infoStyle);
  screen.target().text(fui::Rect{band.x, static_cast<int16_t>(band.y + lineHeight), band.width, lineHeight}, infoLine2,
                       infoStyle);
}

void GolfHistoryRoundMenuActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 round.courseName, status);
}

void GolfHistoryRoundMenuActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::SELECT, GolfStrings::UP, GolfStrings::DOWN);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
