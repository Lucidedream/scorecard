#include "GolfStatisticsActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>

#include <cstdio>

#include "GolfReviewFormat.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfStats.h"

void GolfStatisticsActivity::onEnter() {
  Activity::onEnter();
  golfFormatRoundStatus(round, roundStatus, sizeof(roundStatus));
  firstPaint = true;
  requestUpdate();
}

void GolfStatisticsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void GolfStatisticsActivity::drawSection(const int top, const char* label) const {
  renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, top + 10, label, true, EpdFontFamily::BOLD);
  renderer.drawLine(0, top + SECTION_HEIGHT - 1, renderer.getScreenWidth(), top + SECTION_HEIGHT - 1);
}

void GolfStatisticsActivity::drawStat(const int top, const char* label, const uint16_t value,
                                      const bool withPercent) const {
  const int width = renderer.getScreenWidth();
  renderer.drawText(UI_12_FONT_ID, SIDE_PADDING, top + 20, label);
  char count[8];
  snprintf(count, sizeof(count), "%u", value);
  const int countRight = withPercent ? width - 98 : width - SIDE_PADDING;
  renderer.drawText(NOTOSANS_18_FONT_ID,
                    countRight - renderer.getTextWidth(NOTOSANS_18_FONT_ID, count, EpdFontFamily::BOLD), top + 14,
                    count, true, EpdFontFamily::BOLD);
  if (withPercent) {
    char percent[12];
    golfFormatReviewPercent(value, golfScore(round), percent, sizeof(percent));
    renderer.drawText(UI_10_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(UI_10_FONT_ID, percent), top + 23,
                      percent, true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(0, top + STAT_ROW_HEIGHT - 1, width, top + STAT_ROW_HEIGHT - 1);
}

void GolfStatisticsActivity::drawFooter() const {
  const auto labels =
      mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::BACK, GolfStrings::EMPTY, GolfStrings::EMPTY);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfStatisticsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::STATISTICS, roundStatus);
  int top = metrics.topPadding + metrics.headerHeight;
  drawSection(top, GolfStrings::WHERE_SHOTS_WENT);
  top += SECTION_HEIGHT;
  drawStat(top, GolfStrings::LONG_GAME, golfLongTotal(round), true);
  top += STAT_ROW_HEIGHT;
  drawStat(top, GolfStrings::SHORT_GAME, golfShortTotal(round), true);
  top += STAT_ROW_HEIGHT;
  drawStat(top, GolfStrings::PUTTING, golfPuttsTotal(round), true);
  top += STAT_ROW_HEIGHT;
  drawStat(top, GolfStrings::PENALTIES, golfPenaltyStrokesForRound(round), true);
  top += STAT_ROW_HEIGHT;
  drawSection(top, GolfStrings::PUTTING);
  top += SECTION_HEIGHT;
  drawStat(top, GolfStrings::ONE_PUTTS, golfOnePutts(round), false);
  top += STAT_ROW_HEIGHT;
  drawStat(top, GolfStrings::THREE_PUTTS, golfThreePutts(round), false);
  top += STAT_ROW_HEIGHT;
  drawSection(top, GolfStrings::PENALTIES);
  top += SECTION_HEIGHT;
  drawStat(top, GolfStrings::HAZARDS, golfHazardsForRound(round), false);
  top += STAT_ROW_HEIGHT;
  drawStat(top, GolfStrings::OUT_OF_BOUNDS, golfObsForRound(round), false);
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
