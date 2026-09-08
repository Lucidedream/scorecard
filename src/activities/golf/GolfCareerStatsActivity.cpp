#include "GolfCareerStatsActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfRoundFile.h"
#include "golf/RoundArchive.h"

namespace fui = freeink::ui;

namespace {

constexpr char INDEX_PATH[] = "/golf/rounds/index.csv";

void formatTenths(const uint32_t value, char* out, const size_t size) {
  snprintf(out, size, tr(STR_GOLF_DECIMAL_FORMAT), static_cast<unsigned long>(value / 10),
           static_cast<unsigned long>(value % 10));
}

void formatSignedTenths(const int32_t value, char* out, const size_t size) {
  const uint32_t mag = static_cast<uint32_t>(value < 0 ? -value : value);
  const char* format = value > 0   ? tr(STR_GOLF_POSITIVE_DECIMAL_FORMAT)
                       : value < 0 ? tr(STR_GOLF_NEGATIVE_DECIMAL_FORMAT)
                                   : tr(STR_GOLF_DECIMAL_FORMAT);
  snprintf(out, size, format, static_cast<unsigned long>(mag / 10), static_cast<unsigned long>(mag % 10));
}

void formatHundredths(const uint32_t value, char* out, const size_t size) {
  snprintf(out, size, tr(STR_GOLF_DECIMAL2_FORMAT), static_cast<unsigned long>(value / 100),
           static_cast<unsigned long>(value % 100));
}

void formatSignedHundredths(const int32_t value, char* out, const size_t size) {
  const uint32_t mag = static_cast<uint32_t>(value < 0 ? -value : value);
  const char* format = value > 0   ? tr(STR_GOLF_POSITIVE_DECIMAL2_FORMAT)
                       : value < 0 ? tr(STR_GOLF_NEGATIVE_DECIMAL2_FORMAT)
                                   : tr(STR_GOLF_DECIMAL2_FORMAT);
  snprintf(out, size, format, static_cast<unsigned long>(mag / 100), static_cast<unsigned long>(mag % 100));
}

// Rounded average * scale, e.g. scale 10 for one decimal, 100 for two.
uint32_t scaledAverage(const uint32_t sum, const uint32_t count, const uint32_t scale) {
  if (count == 0) return 0;
  return (sum * scale + count / 2) / count;
}

}  // namespace

GolfCareerStatsActivity::GolfCareerStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const uint8_t playerSlot, const char* playerName)
    : Activity("GolfCareerStats", renderer, mappedInput), playerSlot(playerSlot) {
  const char* source = playerName;
  if (source == nullptr && playerSlot < GolfRound::MAX_PLAYERS) source = GOLF_DEFAULT_PLAYER_NAMES[playerSlot];
  if (source != nullptr) snprintf(this->playerName, sizeof(this->playerName), "%s", source);
}

void GolfCareerStatsActivity::onEnter() {
  Activity::onEnter();
  golfFormatPlayerLabel(playerSlot, playerName, tr(STR_GOLF_PLAYER_LABEL_FORMAT), playerLabel, sizeof(playerLabel));
  firstPaint = true;

  scratch = makeUniqueNoThrow<ScanScratch>();
  if (!scratch) {
    LOG_ERR("GOLF", "OOM: career stats scan scratch (%u bytes)", static_cast<unsigned>(sizeof(ScanScratch)));
    scanError = true;
    scanning = false;
    requestUpdate();
    return;
  }

  // Paint the "Reading rounds…" frame, then do the synchronous scan on this
  // task (~1-3 s for a few dozen rounds is acceptable, CONTRACTS-V2 §33.4).
  requestUpdateAndWait();
  runScan();
  scanning = false;
  topItem = 0;
  requestUpdate();
}

void GolfCareerStatsActivity::logMalformed(const uint32_t lineNumber, void*) {
  LOG_ERR("GOLF", "Malformed index row at line %lu", static_cast<unsigned long>(lineNumber));
}

bool GolfCareerStatsActivity::streamIndex() {
  if (!Storage.exists(INDEX_PATH)) return true;  // no archived rounds is not an error
  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;
  while (file.available() > 0) {
    const int bytesRead = file.read(scratch->chunk, sizeof(scratch->chunk));
    if (bytesRead <= 0) return false;
    scratch->history.feed(scratch->chunk, static_cast<size_t>(bytesRead), &GolfCareerStatsActivity::logMalformed, this);
  }
  scratch->history.finish(&GolfCareerStatsActivity::logMalformed, this);
  return true;
}

