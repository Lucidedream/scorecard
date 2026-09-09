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
    : Activity("GolfCourseTeeInfo", renderer, mappedInput), UiAppHost(renderer) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

void GolfCourseTeeInfoActivity::onEnter() {
  Activity::onEnter();
  resolved = CourseStore::resolveAllTees(courseName, teeSet);
  if (!resolved) LOG_ERR("GOLF", "Tee info: course not found: %s", courseName);
  holeCount = teeSet.primary.holeCount;
  activeTab = 0;
  firstPaint = true;
  resetUi();
  if (resolved && teeSet.teeCount > 0) {
    app.on(ACTION_TAB, &GolfCourseTeeInfoActivity::tabTrampoline, this);
    app.setScreen(&GolfCourseTeeInfoActivity::screenTrampoline, this);
  }
  requestUpdate();
}

void GolfCourseTeeInfoActivity::changeTab(const int delta) {
  const uint8_t count = holeCount == 18 ? 2 : 1;
  if (count == 1) return;
  {
    RenderLock lock(*this);
    activeTab = static_cast<uint8_t>((activeTab + count + delta) % count);
  }
  requestUpdate();
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
    return;
  }
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
}

void GolfCourseTeeInfoActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfCourseTeeInfoActivity*>(user)->buildScreen(screen);
}

void GolfCourseTeeInfoActivity::tabTrampoline(const fui::ActionEvent& event, void* user) {
  if (event.value < 0 || event.value > 1) return;
  auto* self = static_cast<GolfCourseTeeInfoActivity*>(user);
  {
    RenderLock lock(*self);
    self->activeTab = static_cast<uint8_t>(event.value);
  }
  self->app.clearTapFlash();
  self->requestUpdate();
}

void GolfCourseTeeInfoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);

  const fui::Rect body = screen.body();
  fui::Rect tableRect = body;
  if (holeCount == 18) {
    segmentTabs[0] = {tr(STR_GOLF_FRONT_NINE), {}, {}, 0, activeTab == 0, true};
    segmentTabs[1] = {tr(STR_GOLF_BACK_NINE), {}, {}, 1, activeTab == 1, true};
    tabProps = {};
    tabProps.tabs = segmentTabs;
    tabProps.count = 2;
    tabProps.action = ACTION_TAB;
    tabProps.inputMask = fui::InputTouch;
    tabProps.text = screen.theme().smallText;
    tabProps.divider = true;
    const int16_t tabHeight = static_cast<int16_t>(screen.target().lineHeight(screen.theme().smallText.font) + 12);
    fui::tabBar(screen.frame(), fui::Rect{body.x, body.y, body.width, tabHeight}, tabProps);
    tableRect = fui::Rect{body.x, static_cast<int16_t>(body.y + tabHeight), body.width,
                          static_cast<int16_t>(body.height - tabHeight)};
  }

  const uint8_t firstHole = holeCount == 18 && activeTab == 1 ? 9 : 0;
  const char* totalHeader =
      holeCount == 18 ? (firstHole == 0 ? tr(STR_GOLF_OUT) : tr(STR_GOLF_IN)) : tr(STR_GOLF_TOTAL_SHORT);
  buildCard(screen, tableRect, firstHole, totalHeader);
}

