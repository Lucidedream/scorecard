#include "GolfTrendsActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

void formatTenths(const uint32_t value, char* output, const size_t size) {
  snprintf(output, size, tr(STR_GOLF_DECIMAL_FORMAT), static_cast<unsigned long>(value / 10),
           static_cast<unsigned long>(value % 10));
}

void formatSignedTenths(const int32_t value, char* output, const size_t size) {
  const uint32_t magnitude = static_cast<uint32_t>(value < 0 ? -value : value);
  const char* format = value > 0   ? tr(STR_GOLF_POSITIVE_DECIMAL_FORMAT)
                       : value < 0 ? tr(STR_GOLF_NEGATIVE_DECIMAL_FORMAT)
                                   : tr(STR_GOLF_DECIMAL_FORMAT);
  snprintf(output, size, format, static_cast<unsigned long>(magnitude / 10),
           static_cast<unsigned long>(magnitude % 10));
}

void formatPercent(const uint32_t value, char* output, const size_t size) {
  snprintf(output, size, tr(STR_GOLF_PERCENT_FORMAT), static_cast<unsigned long>(value / 10),
           static_cast<unsigned long>(value % 10));
}

}  // namespace

GolfTrendsActivity::GolfTrendsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       const uint8_t selectedPlayerSlot, const char* selectedFallbackName)
    : Activity("GolfTrends", renderer, mappedInput), UiAppHost(renderer), playerSlot(selectedPlayerSlot) {
  const char* source = selectedFallbackName;
  if (source == nullptr && playerSlot < GolfRound::MAX_PLAYERS) source = GOLF_DEFAULT_PLAYER_NAMES[playerSlot];
  if (source != nullptr) snprintf(fallbackName, sizeof(fallbackName), "%s", source);
}

void GolfTrendsActivity::onEnter() {
  Activity::onEnter();
  golfFormatPlayerLabel(playerSlot, fallbackName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel, sizeof(playerLabel));
  activeState = &residentState;
  stagingOwner = makeUniqueNoThrow<TrendsStagingScratch>();
  stagingState = stagingOwner ? &stagingOwner->staging : nullptr;
  if (!stagingState) {
    LOG_ERR("GOLF", "OOM: trends staging scratch (%u bytes)", static_cast<unsigned>(sizeof(TrendsStagingScratch)));
    showStagingError();
  } else {
    loadHistory();
  }
  resetUi();
  app.setScreen(&GolfTrendsActivity::screenTrampoline, this);
  requestUpdate();
}

void GolfTrendsActivity::logMalformed(const uint32_t lineNumber, void*) {
  LOG_ERR("GOLF", "Malformed index row at line %lu", static_cast<unsigned long>(lineNumber));
}

bool GolfTrendsActivity::streamIndex(TrendsState& state) {
  if (!Storage.exists(INDEX_PATH)) return true;
  if (!stagingOwner) return false;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;
  bool success = true;
  while (file.available() > 0) {
    const int bytesRead = file.read(stagingOwner->chunk, sizeof(stagingOwner->chunk));
    if (bytesRead <= 0) {
      success = false;
      break;
    }
    state.history.feed(stagingOwner->chunk, static_cast<size_t>(bytesRead), &GolfTrendsActivity::logMalformed, this);
  }
  state.history.finish(&GolfTrendsActivity::logMalformed, this);
  return success;
}

void GolfTrendsActivity::refreshTrends(TrendsState& state) {
  state.trends = golfCalculateTrends(state.history);
  state.subtitle[0] = '\0';
  state.message[0] = '\0';
  if (state.history.count() == 0) {
    snprintf(state.message, sizeof(state.message), "%s", tr(STR_GOLF_NO_ROUNDS));
  } else if (state.trends.enoughRounds()) {
    snprintf(state.subtitle, sizeof(state.subtitle), tr(STR_GOLF_AVERAGE_OVER_ROUNDS_FORMAT),
             static_cast<unsigned>(state.trends.rounds));
    if (state.trends.showsPenalties) {
      snprintf(state.message, sizeof(state.message), tr(STR_GOLF_SHOT_MIX_OVER_ROUNDS_FORMAT),
               static_cast<unsigned>(state.trends.penaltyRounds));
    } else {
      snprintf(state.message, sizeof(state.message), tr(STR_GOLF_SHOT_MIX_NEED_ROUNDS_FORMAT),
               static_cast<unsigned>(state.trends.penaltyRounds));
    }
  } else {
    snprintf(state.message, sizeof(state.message), tr(STR_GOLF_TRENDS_NEED_ROUNDS_FORMAT),
             static_cast<unsigned>(state.trends.rounds));
  }
}

void GolfTrendsActivity::publishState() {
  RenderLock lock(*this);
  TrendsState* previous = activeState;
  activeState = stagingState;
  stagingState = previous;
  closeRouting();
}

void GolfTrendsActivity::showStagingError() {
  RenderLock lock(*this);
  activeState->history.reset(playerSlot);
  activeState->trends = {};
  activeState->subtitle[0] = '\0';
  activeState->message[0] = '\0';
  activeState->loadError = true;
  closeRouting();
}