bool GolfCareerStatsActivity::resolveRoundFile(const uint8_t newestIndex) {
  HalFile file;
  if (!Storage.openFileForRead("GOLF", INDEX_PATH, file)) return false;
  if (!scratch->locator.reset(playerSlot, newestIndex, scratch->history.totalValidRows())) return false;
  while (file.available() > 0) {
    const int bytesRead = file.read(scratch->chunk, sizeof(scratch->chunk));
    if (bytesRead <= 0) break;
    scratch->locator.feed(scratch->chunk, static_cast<size_t>(bytesRead));
  }
  if (!scratch->locator.finish()) return false;
  snprintf(scratch->path, sizeof(scratch->path), "/golf/rounds/%s", scratch->locator.filename());
  return loadGolfRoundFile(scratch->path, scratch->round);
}

void GolfCareerStatsActivity::runScan() {
  if (!scratch->history.reset(playerSlot)) {
    LOG_ERR("GOLF", "Career stats received invalid player slot %u", playerSlot);
    scanError = true;
    return;
  }
  if (!RoundArchive::recoverIndex(scratch->recovery)) {
    LOG_ERR("GOLF", "Career stats refused unrecovered index.csv");
    scanError = true;
    return;
  }
  if (!streamIndex()) {
    scanError = true;
    return;
  }

  roundsToRead = scratch->history.count();
  requestUpdateAndWait();  // repaint with the real "Reading N rounds…" count

  for (uint16_t index = 0; index < roundsToRead; ++index) {
    if (resolveRoundFile(static_cast<uint8_t>(index))) {
      golfFoldCareerRound(scratch->round, playerSlot, tally);
    }
  }
}

void GolfCareerStatsActivity::page(const int direction) {
  if (!overflow || visibleCount <= 0) return;
  {
    RenderLock lock(*this);
    if (direction > 0) {
      topItem += visibleCount;
      if (topItem >= bodyRowCount) topItem = 0;
    } else {
      topItem -= visibleCount;
      if (topItem < 0) {
        const int pages = (bodyRowCount + visibleCount - 1) / visibleCount;
        topItem = pages > 0 ? (pages - 1) * visibleCount : 0;
      }
    }
  }
  requestUpdate();
}

void GolfCareerStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
    return;
  }
  if (scanning) return;
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    page(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    page(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    page(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) page(1);
}

void GolfCareerStatsActivity::buildBodyRows() {
  bodyRowCount = 0;
  auto add = [&](const RowSpec& spec) {
    if (bodyRowCount < MAX_BODY_ROWS) bodyRows[bodyRowCount++] = spec;
  };
  auto header = [&](const char* label) {
    RowSpec row{};
    row.isHeader = true;
    snprintf(row.label, sizeof(row.label), "%s", label);
    add(row);
  };
  const char* dash = tr(STR_GOLF_EM_DASH);

  // --- Score shape -------------------------------------------------------
  header(tr(STR_GOLF_STATS_SCORE_SHAPE));
  const char* shapeLabels[6] = {tr(STR_GOLF_STATS_EAGLE), tr(STR_GOLF_STATS_BIRDIE), tr(STR_GOLF_STATS_PAR),
                                tr(STR_GOLF_STATS_BOGEY), tr(STR_GOLF_STATS_DOUBLE), tr(STR_GOLF_STATS_TRIPLE)};
  uint16_t distSum = 0;
  uint16_t distMax = 0;
  for (const uint16_t bucket : tally.dist) {
    distSum = static_cast<uint16_t>(distSum + bucket);
    if (bucket > distMax) distMax = bucket;
  }
  for (uint8_t i = 0; i < 6; ++i) {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", shapeLabels[i]);
    snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.dist[i]));
    golfFormatReviewPercent(tally.dist[i], distSum, tr(STR_GOLF_PERCENT_FORMAT), row.right, sizeof(row.right));
    row.hasBar = true;
    row.barCount = tally.dist[i];
    row.barMax = distMax;
    add(row);
  }

  // --- By par ----------------------------------------------------------
  header(tr(STR_GOLF_STATS_BY_PAR));
  const char* parLabels[3] = {tr(STR_GOLF_STATS_PAR3), tr(STR_GOLF_STATS_PAR4), tr(STR_GOLF_STATS_PAR5)};
  for (uint8_t t = 0; t < 3; ++t) {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", parLabels[t]);
    if (tally.parHoles[t] == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
      snprintf(row.right, sizeof(row.right), "%s", dash);
    } else {
      const uint32_t avg = scaledAverage(tally.parStrokes[t], tally.parHoles[t], 100);
      formatHundredths(avg, row.value, sizeof(row.value));
      formatSignedHundredths(static_cast<int32_t>(avg) - static_cast<int32_t>((t + 3) * 100), row.right,
                             sizeof(row.right));
    }
    add(row);
  }

  // --- Around the green ---------------------------------------------------
  header(tr(STR_GOLF_STATS_AROUND_GREEN));
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_SCRAMBLING));
    if (tally.scrambleChances == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      golfFormatReviewPercent(tally.scrambles, tally.scrambleChances, tr(STR_GOLF_PERCENT_FORMAT), row.value,
                              sizeof(row.value));
    }
    snprintf(row.right, sizeof(row.right), tr(STR_GOLF_CAREER_RATIO_FORMAT), static_cast<unsigned>(tally.scrambles),
             static_cast<unsigned>(tally.scrambleChances));
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_SAND_SAVES));
    if (tally.sandSaveChances == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      golfFormatReviewPercent(tally.sandSaves, tally.sandSaveChances, tr(STR_GOLF_PERCENT_FORMAT), row.value,
                              sizeof(row.value));
    }
    snprintf(row.right, sizeof(row.right), tr(STR_GOLF_CAREER_RATIO_FORMAT), static_cast<unsigned>(tally.sandSaves),
             static_cast<unsigned>(tally.sandSaveChances));
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_PUTTS_PER_GIR));
    if (tally.girHoles == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      formatHundredths(scaledAverage(tally.puttsOnGir, tally.girHoles, 100), row.value, sizeof(row.value));
    }
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_THREE_PUTT));
    if (tally.holesPlayed == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      golfFormatReviewPercent(tally.threePuttHoles, tally.holesPlayed, tr(STR_GOLF_PERCENT_FORMAT), row.value,
                              sizeof(row.value));
    }
    snprintf(row.right, sizeof(row.right), "%u", static_cast<unsigned>(tally.threePuttHoles));
    add(row);
  }

  // --- Records ---------------------------------------------------------
  header(tr(STR_GOLF_STATS_RECORDS));
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_LOWEST_ROUND));
    if (tally.lowestRound == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.lowestRound));
      snprintf(row.right, sizeof(row.right), "%s", tally.lowestCourse);
    }
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_LOWEST_NINE));
    if (tally.lowestNine == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.lowestNine));
    }
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_FEWEST_PUTTS));
    if (tally.fewestPutts == 0) {
      snprintf(row.value, sizeof(row.value), "%s", dash);
    } else {
      snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.fewestPutts));
    }
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_MOST_PARS));
    snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.mostPars));
    snprintf(row.right, sizeof(row.right), "%s", tr(STR_GOLF_STATS_IN_A_ROUND));
    add(row);
  }
  {
    RowSpec row{};
    snprintf(row.label, sizeof(row.label), "%s", tr(STR_GOLF_STATS_BOGEY_FREE));
    snprintf(row.value, sizeof(row.value), "%u", static_cast<unsigned>(tally.longestBogeyFreeRun));
    snprintf(row.right, sizeof(row.right), "%s", tr(STR_GOLF_STATS_HOLES_WORD));
    add(row);
  }
}

void GolfCareerStatsActivity::drawHeroBand(const fui::Rect rect) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  const int right = rect.x + rect.width - padding;
  const int smallLh = renderer.getLineHeight(UI_10_FONT_ID);

  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + 4, tr(STR_GOLF_SCORING_AVERAGE), true,
                    EpdFontFamily::BOLD);

  char value[16] = "—";
  if (tally.rounds > 0) formatTenths(scaledAverage(tally.strokes, tally.rounds, 10), value, sizeof(value));
  renderer.drawText(NOTOSANS_18_FONT_ID, rect.x + padding, rect.y + smallLh + 2, value, true, EpdFontFamily::BOLD);

  // Right note: to-par (only when every folded round had par) then round count.
  int noteY = rect.y + 4;
  if (tally.rounds > 0 && tally.parRounds == tally.rounds) {
    const int32_t bias = static_cast<int32_t>(tally.rounds / 2);
    const int32_t tenths =
        (tally.toParTotal * 10 + (tally.toParTotal >= 0 ? bias : -bias)) / static_cast<int32_t>(tally.rounds);
    char toPar[12];
    formatSignedTenths(tenths, toPar, sizeof(toPar));
    char line[24];
    snprintf(line, sizeof(line), tr(STR_GOLF_STATS_TO_PAR_NOTE_FORMAT), toPar);
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), noteY, line);
    noteY += smallLh;
  }
  char rounds[24];
  snprintf(rounds, sizeof(rounds), tr(STR_GOLF_STATS_OVER_ROUNDS_FORMAT), static_cast<unsigned>(tally.rounds));
  renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, rounds), noteY, rounds);

  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, 2, true);
}

