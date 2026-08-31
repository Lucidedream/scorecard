#include "GolfHoleReviewActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>

#include <cstdio>
#include <cstring>

#include "GolfLargeNumber.h"
#include "GolfReviewFormat.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/CourseStore.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfStats.h"

void GolfHoleReviewActivity::onEnter() {
  Activity::onEnter();
  loadStrokeIndexes();
  golfFormatRoundStatus(round, roundStatus, sizeof(roundStatus));
  firstPaint = true;
  requestUpdate();
}

void GolfHoleReviewActivity::loadStrokeIndexes() {
  GolfCourse course{};
  if (!CourseStore::findByName(round.courseName, course) || !course.hasSi) return;
  memcpy(si, course.si, sizeof(si));
  hasSi = true;
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeHole(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) changeHole(1);
}

void GolfHoleReviewActivity::drawHoleBand(const int top) const {
  const int width = renderer.getScreenWidth();
  renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, top + 14, GolfStrings::HOLE, true, EpdFontFamily::BOLD);
  golfDrawLargeNumber(renderer, 92, top + 43, 58, currentHole + 1);

  char line[32];
  int y = top + 18;
  if (round.par[currentHole] != 0) {
    snprintf(line, sizeof(line), "%s %u", GolfStrings::PAR, round.par[currentHole]);
    renderer.drawText(UI_12_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(UI_12_FONT_ID, line), y, line, true,
                      EpdFontFamily::BOLD);
    y += 30;
  }
  if (round.yards[currentHole] != 0) {
    snprintf(line, sizeof(line), "%u %s", round.yards[currentHole], GolfStrings::YARDS);
    renderer.drawText(UI_10_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(UI_10_FONT_ID, line), y, line);
    y += 24;
  }
  if (hasSi) {
    snprintf(line, sizeof(line), "%s %u", GolfStrings::SI, si[currentHole]);
    renderer.drawText(UI_10_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(UI_10_FONT_ID, line), y, line);
  }
  renderer.drawLine(0, top + HOLE_BAND_HEIGHT - 1, width, top + HOLE_BAND_HEIGHT - 1);
}

void GolfHoleReviewActivity::drawScoreBand(const int top) const {
  const uint16_t score = golfHoleScore(round, currentHole);
  const bool hasPar = golfHasPar(round);
  const int center = hasPar ? renderer.getScreenWidth() / 2 - 26 : renderer.getScreenWidth() / 2;
  golfDrawLargeNumber(renderer, center, top + 31, 110, score);
  if (hasPar) {
    char toPar[8];
    golfFormatReviewToPar(static_cast<int16_t>(score) - round.par[currentHole], toPar, sizeof(toPar));
    renderer.drawText(NOTOSANS_18_FONT_ID, center + 82, top + 79, toPar, true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(0, top + SCORE_BAND_HEIGHT - 1, renderer.getScreenWidth(), top + SCORE_BAND_HEIGHT - 1);
}

void GolfHoleReviewActivity::formatFieldMarkers(const GolfField field, char* output, const size_t size) const {
  output[0] = '\0';
  size_t used = 0;
  const uint8_t count = round.penaltyCount[currentHole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? round.penaltyCount[currentHole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  for (uint8_t index = 0; index < count; ++index) {
    GolfPenaltyEvent event{};
    if (!golfPenaltyEventAt(round, currentHole, index, event) || event.field != field) continue;
    const char* marker = event.kind == GolfPenaltyKind::Ob ? GolfStrings::OB_TAG : GolfStrings::HAZARD_TAG;
    const int written = snprintf(output + used, size - used, "%s%s", used == 0 ? "" : " ", marker);
    if (written < 0 || static_cast<size_t>(written) >= size - used) break;
    used += static_cast<size_t>(written);
  }
}

void GolfHoleReviewActivity::drawDetailRow(const int top, const char* label, const uint16_t value,
                                           const GolfField field) const {
  const int width = renderer.getScreenWidth();
  renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, top + 24, label, true, EpdFontFamily::BOLD);
  char valueText[8];
  snprintf(valueText, sizeof(valueText), "%u", value);
  renderer.drawText(NOTOSANS_18_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(NOTOSANS_18_FONT_ID, valueText),
                    top + 17, valueText, true, EpdFontFamily::BOLD);
  char markers[32];
  formatFieldMarkers(field, markers, sizeof(markers));
  if (markers[0] != '\0') {
    const int valueWidth = renderer.getTextWidth(NOTOSANS_18_FONT_ID, valueText, EpdFontFamily::BOLD);
    const int markerRight = width - SIDE_PADDING - valueWidth - 18;
    renderer.drawText(UI_10_FONT_ID, markerRight - renderer.getTextWidth(UI_10_FONT_ID, markers, EpdFontFamily::BOLD),
                      top + 26, markers, true, EpdFontFamily::BOLD);
  }
  renderer.drawLine(0, top + DETAIL_ROW_HEIGHT - 1, width, top + DETAIL_ROW_HEIGHT - 1);
}

void GolfHoleReviewActivity::drawPenaltyBand(const int top) const {
  const uint16_t strokes = golfPenaltyStrokesForHole(round, currentHole);
  if (strokes == 0) return;
  const int width = renderer.getScreenWidth();
  renderer.fillRect(0, top, width, PENALTY_BAND_HEIGHT, true);
  renderer.drawText(UI_12_FONT_ID, SIDE_PADDING, top + 17, GolfStrings::PENALTY, false, EpdFontFamily::BOLD);
  char value[8];
  snprintf(value, sizeof(value), "+%u", strokes);
  renderer.drawText(NOTOSANS_18_FONT_ID, width - SIDE_PADDING - renderer.getTextWidth(NOTOSANS_18_FONT_ID, value),
                    top + 11, value, false, EpdFontFamily::BOLD);
}

void GolfHoleReviewActivity::drawFooter() const {
  const auto labels =
      mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::EMPTY, GolfStrings::LEFT, GolfStrings::RIGHT);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfHoleReviewActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::HOLE_BY_HOLE, roundStatus);
  int top = metrics.topPadding + metrics.headerHeight;
  drawHoleBand(top);
  top += HOLE_BAND_HEIGHT;
  drawScoreBand(top);
  top += SCORE_BAND_HEIGHT;
  drawDetailRow(top, GolfStrings::PUTTS, round.putts[currentHole], GolfField::Putts);
  top += DETAIL_ROW_HEIGHT;
  drawDetailRow(top, GolfStrings::IN100, round.in100[currentHole], GolfField::In100);
  top += DETAIL_ROW_HEIGHT;
  drawDetailRow(top, GolfStrings::SCORING_ZONE, round.out100[currentHole], GolfField::Out100);
  top += DETAIL_ROW_HEIGHT;
  drawPenaltyBand(top);
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
