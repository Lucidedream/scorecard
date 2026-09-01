#include "GolfHoleReviewActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfLargeNumber.h"
#include "GolfNavigation.h"
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

void GolfHoleReviewActivity::onEnter() {
  Activity::onEnter();
  if (playerSlot >= GolfRound::MAX_PLAYERS || !golfPlayerIsEnabled(round.players[playerSlot])) {
    LOG_ERR("GOLF", "Hole review selected invalid player slot %u", playerSlot);
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

void GolfHoleReviewActivity::changeHole(const int delta) {
  {
    RenderLock lock(*this);
    currentHole = static_cast<uint8_t>((currentHole + round.holeCount + delta) % round.holeCount);
  }
  requestUpdate();
}

void GolfHoleReviewActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeHole(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    changeHole(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    changeHole(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) changeHole(1);
}

void GolfHoleReviewActivity::drawHoleBand(const freeink::ui::Rect rect) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + 4, tr(STR_GOLF_HOLE), true,
                    EpdFontFamily::BOLD);
  const int digitHeight = golfui::clampValue(rect.height - lineHeight - 10, 24, 58);
  golfDrawLargeNumber(renderer, rect.x + rect.width / 4, rect.y + rect.height - digitHeight - 5, digitHeight,
                      currentHole + 1);

  const GolfPlayer& player = selectedPlayer();
  char line[32];
  const int right = rect.x + rect.width - padding;
  if (rect.height < lineHeight * 4 + 4) {
    if (round.par[currentHole] != 0) {
      snprintf(line, sizeof(line), tr(STR_GOLF_TEE_PAR_FORMAT), teeLabel(player.tee), tr(STR_GOLF_PAR),
               static_cast<unsigned>(round.par[currentHole]));
    } else {
      snprintf(line, sizeof(line), "%s", teeLabel(player.tee));
    }
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), rect.y + 4, line, true,
                      EpdFontFamily::BOLD);
    if (player.yards[currentHole] != 0 || round.hasSi) {
      if (player.yards[currentHole] != 0 && round.hasSi) {
        snprintf(line, sizeof(line), tr(STR_GOLF_DISTANCE_STROKE_INDEX_FORMAT),
                 static_cast<unsigned>(player.yards[currentHole]), tr(STR_GOLF_YARDS_UNIT),
                 tr(STR_GOLF_STROKE_INDEX), static_cast<unsigned>(round.si[currentHole]));
      } else if (player.yards[currentHole] != 0) {
        snprintf(line, sizeof(line), tr(STR_GOLF_DISTANCE_FORMAT),
                 static_cast<unsigned>(player.yards[currentHole]), tr(STR_GOLF_YARDS_UNIT));
      } else {
        snprintf(line, sizeof(line), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_STROKE_INDEX),
                 static_cast<unsigned>(round.si[currentHole]));
      }
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line),
                        rect.y + rect.height - lineHeight - 4, line);
    }
  } else {
    const int lineStep = golfui::clampValue((rect.height - 4) / 4, lineHeight, lineHeight + 6);
    int y = rect.y + 2;
    snprintf(line, sizeof(line), "%s", teeLabel(player.tee));
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), y, line, true,
                      EpdFontFamily::BOLD);
    y += lineStep;
    if (round.par[currentHole] != 0) {
      snprintf(line, sizeof(line), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_PAR),
               static_cast<unsigned>(round.par[currentHole]));
      renderer.drawText(UI_12_FONT_ID, right - renderer.getTextWidth(UI_12_FONT_ID, line), y, line, true,
                        EpdFontFamily::BOLD);
      y += lineStep;
    }
    if (player.yards[currentHole] != 0) {
      snprintf(line, sizeof(line), tr(STR_GOLF_DISTANCE_FORMAT),
               static_cast<unsigned>(player.yards[currentHole]), tr(STR_GOLF_YARDS_UNIT));
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), y, line);
      y += lineStep;
    }
    if (round.hasSi && y + lineHeight <= rect.y + rect.height) {
      snprintf(line, sizeof(line), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_STROKE_INDEX),
               static_cast<unsigned>(round.si[currentHole]));
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), y, line);
    }
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfHoleReviewActivity::drawScoreBand(const freeink::ui::Rect rect) const {
  const uint16_t score = golfHoleScore(round, selectedPlayer().score, currentHole);
  const bool hasPar = golfHasPar(round);
  const int offset = hasPar ? golfui::minValue(26, static_cast<int16_t>(rect.width / 12)) : 0;
  const int center = rect.x + rect.width / 2 - offset;
  const int digitHeight = golfui::clampValue(rect.height - 16, 32, 110);
  golfDrawLargeNumber(renderer, center, rect.y + (rect.height - digitHeight) / 2, digitHeight, score);
  if (hasPar) {
    char toPar[8];
    golfFormatReviewToPar(static_cast<int16_t>(score) - round.par[currentHole], tr(STR_GOLF_EVEN),
                          tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT), tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), toPar,
                          sizeof(toPar));
    const int textY = rect.y + (rect.height - renderer.getLineHeight(NOTOSANS_18_FONT_ID)) / 2;
    renderer.drawText(NOTOSANS_18_FONT_ID, center + digitHeight * 3 / 4, textY, toPar, true,
                      EpdFontFamily::BOLD);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfHoleReviewActivity::formatFieldMarkers(const GolfField field, char* output, const size_t size) const {
  output[0] = '\0';
  size_t used = 0;
  const GolfPlayerScore& score = selectedPlayer().score;
  const uint8_t count = score.penaltyCount[currentHole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? score.penaltyCount[currentHole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (!golfPenaltyEventAt(score, currentHole, index, event) || event.field != field) continue;
    const char* marker = event.kind == GolfPenaltyKind::Ob ? tr(STR_GOLF_OUT_OF_BOUNDS_TAG)
                                                            : tr(STR_GOLF_HAZARD_TAG);
    const int written = snprintf(output + used, size - used, "%s%s", used == 0 ? "" : " ", marker);
    if (written < 0 || static_cast<size_t>(written) >= size - used) break;
    used += static_cast<size_t>(written);
  }
}

void GolfHoleReviewActivity::drawDetailRow(const freeink::ui::Rect rect, const char* label, const uint16_t value,
                                           const GolfField field) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int right = rect.x + rect.width - padding;
  const int labelY = rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, labelY, label, true, EpdFontFamily::BOLD);
  char valueText[8];
  snprintf(valueText, sizeof(valueText), "%u", static_cast<unsigned>(value));
  const int valueFont = rect.height >= renderer.getLineHeight(NOTOSANS_18_FONT_ID) ? NOTOSANS_18_FONT_ID
                                                                                   : UI_12_FONT_ID;
  const int valueWidth = renderer.getTextWidth(valueFont, valueText, EpdFontFamily::BOLD);
  const int valueY = rect.y + (rect.height - renderer.getLineHeight(valueFont)) / 2;
  renderer.drawText(valueFont, right - valueWidth, valueY, valueText, true, EpdFontFamily::BOLD);
  char markers[32];
  formatFieldMarkers(field, markers, sizeof(markers));
  if (markers[0] != '\0') {
    const int markerRight = right - valueWidth - 18;
    renderer.drawText(UI_10_FONT_ID, markerRight - renderer.getTextWidth(UI_10_FONT_ID, markers, EpdFontFamily::BOLD),
                      labelY, markers, true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfHoleReviewActivity::drawPenaltyBand(const freeink::ui::Rect rect) const {
  const uint16_t strokes = golfPenaltyStrokesForHole(selectedPlayer().score, currentHole);
  if (strokes == 0 || rect.height <= 0) return;
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
  renderer.drawText(UI_12_FONT_ID, rect.x + padding, textY, tr(STR_GOLF_PENALTY), false,
                    EpdFontFamily::BOLD);
  char value[8];
  snprintf(value, sizeof(value), "+%u", static_cast<unsigned>(strokes));
  const int valueFont = rect.height >= renderer.getLineHeight(NOTOSANS_18_FONT_ID) ? NOTOSANS_18_FONT_ID
                                                                                   : UI_12_FONT_ID;
  renderer.drawText(valueFont, rect.x + rect.width - padding - renderer.getTextWidth(valueFont, value),
                    rect.y + (rect.height - renderer.getLineHeight(valueFont)) / 2, value, false,
                    EpdFontFamily::BOLD);
}

void GolfHoleReviewActivity::drawFooter() const {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), "", tr(STR_GOLF_BUTTON_PREVIOUS),
                                            tr(STR_GOLF_BUTTON_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfHoleReviewActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, metrics.topPadding, metrics.headerHeight);
  const GolfPlayerScore& score = selectedPlayer().score;
  const bool hasPenalty = golfPenaltyStrokesForHole(score, currentHole) > 0;
  const int fontMinimum = renderer.getLineHeight(UI_10_FONT_ID);
  const auto layout = golfui::makeHoleReviewLayout(chrome.body, hasPenalty, fontMinimum);
  GUI.drawHeader(renderer, Rect{chrome.header.x, chrome.header.y, chrome.header.width, chrome.header.height},
                 playerLabel, roundStatus);
  drawHoleBand(layout.hole);
  drawScoreBand(layout.score);
  drawDetailRow(layout.details[0], tr(STR_GOLF_PUTTS), score.putts[currentHole], GolfField::Putts);
  drawDetailRow(layout.details[1], tr(STR_GOLF_INSIDE_100), score.in100[currentHole], GolfField::In100);
  drawDetailRow(layout.details[2], tr(STR_GOLF_SCORING_ZONE), score.out100[currentHole], GolfField::Out100);
  drawPenaltyBand(layout.penalty);
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
