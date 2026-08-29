#include "GolfRoundSummaryActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdio>

#include "GolfStrings.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {

void formatToPar(const int16_t value, char* output, const size_t size) {
  if (value == 0) {
    snprintf(output, size, "%s", GolfStrings::EVEN);
  } else {
    snprintf(output, size, value > 0 ? "+%d" : "%d", value);
  }
}

}  // namespace

void GolfRoundSummaryActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&GolfRoundSummaryActivity::screenTrampoline, this);
  requestUpdate();
}

void GolfRoundSummaryActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfRoundSummaryActivity*>(user)->buildScreen(screen);
}

void GolfRoundSummaryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void GolfRoundSummaryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  const bool hasPar = golfHistoryShowsToPar(entry);
  const char* labels[] = {GolfStrings::SCORE, GolfStrings::TO_PAR, GolfStrings::PUTTS, GolfStrings::IN100_CARD,
                          GolfStrings::LONG_GAME};
  const uint16_t longGame = entry.strokes >= entry.in100 ? static_cast<uint16_t>(entry.strokes - entry.in100) : 0;
  const uint16_t values[] = {entry.strokes, 0, entry.putts, entry.in100, longGame};
  const uint8_t rows = hasPar ? 5 : 4;
  for (uint8_t displayRow = 0; displayRow < rows; ++displayRow) {
    const uint8_t dataRow = !hasPar && displayRow > 0 ? static_cast<uint8_t>(displayRow + 1) : displayRow;
    snprintf(cells[displayRow][0], sizeof(cells[displayRow][0]), "%s", labels[dataRow]);
    if (dataRow == 1) {
      formatToPar(static_cast<int16_t>(entry.strokes) - entry.par, cells[displayRow][1], sizeof(cells[displayRow][1]));
    } else {
      snprintf(cells[displayRow][1], sizeof(cells[displayRow][1]), "%u", values[dataRow]);
    }
  }

  const fui::Rect body = screen.body();
  const int16_t rowHeight = static_cast<int16_t>(body.height / rows);
  for (uint8_t row = 0; row < rows; ++row) {
    const char* pointers[] = {cells[row][0], cells[row][1]};
    fui::TableProps props;
    props.cells = pointers;
    props.rows = 1;
    props.cols = 2;
    props.rowHeight = rowHeight;
    props.text = screen.theme().bodyText;
    props.text.align = fui::TextAlign::Center;
    props.text.bold = row == 0;
    fui::table(screen.frame(), fui::Rect{body.x, static_cast<int16_t>(body.y + row * rowHeight), body.width, rowHeight},
               props);
  }
}

void GolfRoundSummaryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::ROUND_SUMMARY, entry.course);
  renderUi();
  const auto labels =
      mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::BACK, GolfStrings::EMPTY, GolfStrings::EMPTY);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
