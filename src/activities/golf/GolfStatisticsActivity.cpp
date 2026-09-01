#include "GolfStatisticsActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfReviewFormat.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfStats.h"

namespace {

const char* teeLabel(const TeeSelection tee) {
  return tee == TeeSelection::White ? tr(STR_GOLF_WHITE) : tr(STR_GOLF_BLUE);
}

}  // namespace

void GolfStatisticsActivity::onEnter() {
  Activity::onEnter();
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot])) {
    LOG_ERR("GOLF", "Statistics selected invalid player slot %u", playerSlot);
    finish();
    return;
  }
  golfFormatPlayerLabel(playerSlot, selectedPlayer().name, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel,
                        sizeof(playerLabel));
  golfFormatRoundStatus(round, selectedPlayer().score, tr(STR_GOLF_EVEN), tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT),
                        tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), tr(STR_GOLF_ROUND_STATUS_FORMAT), roundStatus,
                        sizeof(roundStatus));
  firstPaint = true;
  requestUpdate();
}

void GolfStatisticsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void GolfStatisticsActivity::drawSection(const freeink::ui::Rect rect, const char* label,
                                         const char* rightLabel) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, textY, label, true, EpdFontFamily::BOLD);
  if (rightLabel != nullptr) {
    const int right = rect.x + rect.width - padding;
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, rightLabel), textY, rightLabel,
                      true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfStatisticsActivity::drawStat(const freeink::ui::Rect rect, const char* label, const uint16_t value,
                                      const bool withPercent) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int right = rect.x + rect.width - padding;
  const int labelY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, rect.x + padding, labelY, label);
  char count[8];
  snprintf(count, sizeof(count), "%u", static_cast<unsigned>(value));
  const int valueFont = rect.height >= renderer.getLineHeight(NOTOSANS_18_FONT_ID) ? NOTOSANS_18_FONT_ID
                                                                                   : UI_12_FONT_ID;
  const int countRight = withPercent ? right - 80 : right;
  renderer.drawText(valueFont, countRight - renderer.getTextWidth(valueFont, count, EpdFontFamily::BOLD),
                    rect.y + (rect.height - renderer.getLineHeight(valueFont)) / 2, count, true,
                    EpdFontFamily::BOLD);
  if (withPercent) {
    char percent[12];
    golfFormatReviewPercent(value, golfScore(round, selectedPlayer().score), tr(STR_GOLF_PERCENT_FORMAT), percent,
                            sizeof(percent));
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, percent),
                      rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, percent, true,
                      EpdFontFamily::BOLD);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfStatisticsActivity::drawFooter() const {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), tr(STR_GOLF_BUTTON_BACK), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfStatisticsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, metrics.topPadding, metrics.headerHeight);
  const auto layout = golfui::makeStatisticsLayout(chrome.body, renderer.getLineHeight(UI_10_FONT_ID));
  const GolfPlayer& player = selectedPlayer();
  const GolfPlayerScore& score = player.score;
  GUI.drawHeader(renderer, Rect{chrome.header.x, chrome.header.y, chrome.header.width, chrome.header.height},
                 playerLabel, roundStatus);
  drawSection(layout.rows[0], tr(STR_GOLF_WHERE_SHOTS_WENT), teeLabel(player.tee));
  drawStat(layout.rows[1], tr(STR_GOLF_LONG_GAME), golfLongTotal(round, score), true);
  drawStat(layout.rows[2], tr(STR_GOLF_SHORT_GAME), golfShortTotal(round, score), true);
  drawStat(layout.rows[3], tr(STR_GOLF_PUTTING), golfPuttsTotal(round, score), true);
  drawStat(layout.rows[4], tr(STR_GOLF_PENALTIES), golfPenaltyStrokesForRound(score, round.holeCount), true);
  drawSection(layout.rows[5], tr(STR_GOLF_PUTTING));
  drawStat(layout.rows[6], tr(STR_GOLF_ONE_PUTTS), golfOnePutts(round, score), false);
  drawStat(layout.rows[7], tr(STR_GOLF_THREE_PUTTS), golfThreePutts(round, score), false);
  drawSection(layout.rows[8], tr(STR_GOLF_PENALTIES));
  drawStat(layout.rows[9], tr(STR_GOLF_HAZARDS), golfHazardsForRound(score, round.holeCount), false);
  drawStat(layout.rows[10], tr(STR_GOLF_OUT_OF_BOUNDS), golfObsForRound(score, round.holeCount), false);
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
