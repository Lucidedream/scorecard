#include "GolfTrendsActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "GolfStrings.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

void formatTenths(const uint32_t value, char* output, const size_t size) {
  snprintf(output, size, GolfStrings::DECIMAL_FORMAT, static_cast<unsigned long>(value / 10),
           static_cast<unsigned long>(value % 10));
}

void formatSignedTenths(const int32_t value, char* output, const size_t size) {
  const uint32_t magnitude = static_cast<uint32_t>(value < 0 ? -value : value);
  const char* format = value > 0   ? GolfStrings::POSITIVE_DECIMAL_FORMAT
                       : value < 0 ? GolfStrings::NEGATIVE_DECIMAL_FORMAT
                                   : GolfStrings::DECIMAL_FORMAT;
  snprintf(output, size, format, static_cast<unsigned long>(magnitude / 10),
           static_cast<unsigned long>(magnitude % 10));
}

void formatPercent(const uint32_t value, char* output, const size_t size) {
  snprintf(output, size, GolfStrings::PERCENT_FORMAT, static_cast<unsigned long>(value / 10),
           static_cast<unsigned long>(value % 10));
}

}  // namespace

void GolfTrendsActivity::onEnter() {
  Activity::onEnter();
  loadHistory();
  trends = golfCalculateTrends(history);
  if (trends.enoughRounds()) {
    snprintf(subtitle, sizeof(subtitle), GolfStrings::AVERAGE_OVER_ROUNDS, trends.rounds);
  } else {
    snprintf(message, sizeof(message), GolfStrings::TRENDS_NEED_ROUNDS, trends.rounds);
  }
  resetUi();
  app.setScreen(&GolfTrendsActivity::screenTrampoline, this);
  requestUpdate();
}

void GolfTrendsActivity::logMalformed(const uint32_t lineNumber, void*) {
  LOG_ERR("GOLF", "Malformed index row at line %lu", static_cast<unsigned long>(lineNumber));
}

void GolfTrendsActivity::loadHistory() {
  history.reset();
  loadError = false;
  if (!Storage.exists(INDEX_PATH)) return;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) {
    loadError = true;
    return;
  }
  char chunk[128];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      loadError = true;
      break;
    }
    history.feed(chunk, static_cast<size_t>(bytesRead), &GolfTrendsActivity::logMalformed, this);
  }
  history.finish(&GolfTrendsActivity::logMalformed, this);
}

void GolfTrendsActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfTrendsActivity*>(user)->buildScreen(screen);
}

void GolfTrendsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void GolfTrendsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  if (loadError || !trends.enoughRounds()) {
    screen.centeredText(loadError ? GolfStrings::TRENDS_ERROR : message, screen.theme().bodyText);
    return;
  }

  const char* labels[MAX_ROWS] = {GolfStrings::SCORING_AVERAGE, GolfStrings::AVERAGE_TO_PAR,
                                  GolfStrings::BEST_WORST,      GolfStrings::PUTTS_PER_ROUND,
                                  GolfStrings::LONG_GAME,       GolfStrings::SHORT_GAME,
                                  GolfStrings::PUTTING};
  const uint32_t averages[MAX_ROWS] = {trends.scoringAverageTenths, 0,
                                       0,                           trends.puttsAverageTenths,
                                       trends.longAverageTenths,    trends.shortAverageTenths,
                                       trends.puttingAverageTenths};
  const uint32_t percentages[3] = {trends.longPercentTenths, trends.shortPercentTenths,
                                   trends.puttingPercentTenths};
  const uint8_t rowCount = trends.showsToPar ? MAX_ROWS : static_cast<uint8_t>(MAX_ROWS - 1);
  for (uint8_t row = 0; row < rowCount; ++row) {
    const uint8_t dataRow = !trends.showsToPar && row > 0 ? static_cast<uint8_t>(row + 1) : row;
    snprintf(cells[row][0], sizeof(cells[row][0]), "%s", labels[dataRow]);
    cells[row][2][0] = '\0';
    if (dataRow == 1) {
      formatSignedTenths(trends.toParAverageTenths, cells[row][1], sizeof(cells[row][1]));
    } else if (dataRow == 2) {
      snprintf(cells[row][1], sizeof(cells[row][1]), GolfStrings::BEST_WORST_FORMAT, trends.best, trends.worst);
    } else {
      formatTenths(averages[dataRow], cells[row][1], sizeof(cells[row][1]));
    }
    if (dataRow >= 4) formatPercent(percentages[dataRow - 4], cells[row][2], sizeof(cells[row][2]));
  }

  const fui::Rect body = screen.body();
  const int16_t rowHeight = static_cast<int16_t>(body.height / rowCount);
  for (uint8_t row = 0; row < rowCount; ++row) {
    const char* pointers[] = {cells[row][0], cells[row][1], cells[row][2]};
    fui::TableProps props;
    props.cells = pointers;
    props.rows = 1;
    props.cols = 3;
    props.rowHeight = rowHeight;
    props.text = screen.theme().bodyText;
    props.text.align = fui::TextAlign::Center;
    props.text.bold = row == 0;
    fui::table(screen.frame(), fui::Rect{body.x, static_cast<int16_t>(body.y + row * rowHeight), body.width, rowHeight},
               props);
  }
}

void GolfTrendsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight},
                 GolfStrings::TRENDS, trends.enoughRounds() ? subtitle : nullptr);
  renderUi();
  const auto labels = mappedInput.mapLabels(GolfStrings::BACK, GolfStrings::BACK, GolfStrings::EMPTY, GolfStrings::EMPTY);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
