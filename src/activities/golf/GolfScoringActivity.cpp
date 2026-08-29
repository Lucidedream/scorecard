#include "GolfScoringActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "GolfNavigation.h"
#include "GolfRoundMenuActivity.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/CourseStore.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfStats.h"

namespace {

constexpr int STATUS_BOTTOM = 46;
constexpr int HOLE_BOTTOM = 156;
constexpr int COUNTERS_BOTTOM = 626;
constexpr int TOTALS_BOTTOM = 696;
constexpr int NINE_BOTTOM = 756;
constexpr uint32_t IDLE_SAVE_MS = 5000;
constexpr uint32_t REPEAT_START_MS = 500;
constexpr uint32_t REPEAT_INTERVAL_MS = 250;

uint8_t segmentMask(const uint8_t digit) {
  static constexpr uint8_t MASKS[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  return digit < 10 ? MASKS[digit] : 0;
}

void drawSegment(const GfxRenderer& renderer, const int x, const int y, const int width, const int height,
                 const bool ink, const bool outline) {
  if (outline) {
    renderer.drawRect(x, y, width, height, 2, ink);
  } else {
    renderer.fillRect(x, y, width, height, ink);
  }
}

void drawDigit(const GfxRenderer& renderer, const int x, const int y, const int height, const uint8_t digit,
               const bool ink, const bool outline) {
  const int thickness = height / 10;
  const int width = height * 11 / 20;
  const int half = height / 2;
  const uint8_t mask = segmentMask(digit);
  if (mask & 0x01) drawSegment(renderer, x + thickness, y, width - thickness * 2, thickness, ink, outline);
  if (mask & 0x02)
    drawSegment(renderer, x + width - thickness, y + thickness, thickness, half - thickness, ink, outline);
  if (mask & 0x04) drawSegment(renderer, x + width - thickness, y + half, thickness, half - thickness, ink, outline);
  if (mask & 0x08)
    drawSegment(renderer, x + thickness, y + height - thickness, width - thickness * 2, thickness, ink, outline);
  if (mask & 0x10) drawSegment(renderer, x, y + half, thickness, half - thickness, ink, outline);
  if (mask & 0x20) drawSegment(renderer, x, y + thickness, thickness, half - thickness, ink, outline);
  if (mask & 0x40)
    drawSegment(renderer, x + thickness, y + half - thickness / 2, width - thickness * 2, thickness, ink, outline);
}

void drawNumber(const GfxRenderer& renderer, const int centerX, const int y, const int height, const uint8_t value,
                const bool ink, const bool outline) {
  const int digitWidth = height * 11 / 20;
  const int gap = height / 10;
  const bool twoDigits = value >= 10;
  const int totalWidth = twoDigits ? digitWidth * 2 + gap : digitWidth;
  int x = centerX - totalWidth / 2;
  if (twoDigits) {
    drawDigit(renderer, x, y, height, value / 10, ink, outline);
    x += digitWidth + gap;
  }
  drawDigit(renderer, x, y, height, value % 10, ink, outline);
}

void formatToPar(const int16_t value, char* output, const size_t size) {
  if (value == 0) {
    snprintf(output, size, "%s", GolfStrings::EVEN);
  } else {
    snprintf(output, size, value > 0 ? "+%d" : "%d", value);
  }
}

void ellipsize(const GfxRenderer& renderer, const char* input, char* output, const size_t size, const int maxWidth) {
  if (size == 0) return;
  snprintf(output, size, "%s", input == nullptr ? "" : input);
  if (renderer.getTextWidth(UI_12_FONT_ID, output, EpdFontFamily::BOLD) <= maxWidth) return;
  size_t length = strlen(output);
  while (length > 3) {
    output[--length] = '\0';
    output[length - 3] = '.';
    output[length - 2] = '.';
    output[length - 1] = '.';
    if (renderer.getTextWidth(UI_12_FONT_ID, output, EpdFontFamily::BOLD) <= maxWidth) return;
  }
}

}  // namespace

void GolfScoringActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  loadCourseDisplayData();
  requestUpdate();
}

void GolfScoringActivity::onExit() { Activity::onExit(); }

