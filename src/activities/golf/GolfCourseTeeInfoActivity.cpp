#include "GolfCourseTeeInfoActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfReviewFormat.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

GolfCourseTeeInfoActivity::GolfCourseTeeInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const char* courseName)
    : Activity("GolfCourseTeeInfo", renderer, mappedInput) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

void GolfCourseTeeInfoActivity::onEnter() {
  Activity::onEnter();
  resolved = CourseStore::resolveAllTees(courseName, teeSet);
  if (!resolved) LOG_ERR("GOLF", "Tee info: course not found: %s", courseName);
  holeCount = teeSet.primary.holeCount;
  activeTab = 0;
  firstPaint = true;
  renderScreen();
}

void GolfCourseTeeInfoActivity::changeTab(const int delta) {
  if (holeCount != 18) return;
  const uint8_t next = static_cast<uint8_t>((activeTab + 2 + delta) % 2);
  if (next == activeTab) return;
  activeTab = next;
  renderScreen();
}

void GolfCourseTeeInfoActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeTab(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    changeTab(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    changeTab(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    changeTab(1);
  }
}

void GolfCourseTeeInfoActivity::buildRows(const uint8_t firstHole) {
  for (auto& row : dataCells) {
    for (auto& cell : row) cell[0] = '\0';
  }
  for (auto& label : rowLabels) label = "";

  const GolfCourse& primary = teeSet.primary;
  const bool eighteen = holeCount == 18;

  // Row 0: hole numbers, then the nine's total-column header.
  rowLabels[0] = tr(STR_GOLF_HOLE);
  for (uint8_t i = 0; i < 9; ++i) {
    snprintf(dataCells[0][i], CELL_CAP, "%u", static_cast<unsigned>(firstHole + i + 1));
  }
  snprintf(dataCells[0][9], CELL_CAP, "%s",
           eighteen ? (firstHole == 0 ? tr(STR_GOLF_OUT) : tr(STR_GOLF_IN)) : tr(STR_GOLF_TOTAL_SHORT));

  uint8_t row = 1;

  // Par.
  rowLabels[row] = tr(STR_GOLF_PAR);
  uint16_t parTotal = 0;
  for (uint8_t i = 0; i < 9; ++i) {
    const uint8_t hole = static_cast<uint8_t>(firstHole + i);
    const uint8_t par = hole < primary.holeCount ? primary.par[hole] : 0;
    parTotal = static_cast<uint16_t>(parTotal + par);
    if (par != 0) snprintf(dataCells[row][i], CELL_CAP, "%u", static_cast<unsigned>(par));
  }
  if (parTotal != 0) snprintf(dataCells[row][9], CELL_CAP, "%u", static_cast<unsigned>(parTotal));
  ++row;

  // Stroke index, only when the course carries it (no dash row otherwise).
  if (primary.hasSi) {
    rowLabels[row] = tr(STR_GOLF_STROKE_INDEX);
    for (uint8_t i = 0; i < 9; ++i) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + i);
      if (hole < primary.holeCount) {
        snprintf(dataCells[row][i], CELL_CAP, "%u", static_cast<unsigned>(primary.si[hole]));
      }
    }
    ++row;
  }

  // One row per tee, in set order.
  for (uint8_t t = 0; t < teeSet.teeCount && row < MAX_ROWS; ++t) {
    const GolfCourseTee& tee = teeSet.tees[t];
    rowLabels[row] = golfTeeDisplayLabel(tee.name);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 9; ++i) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + i);
      if (tee.hasYards && hole < primary.holeCount) {
        sum += tee.yards[hole];
        snprintf(dataCells[row][i], CELL_CAP, "%u", static_cast<unsigned>(tee.yards[hole]));
      } else {
        snprintf(dataCells[row][i], CELL_CAP, "%s", tr(STR_GOLF_EM_DASH));
      }
    }
    if (tee.hasYards) snprintf(dataCells[row][9], CELL_CAP, "%lu", static_cast<unsigned long>(sum));
    ++row;
  }

  rowCount = row;
}