void GolfCourseTeeInfoActivity::buildCard(UiScreen& screen, const fui::Rect tableRect, const uint8_t firstHole,
                                          const char* segmentLabel) {
  const GolfCourse& primary = teeSet.primary;
  const bool hasSiRow = primary.hasSi;

  for (uint8_t row = 0; row < MAX_TABLE_ROWS; ++row) {
    for (uint8_t column = 0; column < DATA_COLS; ++column) dataCells[row][column][0] = '\0';
  }

  // Row 0: hole numbers, then the nine's total-column header.
  for (uint8_t offset = 0; offset < 9; ++offset) {
    snprintf(dataCells[0][offset], CELL_CAP, "%u", static_cast<unsigned>(firstHole + offset + 1));
  }
  snprintf(dataCells[0][9], CELL_CAP, "%s", segmentLabel);

  uint8_t row = 1;

  // Par.
  uint16_t parTotal = 0;
  for (uint8_t offset = 0; offset < 9; ++offset) {
    const uint8_t hole = static_cast<uint8_t>(firstHole + offset);
    const uint8_t par = hole < primary.holeCount ? primary.par[hole] : 0;
    parTotal = static_cast<uint16_t>(parTotal + par);
    if (par != 0) snprintf(dataCells[row][offset], CELL_CAP, "%u", static_cast<unsigned>(par));
  }
  if (parTotal != 0) snprintf(dataCells[row][9], CELL_CAP, "%u", static_cast<unsigned>(parTotal));
  ++row;

  // Stroke index, only when the course carries it (no dash row otherwise).
  if (hasSiRow) {
    for (uint8_t offset = 0; offset < 9; ++offset) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + offset);
      if (hole < primary.holeCount) {
        snprintf(dataCells[row][offset], CELL_CAP, "%u", static_cast<unsigned>(primary.si[hole]));
      }
    }
    ++row;
  }

  // One row per tee, already longest-first from CONTRACTS-V2 §35.
  for (uint8_t tee = 0; tee < teeSet.teeCount && row < MAX_TABLE_ROWS; ++tee) {
    const GolfCourseTee& entry = teeSet.tees[tee];
    uint32_t sum = 0;
    for (uint8_t offset = 0; offset < 9; ++offset) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + offset);
      if (entry.hasYards && hole < primary.holeCount) {
        sum += entry.yards[hole];
        snprintf(dataCells[row][offset], CELL_CAP, "%u", static_cast<unsigned>(entry.yards[hole]));
      } else {
        snprintf(dataCells[row][offset], CELL_CAP, "%s", tr(STR_GOLF_EM_DASH));
      }
    }
    if (entry.hasYards) snprintf(dataCells[row][9], CELL_CAP, "%lu", static_cast<unsigned long>(sum));
    ++row;
  }

  tableRows = row;

  const int16_t labelWidth = static_cast<int16_t>((static_cast<int32_t>(tableRect.width) * LABEL_COLUMN_UNITS) /
                                                  (DATA_COLS + LABEL_COLUMN_UNITS));
  const int16_t dataLeft = static_cast<int16_t>(tableRect.x + labelWidth);
  const int16_t dataWidth = static_cast<int16_t>(tableRect.width - labelWidth);

  for (uint8_t index = 0; index < tableRows; ++index) {
    const fui::Rect rowRect = golfui::evenRow(tableRect, tableRows, index);
    const int16_t top = rowRect.y;
    const int16_t rowHeight = rowRect.height;
    const bool header = index == 0;

    const char* rowLabel = index == 0               ? tr(STR_GOLF_HOLE)
                           : index == 1             ? tr(STR_GOLF_PAR)
                           : hasSiRow && index == 2 ? tr(STR_GOLF_STROKE_INDEX)
                                                    : "";
    if (rowLabel[0] == '\0') {
      const uint8_t teeIndex = static_cast<uint8_t>(index - (hasSiRow ? 3 : 2));
      if (teeIndex < teeSet.teeCount) rowLabel = golfTeeDisplayLabel(teeSet.tees[teeIndex].name);
    }

    labelPointer[0] = rowLabel;
    tableProps = {};
    tableProps.cells = labelPointer;
    tableProps.rows = 1;
    tableProps.cols = 1;
    tableProps.rowHeight = rowHeight;
    tableProps.padding = 2;
    tableProps.text = screen.theme().smallText;
    tableProps.text.align = header ? fui::TextAlign::Center : fui::TextAlign::Left;
    tableProps.text.bold = true;
    tableProps.headerRow = header;
    fui::table(screen.frame(), fui::Rect{tableRect.x, top, labelWidth, rowHeight}, tableProps);

    for (uint8_t column = 0; column < DATA_COLS; ++column) dataPointers[column] = dataCells[index][column];
    tableProps.cells = dataPointers;
    tableProps.cols = DATA_COLS;
    tableProps.text.align = fui::TextAlign::Center;
    tableProps.text.bold = header;
    fui::table(screen.frame(), fui::Rect{dataLeft, top, dataWidth, rowHeight}, tableProps);
  }
}

void GolfCourseTeeInfoActivity::drawFooter() const {
  const bool hasTabs = holeCount == 18;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", hasTabs ? tr(STR_GOLF_PREVIOUS_TAB) : "",
                                            hasTabs ? tr(STR_GOLF_NEXT_TAB) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfCourseTeeInfoActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, tr(STR_GOLF_TEE_INFO_TITLE), courseName);

  if (!resolved || teeSet.teeCount == 0) {
    const fui::Rect body = layout.body;
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawCenteredText(UI_12_FONT_ID, body.y + (body.height - lineHeight) / 2, tr(STR_GOLF_TEE_INFO_EMPTY),
                              true);
  } else {
    renderUi();
  }

  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