void GolfScoringActivity::loadCourseDisplayData() {
  GolfCourse course{};
  if (!CourseStore::findByName(GOLF_ROUND_STORE.getRound().courseName, course) || !course.hasSi) return;
  memcpy(si, course.si, sizeof(si));
  hasSi = true;
}

bool GolfScoringActivity::flushDirty() {
  const bool success = flushGolfRoundIfDirty();
  if (!success) {
    saveFailed = true;
    requestUpdate();
  }
  return success;
}

void GolfScoringActivity::mutateCounter(const bool increment) {
  GolfMutationResult result{};
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    const bool seeded = seedGolfHoleAtPar(round, round.currentHole);
    result = increment ? incrementGolfCounter(round, round.currentHole, focusedField)
                       : decrementGolfCounter(round, round.currentHole, focusedField);
    if (seeded && !result.changed) result.changed = true;
    if (result.carriedIn100) carryNotice = GolfStrings::IN100_CARRY;
    if (result.loweredPutts) carryNotice = GolfStrings::PUTTS_CARRY;
  }
  if (result.changed) {
    markGolfRoundDirty();
    lastCounterChangeAt = millis();
    saveFailed = false;
  }
  if (result.changed) requestUpdate();
}

void GolfScoringActivity::handleConfirm() {
  GolfConfirmAction action = GolfConfirmAction::CycleFocus;
  {
    RenderLock lock(*this);
    const GolfRound& round = GOLF_ROUND_STORE.getRound();
    const uint8_t hole = round.currentHole;
    const bool logged = golfHoleScore(round, hole) != 0;
    const bool canCommit = !logged && round.par[hole] >= 3;
    action = golfConfirmPress(focusedField, logged, canCommit);
    if (action == GolfConfirmAction::CycleFocus) focusedField = nextGolfField(focusedField);
  }
  if (action == GolfConfirmAction::CommitAndAdvance) {
    commitAndAdvance();
    return;
  }
  if (action == GolfConfirmAction::AdvanceWithoutCommit) {
    changeHole(1);
    return;
  }
  requestUpdate();
}

void GolfScoringActivity::commitAndAdvance() {
  bool committed = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    committed = seedGolfHoleAtPar(round, round.currentHole);
  }
  if (committed) {
    markGolfRoundDirty();
    lastCounterChangeAt = millis();
    saveFailed = false;
  }
  changeHole(1);
}

void GolfScoringActivity::changeHole(const int delta) {
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    round.currentHole = static_cast<uint8_t>((round.currentHole + round.holeCount + delta) % round.holeCount);
    focusedField = GolfField::Putts;
    carryNotice = nullptr;
  }
  markGolfRoundDirty();
  flushDirty();
  requestUpdate();
}

void GolfScoringActivity::openRoundMenu() {
  auto menu = makeUniqueNoThrow<GolfRoundMenuActivity>(renderer, mappedInput);
  if (!menu) {
    LOG_ERR("GOLF", "OOM: round menu");
    return;
  }
  startActivityForResult(std::move(menu), nullptr);
}

bool GolfScoringActivity::handleHomeGesture() {
  flushDirty();
  return false;
}

void GolfScoringActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    openRoundMenu();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeHole(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    changeHole(1);
    return;
  }
  const bool confirmReleased = mappedInput.wasReleased(MappedInputManager::Button::Confirm);
  bool powerCyclesField = SETTINGS.shortPwrBtn != CrossPointSettings::SHORT_PWRBTN::SLEEP;
#if FREEINK_CAP_TOUCH
  // X4 Pro turns a configured Power-as-Confirm click into a delayed Confirm event.
  // Waiting for that event avoids counting the same physical click twice.
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PWR_CONFIRM && BoardConfig::isX4Pro()) {
    powerCyclesField = false;
  }
