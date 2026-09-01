#include "GolfHistoryRoundMenuActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCardActivity.h"
#include "GolfHoleReviewActivity.h"
#include "GolfReviewFormat.h"
#include "GolfStatisticsActivity.h"
#include "GolfUiLayout.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

const char* teeLabel(const TeeSelection tee) {
  return tee == TeeSelection::White ? tr(STR_GOLF_WHITE) : tr(STR_GOLF_BLUE);
}

}  // namespace

GolfHistoryRoundMenuActivity::GolfHistoryRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                           const GolfRound& archivedRound, const char* filename,
                                                           const uint8_t selectedPlayerSlot)
    : UiListActivity("GolfHistoryMenu", renderer, mappedInput), round(archivedRound), playerSlot(selectedPlayerSlot) {
  if (filename != nullptr) snprintf(archiveFilename, sizeof(archiveFilename), "%s", filename);
}

void GolfHistoryRoundMenuActivity::onEnter() {
  rows[0].label = tr(STR_GOLF_APP_TITLE);
  rows[1].label = tr(STR_GOLF_HOLE_BY_HOLE);
  rows[2].label = tr(STR_GOLF_STATISTICS);
  rows[3].label = tr(STR_GOLF_DELETE_ROUND);
  for (uint8_t i = 0; i < ROW_COUNT; ++i) {
    rows[i].value = ">";
    rows[i].actionValue = i;
  }
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot])) {
    LOG_ERR("GOLF", "Archived menu selected invalid player slot %u", playerSlot);
    finish();
    return;
  }
  const GolfPlayer& player = selectedPlayer();
  golfFormatRoundStatus(round, player.score, tr(STR_GOLF_EVEN), tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT),
                        tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), tr(STR_GOLF_ROUND_STATUS_FORMAT), status, sizeof(status));
  golfFormatPlayerLabel(playerSlot, player.name, tr(STR_GOLF_PLAYER_LABEL_FORMAT), infoLine1, sizeof(infoLine1));
  snprintf(infoLine2, sizeof(infoLine2), tr(STR_GOLF_ROUND_INFO_HOLES_FORMAT), static_cast<unsigned>(round.holeCount),
           teeLabel(player.tee));
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
    auto holes = makeUniqueNoThrow<GolfHoleReviewActivity>(renderer, mappedInput, round, playerSlot);
    if (!holes) {
      LOG_ERR("GOLF", "OOM: hole review activity");
      return;
    }
    startActivityForResult(std::move(holes), nullptr);
    return;
  }
  if (index == 2) {
    auto statistics = makeUniqueNoThrow<GolfStatisticsActivity>(renderer, mappedInput, round, playerSlot);
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
    snprintf(deletePrompt, sizeof(deletePrompt), tr(STR_GOLF_DELETE_DATED_PROMPT_FORMAT), round.courseName, date);
  } else {
    snprintf(deletePrompt, sizeof(deletePrompt), tr(STR_GOLF_DELETE_UNDATED_PROMPT_FORMAT), round.courseName);
  }
  auto confirmation =
      makeUniqueNoThrow<ConfirmationActivity>(renderer, mappedInput, tr(STR_GOLF_DELETE_ROUND), deletePrompt);
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
  const auto chrome = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);

  const int16_t smallLine = screen.target().lineHeight(screen.theme().smallText.font);
  const int16_t bodyLine = screen.target().lineHeight(screen.theme().bodyText.font);
  const int16_t rowMinimum = bodyLine > smallLine ? bodyLine : smallLine;
  const auto layout = golfui::makeMenuInfoLayout(screen.body(), ROW_COUNT, rowMinimum, smallLine * 2 + 16);
  screen.takeBottom(layout.info.height);

  listProps = {};
  listProps.items = rows;
  listProps.count = ROW_COUNT;
  listProps.action = ACTION_ROW;
  listProps.inputMask = fui::InputTouch;
  listProps.rowHeight = layout.rowHeight;
  listProps.rowGap = 0;
  listProps.scrollIndicator = false;
  syncListViewport(screen, listProps);
  screen.list(listProps, layout.menu.height);

  auto infoStyle = screen.theme().smallText;
  infoStyle.align = fui::TextAlign::Left;
  const fui::Rect band = golfui::inset(layout.info, fui::Insets{8, 18, 8, 18});
  if (deleteFailed) {
    screen.target().text(band, tr(STR_GOLF_DELETE_ERROR), infoStyle);
    return;
  }
  const int16_t lineHeight = static_cast<int16_t>(band.height / 2);
  screen.target().text(fui::Rect{band.x, band.y, band.width, lineHeight}, infoLine1, infoStyle);
  screen.target().text(fui::Rect{band.x, static_cast<int16_t>(band.y + lineHeight), band.width, lineHeight}, infoLine2,
                       infoStyle);
}

void GolfHistoryRoundMenuActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, round.courseName, status);
}

void GolfHistoryRoundMenuActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), tr(STR_GOLF_BUTTON_SELECT),
                                            tr(STR_GOLF_BUTTON_UP), tr(STR_GOLF_BUTTON_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

#endif
