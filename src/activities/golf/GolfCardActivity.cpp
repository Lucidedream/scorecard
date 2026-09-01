#include "GolfCardActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>
#include <I18n.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfStats.h"

namespace fui = freeink::ui;

namespace {

bool entered(const GolfRound& round, const GolfPlayerScore& score, const uint8_t hole) {
  return golfHoleScore(round, score, hole) != 0;
}

uint16_t scoreRangeTotal(const GolfRound& round, const GolfPlayerScore& score, const uint8_t first,
                         const uint8_t last) {
  uint16_t total = 0;
  for (uint8_t hole = first; hole < last && hole < round.holeCount; ++hole) {
    if (entered(round, score, hole)) total += golfHoleScore(round, score, hole);
  }
  return total;
}

uint16_t parRangeTotal(const GolfRound& round, const uint8_t first, const uint8_t last) {
  uint16_t total = 0;
  for (uint8_t hole = first; hole < last && hole < round.holeCount; ++hole) total += round.par[hole];
  return total;
}

bool anyEntered(const GolfRound& round, const GolfPlayerScore& score, const uint8_t first, const uint8_t last) {
  for (uint8_t hole = first; hole < last && hole < round.holeCount; ++hole) {
    if (entered(round, score, hole)) return true;
  }
  return false;
}

}  // namespace

void GolfCardActivity::onEnter() {
  Activity::onEnter();
  if (!archived) round = GOLF_ROUND_STORE.getRound();
  collectPlayers();
  resetUi();
  app.on(ACTION_TAB, &GolfCardActivity::tabTrampoline, this);
  app.setScreen(&GolfCardActivity::screenTrampoline, this);
  firstPaint = true;
  requestUpdate();
}

void GolfCardActivity::collectPlayers() {
  playerCount = 0;
  for (uint8_t slot = 0; slot < GolfRound::MAX_PLAYERS; ++slot) {
    if (!golfPlayerIsEnabled(round.players[slot])) continue;
    playerSlots[playerCount] = slot;
    golfFormatPlayerLabel(slot, round.players[slot].name, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabels[playerCount],
                          sizeof(playerLabels[playerCount]));
    ++playerCount;
  }
}

void GolfCardActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfCardActivity*>(user)->buildScreen(screen);
}

void GolfCardActivity::tabTrampoline(const fui::ActionEvent& event, void* user) {
  if (event.value < 0 || event.value > 1) return;
  auto* self = static_cast<GolfCardActivity*>(user);
  {
    RenderLock lock(*self);
    self->activeTab = static_cast<uint8_t>(event.value);
  }
  self->app.clearTapFlash();
  self->requestUpdate();
}

void GolfCardActivity::changeTab(const int delta) {
  const uint8_t count = round.holeCount == 18 ? 2 : 1;
  if (count == 1) return;
  {
    RenderLock lock(*this);
    activeTab = static_cast<uint8_t>((activeTab + count + delta) % count);
  }
  requestUpdate();
}

void GolfCardActivity::loop() {
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

void GolfCardActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  const int16_t fontMinimum = screen.target().lineHeight(screen.theme().smallText.font);
  const auto layout = golfui::makeCardLayout(screen.body(), round.holeCount == 18, metrics.tabBarHeight,
                                             metrics.verticalSpacing, playerCount, golfHasPar(round), fontMinimum);
  if (round.holeCount == 18) {
    segmentTabs[0] = {tr(STR_GOLF_FRONT_NINE), {}, {}, 0, activeTab == 0, true};
    segmentTabs[1] = {tr(STR_GOLF_BACK_NINE), {}, {}, 1, activeTab == 1, true};
    tabProps = {};
    tabProps.tabs = segmentTabs;
    tabProps.count = 2;
    tabProps.action = ACTION_TAB;
    tabProps.inputMask = fui::InputTouch;
    tabProps.text = screen.theme().smallText;
    tabProps.divider = true;
    fui::tabBar(screen.frame(), layout.tabs, tabProps);
  }
  const uint8_t first = round.holeCount == 18 && activeTab == 1 ? 9 : 0;
  buildCard(screen, layout.table, first,
            first == 0 && round.holeCount == 18 ? tr(STR_GOLF_OUT)
            : first == 9                        ? tr(STR_GOLF_IN)
                                                : nullptr);
}