#endif
  powerCyclesField = powerCyclesField && mappedInput.wasReleased(MappedInputManager::Button::Power);
  if (confirmReleased || powerCyclesField) {
    handleConfirm();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    lastRepeatAt = millis();
    mutateCounter(true);
    return;
  }
  if (mappedInput.isPressed(MappedInputManager::Button::Up) && mappedInput.getHeldTime() >= REPEAT_START_MS &&
      millis() - lastRepeatAt >= REPEAT_INTERVAL_MS) {
    lastRepeatAt = millis();
    mutateCounter(true);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    mutateCounter(false);
    return;
  }
  if (isGolfRoundDirty() && millis() - lastCounterChangeAt >= IDLE_SAVE_MS) flushDirty();
}

void GolfScoringActivity::drawStatusBar() const {
  char title[40];
  char time[9];
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  ellipsize(renderer, saveFailed ? GolfStrings::SAVE_ERROR : round.courseName, title, sizeof(title),
            renderer.getScreenWidth() - 135);
  const char* right = nullptr;
  if (halClock.isAvailable() && halClock.formatTime(time, sizeof(time))) right = time;
  GUI.drawHeader(renderer, Rect{0, 0, renderer.getScreenWidth(), STATUS_BOTTOM}, title, right);
}

void GolfScoringActivity::drawHoleBand() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const uint8_t hole = round.currentHole;
  renderer.drawText(UI_10_FONT_ID, 18, 61, GolfStrings::HOLE, true, EpdFontFamily::BOLD);
  drawNumber(renderer, 125, 82, 58, hole + 1, true, false);
  char text[24];
  if (round.par[hole] != 0) {
    snprintf(text, sizeof(text), "%s %u", GolfStrings::PAR, round.par[hole]);
    renderer.drawText(UI_12_FONT_ID, renderer.getScreenWidth() - 20 - renderer.getTextWidth(UI_12_FONT_ID, text), 66,
                      text, true, EpdFontFamily::BOLD);
  }
  int y = 100;
  if (round.yards[hole] != 0) {
    snprintf(text, sizeof(text), "%u %s", round.yards[hole], GolfStrings::YARDS);
    renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - 20 - renderer.getTextWidth(UI_10_FONT_ID, text), y,
                      text);
    y += 24;
  }
  if (hasSi) {
    snprintf(text, sizeof(text), "%s %u", GolfStrings::SI, si[hole]);
    renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - 20 - renderer.getTextWidth(UI_10_FONT_ID, text), y,
                      text);
  }
  renderer.drawLine(0, HOLE_BOTTOM - 1, renderer.getScreenWidth(), HOLE_BOTTOM - 1);
}

void GolfScoringActivity::drawCounters() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const uint8_t hole = round.currentHole;
  const bool entered = golfHoleScore(round, hole) != 0;
  const bool preseed = !entered && round.par[hole] >= 3;
  const uint8_t values[] = {preseed ? static_cast<uint8_t>(2) : round.putts[hole],
                            preseed ? static_cast<uint8_t>(2) : round.in100[hole],
                            preseed ? static_cast<uint8_t>(round.par[hole] - 2) : round.out100[hole]};
  const char* labels[] = {GolfStrings::PUTTS, GolfStrings::IN100, GolfStrings::OUT100};
  int top = HOLE_BOTTOM;
  for (uint8_t index = 0; index < 3; ++index) {
    const bool focused = focusedField == static_cast<GolfField>(index);
    const int height = focused ? 170 : 150;
    const bool inverse = focused;
    if (inverse) renderer.fillRect(0, top, renderer.getScreenWidth(), height, true);
    renderer.drawText(UI_10_FONT_ID, 20, top + 14, labels[index], !inverse, EpdFontFamily::BOLD);
    if (index == 2) {
      char badge[24];
      const uint16_t total = static_cast<uint16_t>(values[1]) + values[2];
      if (round.par[hole] != 0) {
        char toPar[8];
        formatToPar(static_cast<int16_t>(total) - round.par[hole], toPar, sizeof(toPar));
        snprintf(badge, sizeof(badge), "%s %u %s", GolfStrings::TOTAL, total, toPar);
      } else {
        snprintf(badge, sizeof(badge), "%s %u", GolfStrings::TOTAL, total);
      }
      renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - 22 - renderer.getTextWidth(UI_10_FONT_ID, badge),
                        top + 14, badge, !inverse, EpdFontFamily::BOLD);
    } else if (carryNotice != nullptr && index == static_cast<uint8_t>(focusedField)) {
      renderer.drawText(UI_10_FONT_ID,
                        renderer.getScreenWidth() - 22 - renderer.getTextWidth(UI_10_FONT_ID, carryNotice), top + 14,
                        carryNotice, !inverse, EpdFontFamily::BOLD);
    }
    const int digitHeight = focused ? 100 : 66;
    drawNumber(renderer, renderer.getScreenWidth() / 2, top + (height - digitHeight) / 2 + 12, digitHeight,
               values[index], !inverse, preseed);
    renderer.drawLine(0, top + height - 1, renderer.getScreenWidth(), top + height - 1, !inverse);
    top += height;
  }
}