void GolfCareerStatsActivity::drawBodyRow(const fui::Rect rect, const RowSpec& row) const {
  const int padding = golfui::minValue(SIDE_PADDING, static_cast<int16_t>(rect.width / 8));
  if (row.isHeader) {
    renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
    const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, rect.x + padding, textY, row.label, false, EpdFontFamily::BOLD);
    return;
  }

  const int right = rect.x + rect.width - padding;
  const int labelY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, rect.x + padding, labelY, row.label);

  const int valueY = rect.y + (rect.height - renderer.getLineHeight(NOTOSANS_16_FONT_ID)) / 2;
  const int valueWidth = renderer.getTextWidth(NOTOSANS_16_FONT_ID, row.value, EpdFontFamily::BOLD);
  renderer.drawText(NOTOSANS_16_FONT_ID, right - valueWidth, valueY, row.value, true, EpdFontFamily::BOLD);

  if (row.right[0] != '\0') {
    const int contextRight = right - valueWidth - 12;
    const int contextWidth = renderer.getTextWidth(UI_10_FONT_ID, row.right);
    renderer.drawText(UI_10_FONT_ID, golfui::maxValue(rect.x + padding, contextRight - contextWidth), labelY,
                      row.right);
  }

  if (row.hasBar && row.barMax > 0) {
    const int track = rect.width - 2 * padding;
    const int barWidth = static_cast<int>(static_cast<uint32_t>(track) * row.barCount / row.barMax);
    if (barWidth > 0) renderer.fillRect(rect.x + padding, rect.y + rect.height - 5, barWidth, 3, true);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfCareerStatsActivity::drawFooter() const {
  const char* more = (!scanning && overflow) ? tr(STR_GOLF_FOOTER_MORE) : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", more);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfCareerStatsActivity::finishPaint() {
  renderer.displayBuffer(firstPaint ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  firstPaint = false;
}

void GolfCareerStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, metrics.topPadding);
  golfui::drawHeader(renderer, chrome.header, playerLabel);

  const fui::Rect body = chrome.body;
  const int centerLh = renderer.getLineHeight(UI_12_FONT_ID);

  if (scanning) {
    char line[32];
    if (roundsToRead == 0) {
      snprintf(line, sizeof(line), "%s", tr(STR_GOLF_STATS_READING));
    } else {
      snprintf(line, sizeof(line), tr(STR_GOLF_STATS_READING_FORMAT), static_cast<unsigned>(roundsToRead));
    }
    renderer.drawCenteredText(UI_12_FONT_ID, body.y + (body.height - centerLh) / 2, line, true);
    drawFooter();
    finishPaint();
    return;
  }

  if (scanError) {
    renderer.drawCenteredText(UI_12_FONT_ID, body.y + (body.height - centerLh) / 2, tr(STR_GOLF_STATS_ERROR), true);
    overflow = false;
    drawFooter();
    finishPaint();
    return;
  }

  if (tally.rounds == 0) {
    renderer.drawCenteredText(UI_12_FONT_ID, body.y + (body.height - centerLh) / 2, tr(STR_GOLF_NO_ROUNDS_YET), true);
    overflow = false;
    drawFooter();
    finishPaint();
    return;
  }

  buildBodyRows();

  const int heroHeight = golfui::clampValue(
      renderer.getLineHeight(NOTOSANS_18_FONT_ID) + renderer.getLineHeight(UI_10_FONT_ID) + 12, 60, body.height / 2);
  const fui::Rect hero{body.x, body.y, body.width, static_cast<int16_t>(heroHeight)};
  drawHeroBand(hero);

  const int scrollTop = body.y + heroHeight;
  const int scrollHeight = body.y + body.height - scrollTop;
  const int rowHeight = golfui::clampValue(renderer.getLineHeight(UI_12_FONT_ID) + 14, 30, 44);
  visibleCount = golfui::maxValue(1, scrollHeight / rowHeight);
  overflow = bodyRowCount > visibleCount;
  if (topItem >= bodyRowCount || topItem < 0) topItem = 0;

  for (int i = 0; i < visibleCount; ++i) {
    const int itemIndex = topItem + i;
    if (itemIndex >= bodyRowCount) break;
    const fui::Rect rowRect{body.x, static_cast<int16_t>(scrollTop + i * rowHeight), body.width,
                            static_cast<int16_t>(rowHeight)};
    drawBodyRow(rowRect, bodyRows[itemIndex]);
  }

  drawFooter();
  finishPaint();
}

#endif