void GolfCardActivity::buildCard(UiScreen& screen, const fui::Rect tableRect, const uint8_t firstHole,
                                 const char* segmentLabel) {
  const bool hasPar = golfHasPar(round);
  tableHeaderRows = hasPar ? 2 : 1;
  tableRows = static_cast<uint8_t>(tableHeaderRows + playerCount);
  tableDataColumns = round.holeCount == 18 ? 11 : 10;
  tableFirstHole = firstHole;
  const uint8_t lastHole = static_cast<uint8_t>(firstHole + 9);

  for (uint8_t row = 0; row < MAX_TABLE_ROWS; ++row) {
    for (uint8_t column = 0; column < MAX_DATA_COLS; ++column) dataCells[row][column][0] = '\0';
  }

  for (uint8_t offset = 0; offset < 9; ++offset) {
    snprintf(dataCells[0][offset], sizeof(dataCells[0][offset]), "%u", static_cast<unsigned>(firstHole + offset + 1));
  }
  if (segmentLabel != nullptr) {
    snprintf(dataCells[0][9], sizeof(dataCells[0][9]), "%s", segmentLabel);
  }
  snprintf(dataCells[0][tableDataColumns - 1], sizeof(dataCells[0][tableDataColumns - 1]), "%s",
           tr(STR_GOLF_TOTAL_SHORT));

  if (hasPar) {
    for (uint8_t offset = 0; offset < 9; ++offset) {
      snprintf(dataCells[1][offset], sizeof(dataCells[1][offset]), "%u",
               static_cast<unsigned>(round.par[firstHole + offset]));
    }
    if (segmentLabel != nullptr) {
      snprintf(dataCells[1][9], sizeof(dataCells[1][9]), "%u",
               static_cast<unsigned>(parRangeTotal(round, firstHole, lastHole)));
    }
    snprintf(dataCells[1][tableDataColumns - 1], sizeof(dataCells[1][tableDataColumns - 1]), "%u",
             static_cast<unsigned>(parRangeTotal(round, 0, round.holeCount)));
  }

  for (uint8_t playerRow = 0; playerRow < playerCount; ++playerRow) {
    const GolfPlayerScore& score = round.players[playerSlots[playerRow]].score;
    const uint8_t row = static_cast<uint8_t>(tableHeaderRows + playerRow);
    for (uint8_t offset = 0; offset < 9; ++offset) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + offset);
      if (entered(round, score, hole)) {
        snprintf(dataCells[row][offset], sizeof(dataCells[row][offset]), "%u",
                 static_cast<unsigned>(golfHoleScore(round, score, hole)));
      } else {
        snprintf(dataCells[row][offset], sizeof(dataCells[row][offset]), "·");
      }
    }
    if (segmentLabel != nullptr) {
      if (anyEntered(round, score, firstHole, lastHole)) {
        snprintf(dataCells[row][9], sizeof(dataCells[row][9]), "%u",
                 static_cast<unsigned>(scoreRangeTotal(round, score, firstHole, lastHole)));
      } else {
        snprintf(dataCells[row][9], sizeof(dataCells[row][9]), "·");
      }
    }
    if (anyEntered(round, score, 0, round.holeCount)) {
      snprintf(dataCells[row][tableDataColumns - 1], sizeof(dataCells[row][tableDataColumns - 1]), "%u",
               static_cast<unsigned>(scoreRangeTotal(round, score, 0, round.holeCount)));
    } else {
      snprintf(dataCells[row][tableDataColumns - 1], sizeof(dataCells[row][tableDataColumns - 1]), "·");
    }
  }

  tableTop = tableRect.y;
  tableHeight = tableRect.height;
  const int16_t labelWidth = static_cast<int16_t>((static_cast<int32_t>(tableRect.width) * LABEL_COLUMN_UNITS) /
                                                  (tableDataColumns + LABEL_COLUMN_UNITS));
  dataLeft = static_cast<int16_t>(tableRect.x + labelWidth);
  dataWidth = static_cast<int16_t>(tableRect.width - labelWidth);

  for (uint8_t row = 0; row < tableRows; ++row) {
    const fui::Rect rowRect = golfui::evenRow(tableRect, tableRows, row);
    const int16_t top = rowRect.y;
    const int16_t rowHeight = rowRect.height;
    const char* rowLabel = row == 0                                    ? tr(STR_GOLF_HOLE)
                           : hasPar && row == 1                        ? tr(STR_GOLF_PAR)
                           : row >= tableHeaderRows && playerCount > 0 ? playerLabels[row - tableHeaderRows]
                                                                       : "";

    labelPointer[0] = rowLabel;
    tableProps = {};
    tableProps.cells = labelPointer;
    tableProps.rows = 1;
    tableProps.cols = 1;
    tableProps.rowHeight = rowHeight;
    tableProps.padding = 2;
    tableProps.text = screen.theme().smallText;
    tableProps.text.align = row >= tableHeaderRows ? fui::TextAlign::Left : fui::TextAlign::Center;
    tableProps.text.bold = row == 0 || row >= tableHeaderRows;
    tableProps.headerRow = row == 0;
    fui::table(screen.frame(), fui::Rect{tableRect.x, top, labelWidth, rowHeight}, tableProps);

    for (uint8_t column = 0; column < tableDataColumns; ++column) dataPointers[column] = dataCells[row][column];
    tableProps.cells = dataPointers;
    tableProps.cols = tableDataColumns;
    tableProps.text.align = fui::TextAlign::Center;
    fui::table(screen.frame(), fui::Rect{dataLeft, top, dataWidth, rowHeight}, tableProps);
  }
}