void GolfCourseTeeInfoActivity::drawTabs(const fui::Rect rect) const {
  const char* labels[2] = {tr(STR_GOLF_FRONT_NINE), tr(STR_GOLF_BACK_NINE)};
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int textY = rect.y + (rect.height - lineHeight) / 2;
  for (uint8_t i = 0; i < 2; ++i) {
    const int cellLeft = rect.x + static_cast<int32_t>(rect.width) * i / 2;
    const int cellRight = rect.x + static_cast<int32_t>(rect.width) * (i + 1) / 2;
    const bool active = activeTab == i;
    const EpdFontFamily::Style style = active ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int width = renderer.getTextWidth(UI_10_FONT_ID, labels[i], style);
    const int x = cellLeft + (cellRight - cellLeft - width) / 2;
    renderer.drawText(UI_10_FONT_ID, x, textY, labels[i], true, style);
    if (active) renderer.drawLine(x, textY + lineHeight, x + width, textY + lineHeight);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfCourseTeeInfoActivity::drawTable(const fui::Rect rect) const {
  if (rowCount == 0 || rect.height <= 0 || rect.width <= 0) return;
  const int16_t labelWidth =
      static_cast<int16_t>((static_cast<int32_t>(rect.width) * LABEL_COLUMN_UNITS) / (DATA_COLS + LABEL_COLUMN_UNITS));
  const int16_t dataLeft = static_cast<int16_t>(rect.x + labelWidth);
  const int16_t dataWidth = static_cast<int16_t>(rect.width - labelWidth);
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);

  for (uint8_t row = 0; row < rowCount; ++row) {
    const fui::Rect rowRect = golfui::evenRow(rect, rowCount, row);
    const bool header = row == 0;
    if (header) renderer.fillRect(rowRect.x, rowRect.y, rowRect.width, rowRect.height, true);
    const int textY = rowRect.y + (rowRect.height - lineHeight) / 2;
    const EpdFontFamily::Style style = header ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;

    const char* label = rowLabels[row];
    if (label != nullptr && label[0] != '\0') {
      renderer.drawText(UI_10_FONT_ID, rowRect.x + 4, textY, label, !header, EpdFontFamily::BOLD);
    }

    for (uint8_t col = 0; col < DATA_COLS; ++col) {
      const char* value = dataCells[row][col];
      if (value[0] == '\0') continue;
      const int cellLeft = dataLeft + static_cast<int32_t>(dataWidth) * col / DATA_COLS;
      const int cellRight = dataLeft + static_cast<int32_t>(dataWidth) * (col + 1) / DATA_COLS;
      const int width = renderer.getTextWidth(UI_10_FONT_ID, value, style);
      const int x = cellLeft + (cellRight - cellLeft - width) / 2;
      renderer.drawText(UI_10_FONT_ID, x, textY, value, !header, style);
    }

    if (!header) {
      renderer.drawLine(rowRect.x, rowRect.y + rowRect.height - 1, rowRect.x + rowRect.width - 1,
                        rowRect.y + rowRect.height - 1);
    }
  }
  renderer.drawLine(dataLeft, rect.y, dataLeft, rect.y + rect.height - 1);
}

void GolfCourseTeeInfoActivity::drawFooter() const {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHintsTwo(renderer, labels.btn1, labels.btn2);
}

void GolfCourseTeeInfoActivity::renderScreen() {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, chrome.header, tr(STR_GOLF_TEE_INFO_TITLE), courseName);
  drawFooter();

  const fui::Rect body = chrome.body;
  if (!resolved || teeSet.teeCount == 0) {
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawCenteredText(UI_12_FONT_ID, body.y + (body.height - lineHeight) / 2, tr(STR_GOLF_TEE_INFO_EMPTY),
                              true);
    renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
    firstPaint = false;
    return;
  }

  fui::Rect tableRect = body;
  if (holeCount == 18) {
    const int16_t tabHeight = static_cast<int16_t>(renderer.getLineHeight(UI_10_FONT_ID) + 12);
    drawTabs(fui::Rect{body.x, body.y, body.width, tabHeight});
    tableRect = fui::Rect{body.x, static_cast<int16_t>(body.y + tabHeight), body.width,
                          static_cast<int16_t>(body.height - tabHeight)};
  }

  const uint8_t firstHole = holeCount == 18 && activeTab == 1 ? 9 : 0;
  buildRows(firstHole);
  drawTable(tableRect);

  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
