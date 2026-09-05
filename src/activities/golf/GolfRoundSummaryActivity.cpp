#include "GolfRoundSummaryActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <I18n.h>
#include <Memory.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfRoundExportActivity.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {

void formatToPar(const int16_t value, char* output, const size_t size) {
  golfFormatReviewToPar(value, tr(STR_GOLF_EVEN), tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT),
                        tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), output, size);
}

}  // namespace

void GolfRoundSummaryActivity::onEnter() {
  Activity::onEnter();
  golfFormatPlayerLabel(entry.playerSlot, entry.playerName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel,
                        sizeof(playerLabel));
  resetUi();
  app.setScreen(&GolfRoundSummaryActivity::screenTrampoline, this);
  requestUpdate();
}

void GolfRoundSummaryActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfRoundSummaryActivity*>(user)->buildScreen(screen);
}

void GolfRoundSummaryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    auto transfer = makeUniqueNoThrow<GolfRoundExportActivity>(renderer, mappedInput, entry, archiveFilename);
    if (!transfer) {
      LOG_ERR("GOLF", "OOM: summary export activity");
      {
        RenderLock lock(*this);
        exportFailed = true;
      }
      requestUpdate();
      return;
    }
    {
      RenderLock lock(*this);
      exportFailed = false;
    }
    startActivityForResult(std::move(transfer), nullptr);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (returnToGolfHome) {
      openGolfHome(activityManager, renderer, mappedInput);
    } else {
      finish();
    }
  }
}

void GolfRoundSummaryActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(layout.contentMargins);
  const bool hasPar = golfHistoryShowsToPar(entry);
  const char* labels[] = {tr(STR_GOLF_SCORE),           tr(STR_GOLF_TO_PAR),       tr(STR_GOLF_PUTTS),
                          tr(STR_GOLF_INSIDE_100_CARD), tr(STR_GOLF_LONG_GAME),    tr(STR_GOLF_TOTAL_PENALTIES),
                          tr(STR_GOLF_HAZARDS),         tr(STR_GOLF_OUT_OF_BOUNDS)};
  const uint16_t penaltyStrokes = static_cast<uint16_t>(entry.hazards + entry.obs * 2);
  const uint16_t values[] = {entry.strokes, 0,        entry.putts, entry.in100, entry.out100, penaltyStrokes,
                             entry.hazards, entry.obs};
  uint8_t rows = 0;
  for (uint8_t dataRow = 0; dataRow < 8; ++dataRow) {
    if ((!hasPar && dataRow == 1) || (!entry.penaltiesRecorded && dataRow >= 5)) continue;
    snprintf(cells[rows][0], sizeof(cells[rows][0]), "%s", labels[dataRow]);
    if (dataRow == 1) {
      formatToPar(static_cast<int16_t>(entry.strokes) - entry.par, cells[rows][1], sizeof(cells[rows][1]));
    } else {
      snprintf(cells[rows][1], sizeof(cells[rows][1]), "%u", static_cast<unsigned>(values[dataRow]));
    }
    ++rows;
  }

  fui::TextStyle noteStyle = screen.theme().smallText;
  noteStyle.align = fui::TextAlign::Center;
  const int16_t noteHeight = screen.target().lineHeight(noteStyle.font);
  const int16_t bodyLine = screen.target().lineHeight(screen.theme().bodyText.font);
  const int requiredRows = rows * bodyLine;
  const int16_t noteGap = screen.body().height > noteHeight + requiredRows
                              ? golfui::minValue(static_cast<int16_t>(metrics.verticalSpacing),
                                                 static_cast<int16_t>(screen.body().height - noteHeight - requiredRows))
                              : 0;
  screen.target().text(screen.takeBottom(noteHeight, noteGap),
                       exportFailed         ? tr(STR_GOLF_EXPORT_ERROR)
                       : archiveFilename[0] ? tr(STR_GOLF_EXPORT_SEND)
                                            : tr(STR_GOLF_CSV_DETAIL_UNAVAILABLE),
                       noteStyle);

  const fui::Rect body = screen.body();
  for (uint8_t row = 0; row < rows; ++row) {
    const char* pointers[] = {cells[row][0], cells[row][1]};
    const fui::Rect rowRect = golfui::evenRow(body, rows, row);
    tableProps = {};
    tableProps.cells = pointers;
    tableProps.rows = 1;
    tableProps.cols = 2;
    tableProps.rowHeight = rowRect.height;
    tableProps.text = screen.theme().bodyText;
    tableProps.text.align = fui::TextAlign::Center;
    tableProps.text.bold = row == 0;
    fui::table(screen.frame(), rowRect, tableProps);
  }
}

void GolfRoundSummaryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, layout.header, playerLabel, entry.course);
  renderUi();
  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), archiveFilename[0] ? tr(STR_GOLF_EXPORT_SEND) : tr(STR_GOLF_EXPORT_SHARE_SUMMARY), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
