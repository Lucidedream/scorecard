#include "GolfCardActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>

#include <cstdio>

#include "GolfStrings.h"
#include "components/UITheme.h"
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
      if (row == 3) total += round.putts[hole];
      if (row == 4) total += round.in100[hole];
      if (row == 5) total += round.out100[hole];
      if (row == 6) total += golfPenaltyStrokesForHole(round, hole);
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

void formatPercent(const uint16_t part, const uint16_t whole, char* output, const size_t size) {
  const uint16_t percent =
      whole == 0 ? 0 : static_cast<uint16_t>((static_cast<uint32_t>(part) * 100 + whole / 2) / whole);
  snprintf(output, size, GolfStrings::WHOLE_PERCENT_FORMAT, percent);
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
  if (event.value < 0 || event.value > 2) return;
  auto* self = static_cast<GolfCardActivity*>(user);
  {
    RenderLock lock(*self);
    self->activeTab = static_cast<uint8_t>(event.value);
  }
  self->app.clearTapFlash();
  self->requestUpdate();
}

void GolfCardActivity::changeTab(const int delta) {
  const uint8_t count = round.holeCount == 18 ? 3 : 1;
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
                                 {GolfStrings::BACK_NINE, {}, {}, 1, activeTab == 1, true},
                                 {GolfStrings::STATS, {}, {}, 2, activeTab == 2, true}};
    fui::TabBarProps props;
    props.tabs = tabs;
    props.count = 3;
    props.action = ACTION_TAB;
    props.inputMask = fui::InputTouch;
    props.text = screen.theme().smallText;
    props.divider = true;
    fui::tabBar(screen.frame(), screen.takeTop(static_cast<int16_t>(metrics.tabBarHeight)), props);
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  }
  if (round.holeCount == 18 && activeTab == 2) {
    buildStats(screen);
  } else {
    const uint8_t first = round.holeCount == 18 && activeTab == 1 ? 9 : 0;
    buildCard(screen, first,
              first == 0 && round.holeCount == 18 ? GolfStrings::OUT
              : first == 9                        ? GolfStrings::IN
                                                  : nullptr);
  }
}

void GolfCardActivity::buildCard(UiScreen& screen, const uint8_t firstHole, const char* segmentLabel) {
  const bool hasPar = golfHasPar(round);
  const uint8_t rows = hasPar ? 7 : 6;
  const uint8_t columns = round.holeCount == 18 ? 12 : 11;
  const uint8_t lastHole = static_cast<uint8_t>(firstHole + 9);
  const char* labels[] = {GolfStrings::HOLE,       GolfStrings::PAR,         GolfStrings::SCORE,   GolfStrings::PUTTS,
                          GolfStrings::IN100_CARD, GolfStrings::OUT100_CARD, GolfStrings::PEN_CARD};

  for (uint8_t displayRow = 0; displayRow < rows; ++displayRow) {
    const uint8_t dataRow = !hasPar && displayRow > 0 ? static_cast<uint8_t>(displayRow + 1) : displayRow;
    snprintf(cells[displayRow][0], sizeof(cells[displayRow][0]), "%s", labels[dataRow]);
    for (uint8_t column = 1; column <= 9; ++column) {
      const uint8_t hole = static_cast<uint8_t>(firstHole + column - 1);
      if (dataRow == 0) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%u", hole + 1);
      } else if (dataRow == 1) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%u", round.par[hole]);
      } else if (!entered(round, hole) || (dataRow == 6 && golfPenaltyStrokesForHole(round, hole) == 0)) {
        snprintf(cells[displayRow][column], sizeof(cells[displayRow][column]), "%s", GolfStrings::DOT);
      } else {
        const uint16_t value = dataRow == 2   ? golfHoleScore(round, hole)
                               : dataRow == 3 ? round.putts[hole]
                               : dataRow == 4 ? round.in100[hole]
                               : dataRow == 5 ? round.out100[hole]
                                              : golfPenaltyStrokesForHole(round, hole);
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
  const int16_t rowHeight = static_cast<int16_t>(body.height / rows);
  for (uint8_t row = 0; row < rows; ++row) {
    const char* pointers[MAX_TABLE_COLS]{};
    for (uint8_t column = 0; column < columns; ++column) pointers[column] = cells[row][column];
    fui::TableProps props;
    props.cells = pointers;
    props.rows = 1;
    props.cols = columns;
    props.rowHeight = rowHeight;
    props.padding = 1;
    props.text = screen.theme().smallText;
    props.text.align = fui::TextAlign::Center;
    props.text.bold = row == 0 || (!hasPar ? row == 1 : row == 2);
    props.headerRow = row == 0;
    fui::table(screen.frame(), fui::Rect{body.x, static_cast<int16_t>(body.y + row * rowHeight), body.width, rowHeight},
               props);
  }
}

void GolfCardActivity::buildStats(UiScreen& screen) {
  const char* labels[] = {GolfStrings::LONG_GAME, GolfStrings::SHORT_GAME, GolfStrings::PUTTING,
                          GolfStrings::PENALTIES, GolfStrings::ONE_PUTTS,  GolfStrings::THREE_PUTTS};
  const uint16_t values[] = {golfLongTotal(round),  golfShortTotal(round),
                             golfPuttsTotal(round), golfPenaltyStrokesForRound(round),
                             golfOnePutts(round),   golfThreePutts(round)};
  const uint16_t score = golfScore(round);
  const fui::Rect body = screen.body();
  const int16_t rowHeight = static_cast<int16_t>(body.height / 6);
  for (uint8_t row = 0; row < 6; ++row) {
    snprintf(cells[row][0], sizeof(cells[row][0]), "%s", labels[row]);
    snprintf(cells[row][1], sizeof(cells[row][1]), "%u", values[row]);
    cells[row][2][0] = '\0';
    if (row < 4) formatPercent(values[row], score, cells[row][2], sizeof(cells[row][2]));
    const char* pointers[] = {cells[row][0], cells[row][1], cells[row][2]};
    fui::TableProps props;
    props.cells = pointers;
    props.rows = 1;
    props.cols = 3;
    props.rowHeight = rowHeight;
    props.text = screen.theme().bodyText;
    props.text.align = fui::TextAlign::Center;
    props.text.bold = row < 4;
    fui::table(screen.frame(), fui::Rect{body.x, static_cast<int16_t>(body.y + row * rowHeight), body.width, rowHeight},
               props);
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
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::VIEW_CARD, archived ? round.courseName : nullptr);
  renderUi();
  drawFooter();
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

#endif