void GolfTrendsActivity::loadHistory() {
  if (!stagingState) {
    showStagingError();
    return;
  }

  TrendsState& candidate = *stagingState;
  candidate.loadError = !candidate.history.reset(playerSlot);
  if (candidate.loadError) LOG_ERR("GOLF", "Trends received invalid player slot %u", playerSlot);
  if (!candidate.loadError && !RoundArchive::recoverIndex(stagingOwner->recovery)) {
    LOG_ERR("GOLF", "Trends refused unrecovered index.csv");
    candidate.loadError = true;
  }
  if (!candidate.loadError && !streamIndex(candidate)) candidate.loadError = true;
  refreshTrends(candidate);
  publishState();
}

void GolfTrendsActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfTrendsActivity*>(user)->buildScreen(screen);
}

void GolfTrendsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }
  const auto route = routeTouch(mappedInput);
  if (route.routed && app.invalidated()) requestUpdate();
}

void GolfTrendsActivity::buildScreen(UiScreen& screen) {
  const TrendsState& state = *activeState;
  const GolfTrendStats& trends = state.trends;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  if (state.loadError || !trends.enoughRounds()) {
    screen.centeredText(state.loadError ? tr(STR_GOLF_TRENDS_ERROR) : state.message, screen.theme().bodyText);
    return;
  }

  fui::TextStyle subtitleStyle = screen.theme().smallText;
  subtitleStyle.align = fui::TextAlign::Center;
  const int16_t subtitleHeight = screen.target().lineHeight(subtitleStyle.font);
  const int16_t gap = static_cast<int16_t>(metrics.verticalSpacing);
  screen.target().text(screen.takeTop(subtitleHeight, gap), state.subtitle, subtitleStyle);

  const char* labels[MAX_ROWS] = {tr(STR_GOLF_SCORING_AVERAGE), tr(STR_GOLF_AVERAGE_TO_PAR), tr(STR_GOLF_BEST_WORST),
                                  tr(STR_GOLF_PUTTS_PER_ROUND), tr(STR_GOLF_LONG_GAME),      tr(STR_GOLF_SHORT_GAME),
                                  tr(STR_GOLF_PUTTING),         tr(STR_GOLF_PENALTIES)};
  const uint32_t averages[MAX_ROWS] = {trends.scoringAverageTenths,
                                       0,
                                       0,
                                       trends.puttsAverageTenths,
                                       trends.longAverageTenths,
                                       trends.shortAverageTenths,
                                       trends.puttingAverageTenths,
                                       trends.penaltyStrokesAverageTenths};
  const uint32_t percentages[4] = {trends.longPercentTenths, trends.shortPercentTenths, trends.puttingPercentTenths,
                                   trends.penaltyPercentTenths};
  uint8_t rowCount = MAX_ROWS;
  if (!trends.showsToPar) --rowCount;
  if (!trends.showsPenalties) rowCount = static_cast<uint8_t>(rowCount - 4);
  for (uint8_t row = 0; row < rowCount; ++row) {
    const uint8_t dataRow = !trends.showsToPar && row > 0 ? static_cast<uint8_t>(row + 1) : row;
    snprintf(cells[row][0], sizeof(cells[row][0]), "%s", labels[dataRow]);
    cells[row][2][0] = '\0';
    if (dataRow == 1) {
      formatSignedTenths(trends.toParAverageTenths, cells[row][1], sizeof(cells[row][1]));
    } else if (dataRow == 2) {
      snprintf(cells[row][1], sizeof(cells[row][1]), tr(STR_GOLF_BEST_WORST_FORMAT), static_cast<unsigned>(trends.best),
               static_cast<unsigned>(trends.worst));
    } else {
      formatTenths(averages[dataRow], cells[row][1], sizeof(cells[row][1]));
    }
    if (dataRow >= 4 && dataRow <= 7) {
      formatPercent(percentages[dataRow - 4], cells[row][2], sizeof(cells[row][2]));
    }
  }

  fui::TextStyle noteStyle = screen.theme().smallText;
  noteStyle.align = fui::TextAlign::Center;
  const int16_t noteHeight = screen.target().lineHeight(noteStyle.font);
  const int16_t bodyLine = screen.target().lineHeight(screen.theme().bodyText.font);
  const int requiredRows = rowCount * bodyLine;
  const int16_t noteGap =
      screen.body().height > noteHeight + requiredRows
          ? golfui::minValue(gap, static_cast<int16_t>(screen.body().height - noteHeight - requiredRows))
          : 0;
  screen.target().text(screen.takeBottom(noteHeight, noteGap), state.message, noteStyle);
  const fui::Rect body = screen.body();
  for (uint8_t row = 0; row < rowCount; ++row) {
    const char* pointers[] = {cells[row][0], cells[row][1], cells[row][2]};
    const fui::Rect rowRect = golfui::evenRow(body, rowCount, row);
    tableProps = {};
    tableProps.cells = pointers;
    tableProps.rows = 1;
    tableProps.cols = 3;
    tableProps.rowHeight = rowRect.height;
    tableProps.text = screen.theme().bodyText;
    tableProps.text.align = fui::TextAlign::Center;
    tableProps.text.bold = row == 0;
    fui::table(screen.frame(), rowRect, tableProps);
  }
}

void GolfTrendsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, tr(STR_GOLF_TRENDS), playerLabel);
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
