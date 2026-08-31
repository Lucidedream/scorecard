#include "GolfCardActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>

#include <cstdio>

#include "GolfReviewFormat.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfStats.h"

namespace fui = freeink::ui;

namespace {

bool entered(const GolfRound& round, const uint8_t hole) { return golfHoleScore(round, hole) != 0; }

uint16_t rangeTotal(const GolfRound& round, const uint8_t first, const uint8_t last, const uint8_t row) {
  uint16_t total = 0;
  for (uint8_t hole = first; hole < last && hole < round.holeCount; ++hole) {
    if (row == 1) {
      total += round.par[hole];
    } else if (entered(round, hole)) {
      if (row == 2) total += golfHoleScore(round, hole);
    }
  }
  return total;
}

bool anyEntered(const GolfRound& round, const uint8_t first, const uint8_t last) {
  for (uint8_t hole = first; hole < last && hole < round.holeCount; ++hole) {
    if (entered(round, hole)) return true;
  }
  return false;
}

}  // namespace

void GolfCardActivity::onEnter() {
  Activity::onEnter();
  if (!archived) round = GOLF_ROUND_STORE.getRound();
  resetUi();
  app.on(ACTION_TAB, &GolfCardActivity::tabTrampoline, this);
  app.setScreen(&GolfCardActivity::screenTrampoline, this);
  firstPaint = true;
  requestUpdate();
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
      mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    changeTab(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    changeTab(1);
    return;
  }
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
}

void GolfCardActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  if (round.holeCount == 18) {
    const fui::TabItem tabs[] = {{GolfStrings::FRONT_NINE, {}, {}, 0, activeTab == 0, true},
                                 {GolfStrings::BACK_NINE, {}, {}, 1, activeTab == 1, true}};
    fui::TabBarProps props;
    props.tabs = tabs;
    props.count = 2;
    props.action = ACTION_TAB;
    props.inputMask = fui::InputTouch;
    props.text = screen.theme().smallText;
    props.divider = true;
    fui::tabBar(screen.frame(), screen.takeTop(static_cast<int16_t>(metrics.tabBarHeight)), props);
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  }
  const uint8_t first = round.holeCount == 18 && activeTab == 1 ? 9 : 0;
  buildCard(screen, first,
            first == 0 && round.holeCount == 18 ? GolfStrings::OUT
            : first == 9                        ? GolfStrings::IN
                                                : nullptr);
}

void GolfCardActivity::buildCard(UiScreen& screen, const uint8_t firstHole, const char* segmentLabel) {
  const bool hasPar = golfHasPar(round);
  const uint8_t rows = hasPar ? 3 : 2;
  const uint8_t columns = round.holeCount == 18 ? 12 : 11;
  const uint8_t lastHole = static_cast<uint8_t>(firstHole + 9);
  const char* labels[] = {GolfStrings::HOLE, GolfStrings::PAR, GolfStrings::SCORE};

  for (uint8_t displayRow = 0; displayRow < rows; ++displayRow) {
    const uint8_t dataRow = !hasPar && displayRow > 0 ? static_cast<uint8_t>(displayRow + 1) : displayRow;
    snprintf(cells[displayRow][0], sizeof(cells[displayRow][0]), "%s", labels[dataRow]);
    for (uint8_t column = 1; column <= 9; ++column) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + column - 1);
      if (dataRow == 0) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%u", hole + 1);
      } else if (dataRow == 1) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%u", round.par[hole]);
      } else if (!entered(round, hole)) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%s", GolfStrings::DOT);
      } else {
        const uint16_t value = golfHoleScore(round, hole);
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%u", value);
      }
    }
    if (segmentLabel != nullptr) {
      if (dataRow == 0) {
        snprintf(cells[displayRow][10], sizeof(cells[displayRow][10]), "%s", segmentLabel);
      } else if (dataRow != 1 && !anyEntered(round, firstHole, lastHole)) {
        snprintf(cells[displayRow][10], sizeof(cells[displayRow][10]), "%s", GolfStrings::DOT);
      } else {
        snprintf(cells[displayRow][10], sizeof(cells[displayRow][10]), "%u",
                 rangeTotal(round, firstHole, lastHole, dataRow));
      }
    }
    const uint8_t totalColumn = static_cast<uint8_t>(columns - 1);
    if (dataRow == 0) {
      snprintf(cells[displayRow][totalColumn], sizeof(cells[displayRow][totalColumn]), "%s", GolfStrings::TOTAL_SHORT);
    } else if (dataRow != 1 && !anyEntered(round, 0, round.holeCount)) {
      snprintf(cells[displayRow][totalColumn], sizeof(cells[displayRow][totalColumn]), "%s", GolfStrings::DOT);
    } else {
      snprintf(cells[displayRow][totalColumn], sizeof(cells[displayRow][totalColumn]), "%u",
               rangeTotal(round, 0, round.holeCount, dataRow));
    }
  }

  const fui::Rect body = screen.body();
  tableLeft = body.x;
  tableTop = body.y;
  tableWidth = body.width;
  tableColumns = columns;
  tableFirstHole = firstHole;
  int16_t y = body.y;
  for (uint8_t row = 0; row < rows; ++row) {
    const char* pointers[MAX_TABLE_COLS]{};
    for (uint8_t column = 0; column < columns; ++column) pointers[column] = cells[row][column];
    fui::TableProps props;
    props.cells = pointers;
    props.rows = 1;
    props.cols = columns;
    const bool scoreRow = row == static_cast<uint8_t>(rows - 1);
    const int16_t rowHeight = row == 0 ? HEADER_ROW_HEIGHT : scoreRow ? SCORE_ROW_HEIGHT : PAR_ROW_HEIGHT;
    props.rowHeight = rowHeight;
    props.padding = 1;
    props.text = scoreRow ? screen.theme().bodyText : screen.theme().smallText;
    props.text.align = fui::TextAlign::Center;
    props.text.bold = row == 0 || scoreRow;
    props.headerRow = row == 0;
    fui::table(screen.frame(), fui::Rect{body.x, y, body.width, rowHeight}, props);
    y = static_cast<int16_t>(y + rowHeight);
  }
}

void GolfCardActivity::drawPenaltySuperscripts() const {
  if (tableColumns == 0) return;
  const bool hasPar = golfHasPar(round);
  const int16_t scoreTop = static_cast<int16_t>(tableTop + HEADER_ROW_HEIGHT + (hasPar ? PAR_ROW_HEIGHT : 0));
  const int16_t cellWidth = static_cast<int16_t>(tableWidth / tableColumns);
  for (uint8_t offset = 0; offset < 9 && tableFirstHole + offset < round.holeCount; ++offset) {
    const uint8_t hole = static_cast<uint8_t>(tableFirstHole + offset);
    if (round.penaltyCount[hole] == 0 || !entered(round, hole)) continue;
    const char* marker = golfObsForHole(round, hole) > 0 ? GolfStrings::O_TAG : GolfStrings::HAZARD_TAG;
    const int16_t center = static_cast<int16_t>(tableLeft + (offset + 1) * cellWidth + cellWidth / 2);
    renderer.drawText(SMALL_FONT_ID, center + 5, scoreTop + 5, marker, true, EpdFontFamily::BOLD);
  }
}

void GolfCardActivity::drawFooter() const {
  const auto labels =
      mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::NEXT_TAB, GolfStrings::PREV_TAB, GolfStrings::NEXT_TAB);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfCardActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  char status[20];
  golfFormatRoundStatus(round, status, sizeof(status));
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::SCORECARD, status);
  renderUi();
  drawPenaltySuperscripts();
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