void GolfScoringActivity::drawTotals() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const int width = renderer.getScreenWidth() / 3;
  const bool hasPar = golfHasPar(round);
  const char* labels[] = {GolfStrings::THRU, GolfStrings::SCORE, hasPar ? GolfStrings::TO_PAR : ""};
  char values[3][8];
  snprintf(values[0], sizeof(values[0]), "%u", golfThru(round));
  snprintf(values[1], sizeof(values[1]), "%u", golfScore(round));
  if (hasPar) {
    formatToPar(golfToPar(round), values[2], sizeof(values[2]));
  } else {
    values[2][0] = '\0';
  }
  for (uint8_t index = 0; index < 3; ++index) {
    const int x = index * width;
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, labels[index], EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, x + (width - labelWidth) / 2, COUNTERS_BOTTOM + 8, labels[index], true,
                      EpdFontFamily::BOLD);
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, values[index], EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, x + (width - textWidth) / 2, COUNTERS_BOTTOM + 35, values[index], true,
                      EpdFontFamily::BOLD);
    if (index != 0) renderer.drawLine(x, COUNTERS_BOTTOM + 6, x, TOTALS_BOTTOM - 6);
  }
  renderer.drawLine(0, TOTALS_BOTTOM - 1, renderer.getScreenWidth(), TOTALS_BOTTOM - 1);
}

void GolfScoringActivity::drawNineStrip() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const uint8_t first = static_cast<uint8_t>((round.currentHole / 9) * 9);
  const int cellWidth = renderer.getScreenWidth() / 9;
  for (uint8_t offset = 0; offset < 9 && first + offset < round.holeCount; ++offset) {
    const uint8_t hole = first + offset;
    const int x = offset * cellWidth;
    const bool current = hole == round.currentHole;
    if (current) renderer.fillRect(x, TOTALS_BOTTOM, cellWidth, NINE_BOTTOM - TOTALS_BOTTOM, true);
    char number[4];
    snprintf(number, sizeof(number), "%u", hole + 1);
    renderer.drawText(SMALL_FONT_ID, x + (cellWidth - renderer.getTextWidth(SMALL_FONT_ID, number)) / 2,
                      TOTALS_BOTTOM + 5, number, !current, EpdFontFamily::BOLD);
    const uint16_t score = golfHoleScore(round, hole);
    if (score == 0) {
      snprintf(number, sizeof(number), ".");
    } else {
      snprintf(number, sizeof(number), "%u", score);
    }
    renderer.drawText(UI_10_FONT_ID, x + (cellWidth - renderer.getTextWidth(UI_10_FONT_ID, number)) / 2,
                      TOTALS_BOTTOM + 28, number, !current, EpdFontFamily::BOLD);
  }
}

void GolfScoringActivity::drawFooter() const {
  const auto labels = mappedInput.mapDirectionalLabels(GolfStrings::MENU, GolfStrings::FIELD_POWER, GolfStrings::LEFT,
                                                       GolfStrings::RIGHT, GolfStrings::PLUS, GolfStrings::MINUS);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfScoringActivity::render(RenderLock&&) {
  renderer.clearScreen();
  drawStatusBar();
  drawHoleBand();
  drawCounters();
  drawTotals();
  drawNineStrip();
  drawFooter();
  ++paintCount;
  if (paintCount % 8 == 0) renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  if (carryNotice != nullptr) {
    carryNotice = nullptr;
    requestUpdate();
  }
}

#endif