void GolfCardActivity::drawPenaltyMarkers() const {
  if (tableRows == 0 || tableDataColumns == 0 || dataWidth <= 0) return;
  for (uint8_t playerRow = 0; playerRow < playerCount; ++playerRow) {
    const GolfPlayerScore& score = round.players[playerSlots[playerRow]].score;
    const uint8_t displayRow = static_cast<uint8_t>(tableHeaderRows + playerRow);
    const int16_t rowTop =
        static_cast<int16_t>(tableTop + (static_cast<int32_t>(tableHeight) * displayRow) / tableRows);
    for (uint8_t offset = 0; offset < 9 && tableFirstHole + offset < round.holeCount; ++offset) {
      const uint8_t hole = static_cast<uint8_t>(tableFirstHole + offset);
      if (score.penaltyCount[hole] == 0 || !entered(round, score, hole)) continue;
      const char* marker =
          golfObsForHole(score, hole) > 0 ? tr(STR_GOLF_OUT_OF_BOUNDS_SHORT_TAG) : tr(STR_GOLF_HAZARD_TAG);
      const int16_t cellRight =
          static_cast<int16_t>(dataLeft + (static_cast<int32_t>(dataWidth) * (offset + 1)) / tableDataColumns);
      const int markerWidth = renderer.getTextWidth(SMALL_FONT_ID, marker, EpdFontFamily::BOLD);
      renderer.drawText(SMALL_FONT_ID, cellRight - markerWidth - 2, rowTop + 2, marker, true, EpdFontFamily::BOLD);
    }
  }
}

void GolfCardActivity::drawFooter() const {
  const bool hasTabs = round.holeCount == 18;
  const auto labels =
      mappedInput.mapLabels(tr(STR_GOLF_BUTTON_BACK), hasTabs ? tr(STR_GOLF_NEXT_TAB) : "",
                            hasTabs ? tr(STR_GOLF_PREVIOUS_TAB) : "", hasTabs ? tr(STR_GOLF_NEXT_TAB) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfCardActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, tr(STR_GOLF_APP_TITLE), round.courseName);
  renderUi();
  drawPenaltyMarkers();
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
