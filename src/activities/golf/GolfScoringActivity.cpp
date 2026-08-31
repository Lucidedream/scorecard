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
#include "GolfLargeNumber.h"
#include "GolfNavigation.h"
#include "GolfRoundMenuActivity.h"
#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/CourseStore.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfStats.h"

namespace {

constexpr int STATUS_BOTTOM = 46;
constexpr int HOLE_BOTTOM = 156;
constexpr int COUNTERS_BOTTOM = 626;
constexpr int TOTALS_BOTTOM = 696;
constexpr uint32_t IDLE_SAVE_MS = 5000;
constexpr uint32_t REPEAT_START_MS = 500;
constexpr uint32_t REPEAT_INTERVAL_MS = 250;

// The three counter rows share HOLE_BOTTOM..countersRegionBottom() in a
// 150:150:170 (unfocused:unfocused:focused) split; 470 is that sum at full height.
constexpr int COUNTER_UNFOCUSED_WEIGHT = 150;
constexpr int COUNTER_REGION_WEIGHT = 470;

// Inverted "PENALTY +N" band, present only on a hole that has penalties. Its
// height comes out of the counter region so the totals strip, nine strip and
// footer sit at the same y in both layouts.
constexpr int PENALTY_BAND_HEIGHT = 42;

// Markers ("H" / "OB") drawn to the right of a field's number. No monospace
// font is bundled, so the small UI font stands in for the mock's mono style.
constexpr int MARKER_FONT_ID = UI_10_FONT_ID;
constexpr int MARKER_GUTTER = 26;        // gap between the number and the first marker
constexpr int MARKER_RIGHT_MARGIN = 20;  // right inset for the marker run

// Penalty picker sheet, anchored to the bottom edge over the scoring screen.
constexpr int SHEET_BORDER = 3;
constexpr int SHEET_TITLE_H = 44;
constexpr int SHEET_NOTICE_H = 32;   // "hole is full" strip, shown only at the cap
constexpr int SHEET_OPTION_H = 104;  // each of the two option rows
constexpr int SHEET_PAD_X = 20;
constexpr int SHEET_TAG_COL_W = 72;  // centred "H" / "OB" column
constexpr int SHEET_TEXT_X = 92;     // title / subtitle left edge

// A Confirm hold this long opens the picker when Confirm is the field-cycle
// button (i.e. Power cannot cycle, §12.6).
constexpr unsigned long PICKER_LONGPRESS_MS = 500;

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

bool GolfScoringActivity::ensureHoleSeeded() {
  GolfRound& round = GOLF_ROUND_STORE.getRound();
  return seedGolfHoleAtPar(round, round.currentHole);
}

void GolfScoringActivity::mutateCounter(const bool increment) {
  GolfMutationResult result{};
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    const bool seeded = ensureHoleSeeded();
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

void GolfScoringActivity::removeOrDecrement() {
  // Down on a field that has markers removes that field's most recent marker,
  // its shot, and its penalty strokes. Otherwise it is a plain decrement,
  // exactly as before. CONTRACTS-V2 §12.4.
  GolfPenaltyMutationStatus status;
  bool seeded = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    seeded = ensureHoleSeeded();
    status = golfRemoveLatestPenalty(round, round.currentHole, focusedField);
  }
  if (status == GolfPenaltyMutationStatus::NoMarker) {
    // No marker on this field: fall through to a plain decrement, which seeds
    // and logs on its own path.
    mutateCounter(false);
    return;
  }
  if (seeded || status == GolfPenaltyMutationStatus::Changed) {
    markGolfRoundDirty();
    lastCounterChangeAt = millis();
    saveFailed = false;
    requestUpdate();
    return;
  }
  LOG_ERR("GOLF", "penalty remove rejected: %d", static_cast<int>(status));
  requestUpdate();
}

bool GolfScoringActivity::powerCyclesField() const {
  bool cycles = SETTINGS.shortPwrBtn != CrossPointSettings::SHORT_PWRBTN::SLEEP;
#if FREEINK_CAP_TOUCH
  // X4 Pro turns a configured Power-as-Confirm click into a delayed Confirm
  // event, so Power is not free to cycle the field there either.
  if (SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PWR_CONFIRM && BoardConfig::isX4Pro()) {
    cycles = false;
  }
#endif
  return cycles;
}

void GolfScoringActivity::openPenaltyPicker() {
  pickerKind = GolfPenaltyKind::Hazard;
  pickerHoleFull = false;
  pickerOpen = true;
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  requestUpdate();
}

void GolfScoringActivity::closePenaltyPicker() {
  // Cancel path: touches no round state at all.
  pickerOpen = false;
  pickerHoleFull = false;
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  requestUpdate();
}

void GolfScoringActivity::handlePickerInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closePenaltyPicker();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) && pickerKind != GolfPenaltyKind::Hazard) {
    pickerKind = GolfPenaltyKind::Hazard;
    pickerHoleFull = false;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && pickerKind != GolfPenaltyKind::Ob) {
    pickerKind = GolfPenaltyKind::Ob;
    pickerHoleFull = false;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    applyPenaltyPick();
    return;
  }
}

void GolfScoringActivity::applyPenaltyPick() {
  GolfPenaltyMutationStatus status;
  bool seeded = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    seeded = ensureHoleSeeded();
    status = golfAppendPenalty(round, round.currentHole, focusedField, pickerKind);
  }
  if (seeded || status == GolfPenaltyMutationStatus::Changed) {
    markGolfRoundDirty();
    lastCounterChangeAt = millis();
    saveFailed = false;
  }
  switch (status) {
    case GolfPenaltyMutationStatus::Changed:
      closePenaltyPicker();
      break;
    case GolfPenaltyMutationStatus::HoleFull:
      // Never silently drop the event: keep the sheet open and say so.
      pickerHoleFull = true;
      requestUpdate();
      break;
    default:
      LOG_ERR("GOLF", "penalty add rejected: %d", static_cast<int>(status));
      closePenaltyPicker();
      break;
  }
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
    committed = ensureHoleSeeded();
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
  if (pickerOpen) {
    handlePickerInput();
    return;
  }

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

  // Field control (CONTRACTS-V2 §12.6). powerCyclesField() partitions every
  // state into exactly one of two bindings, so a field-cycle button is always
  // present and the screen can never get stuck:
  //   powerCyclesField()  -> Power cycles the field; Confirm opens the picker.
  //   !powerCyclesField() -> Confirm cycles the field; a Confirm long-press
  //                          opens the picker. Power keeps its stock sleep role.
  // Hole advance stays with the field-cycle button via handleConfirm() (§10).
  if (powerCyclesField()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Power)) {
      handleConfirm();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      openPenaltyPicker();
      return;
    }
  } else {
    if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, PICKER_LONGPRESS_MS)) {
      openPenaltyPicker();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      handleConfirm();
      return;
    }
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
    removeOrDecrement();
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
  golfDrawLargeNumber(renderer, 125, 82, 58, hole + 1);
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

int GolfScoringActivity::countersRegionBottom() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  return round.penaltyCount[round.currentHole] > 0 ? COUNTERS_BOTTOM - PENALTY_BAND_HEIGHT : COUNTERS_BOTTOM;
}

int GolfScoringActivity::formatHoleMarkers(const GolfRound& round, const uint8_t hole, const int fieldFilter,
                                           const int maxWidth, char* out, const size_t size) const {
  out[0] = '\0';
  const uint8_t count = round.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? round.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  const char* tokens[GolfRound::MAX_PENALTIES_PER_HOLE];
  uint8_t n = 0;
  for (uint8_t i = 0; i < count; ++i) {
    GolfPenaltyEvent event{};
    if (!golfPenaltyEventAt(round, hole, i, event)) continue;
    if (fieldFilter >= 0 && static_cast<int>(event.field) != fieldFilter) continue;
    tokens[n++] = event.kind == GolfPenaltyKind::Ob ? GolfStrings::OB_TAG : GolfStrings::HAZARD_TAG;
  }
  if (n == 0) return 0;

  // Try the whole run, then progressively fewer markers with a "+N" tail, and
  // keep the first that fits (the "+8" tail always fits).
  for (uint8_t shown = n;; --shown) {
    char buf[48];
    size_t len = 0;
    for (uint8_t i = 0; i < shown && len < sizeof(buf); ++i) {
      len += snprintf(buf + len, sizeof(buf) - len, "%s%s", i == 0 ? "" : " ", tokens[i]);
    }
    if (shown < n && len < sizeof(buf)) {
      len += snprintf(buf + len, sizeof(buf) - len, "%s+%u", shown == 0 ? "" : " ", n - shown);
    }
    const int width = renderer.getTextWidth(MARKER_FONT_ID, buf, EpdFontFamily::BOLD);
    if (width <= maxWidth || shown == 0) {
      snprintf(out, size, "%s", buf);
      return width;
    }
  }
}

void GolfScoringActivity::drawCounters() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const uint8_t hole = round.currentHole;
  const int screenWidth = renderer.getScreenWidth();
  const bool entered = golfHoleScore(round, hole) != 0;
  const bool preseed = !entered && round.par[hole] >= 3;
  const uint8_t values[] = {preseed ? static_cast<uint8_t>(2) : round.putts[hole],
                            preseed ? static_cast<uint8_t>(2) : round.in100[hole],
                            preseed ? static_cast<uint8_t>(round.par[hole] - 2) : round.out100[hole]};
  const char* labels[] = {GolfStrings::PUTTS, GolfStrings::IN100, GolfStrings::OUT100};

  const int region = countersRegionBottom() - HOLE_BOTTOM;
  const int unfocusedHeight = region * COUNTER_UNFOCUSED_WEIGHT / COUNTER_REGION_WEIGHT;
  const int focusedHeight = region - unfocusedHeight * 2;

  // Left limit for the marker run: clear of the widest centred number.
  const int markerLeft = screenWidth / 2 + 60 + MARKER_GUTTER;
  const int markerMaxWidth = screenWidth - MARKER_RIGHT_MARGIN - markerLeft;

  int top = HOLE_BOTTOM;
  for (uint8_t index = 0; index < 3; ++index) {
    const bool focused = focusedField == static_cast<GolfField>(index);
    const int height = focused ? focusedHeight : unfocusedHeight;
    const bool inverse = focused;
    if (inverse) renderer.fillRect(0, top, screenWidth, height, true);
    renderer.drawText(UI_10_FONT_ID, 20, top + 14, labels[index], !inverse, EpdFontFamily::BOLD);
    if (index == 2) {
      char badge[24];
      // Penalty-inclusive so it agrees with the totals strip and the band.
      const uint16_t total = preseed ? static_cast<uint16_t>(values[1] + values[2]) : golfHoleScore(round, hole);
      if (round.par[hole] != 0) {
        char toPar[8];
        formatToPar(static_cast<int16_t>(total) - round.par[hole], toPar, sizeof(toPar));
        snprintf(badge, sizeof(badge), "%s %u %s", GolfStrings::TOTAL, total, toPar);
      } else {
        snprintf(badge, sizeof(badge), "%s %u", GolfStrings::TOTAL, total);
      }
      renderer.drawText(UI_10_FONT_ID, screenWidth - 22 - renderer.getTextWidth(UI_10_FONT_ID, badge), top + 14, badge,
                        !inverse, EpdFontFamily::BOLD);
    } else if (carryNotice != nullptr && index == static_cast<uint8_t>(focusedField)) {
      renderer.drawText(UI_10_FONT_ID, screenWidth - 22 - renderer.getTextWidth(UI_10_FONT_ID, carryNotice), top + 14,
                        carryNotice, !inverse, EpdFontFamily::BOLD);
    }
    const int digitHeight = focused ? 100 : 66;
    golfDrawLargeNumber(renderer, screenWidth / 2, top + (height - digitHeight) / 2 + 12, digitHeight, values[index],
                        !inverse, preseed);

    // Markers for this field, right of the number, in the order they happened.
    char markers[48];
    const int markerWidth = formatHoleMarkers(round, hole, index, markerMaxWidth, markers, sizeof(markers));
    if (markerWidth > 0) {
      renderer.drawText(MARKER_FONT_ID, screenWidth - MARKER_RIGHT_MARGIN - markerWidth, top + height / 2 - 6, markers,
                        !inverse, EpdFontFamily::BOLD);
    }

    renderer.drawLine(0, top + height - 1, screenWidth, top + height - 1, !inverse);
    top += height;
  }
}

void GolfScoringActivity::drawPenaltyBand() const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const uint8_t hole = round.currentHole;
  if (round.penaltyCount[hole] == 0) return;  // absent entirely on a clean hole

  const int screenWidth = renderer.getScreenWidth();
  const int top = COUNTERS_BOTTOM - PENALTY_BAND_HEIGHT;
  renderer.fillRect(0, top, screenWidth, PENALTY_BAND_HEIGHT, true);

  char label[24];
  snprintf(label, sizeof(label), "%s +%u", GolfStrings::PENALTY, golfPenaltyStrokesForHole(round, hole));
  renderer.drawText(UI_12_FONT_ID, 18, top + 12, label, false, EpdFontFamily::BOLD);

  // Whole-hole marker sequence on the right; same "+N" overflow trim.
  const int labelRight = 18 + renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  char sequence[48];
  const int sequenceWidth =
      formatHoleMarkers(round, hole, -1, screenWidth - 18 - labelRight - 12, sequence, sizeof(sequence));
  if (sequenceWidth > 0) {
    renderer.drawText(MARKER_FONT_ID, screenWidth - 18 - sequenceWidth, top + 14, sequence, false, EpdFontFamily::BOLD);
  }
  renderer.drawLine(0, COUNTERS_BOTTOM - 1, screenWidth, COUNTERS_BOTTOM - 1, true);
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
  const int stripBottom = renderer.getScreenHeight() - UITheme::getInstance().getMetrics().buttonHintsHeight;
  for (uint8_t offset = 0; offset < 9 && first + offset < round.holeCount; ++offset) {
    const uint8_t hole = first + offset;
    const int x = offset * cellWidth;
    const bool current = hole == round.currentHole;
    if (current) renderer.fillRect(x, TOTALS_BOTTOM, cellWidth, stripBottom - TOTALS_BOTTOM, true);
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
    const int scoreWidth = renderer.getTextWidth(UI_10_FONT_ID, number, EpdFontFamily::BOLD);
    // Superscript on a hole that had a penalty: "O" if any OB, else "H". Sized
    // to sit inside the column with the score, so the strip never widens.
    const bool superscript = score != 0 && round.penaltyCount[hole] > 0;
    const char* mark = superscript && golfObsForHole(round, hole) > 0 ? "O" : "H";
    const int markWidth = superscript ? renderer.getTextWidth(SMALL_FONT_ID, mark) + 1 : 0;
    const int scoreX = x + (cellWidth - scoreWidth - markWidth) / 2;
    renderer.drawText(UI_10_FONT_ID, scoreX, TOTALS_BOTTOM + 28, number, !current, EpdFontFamily::BOLD);
    if (superscript) {
      renderer.drawText(SMALL_FONT_ID, scoreX + scoreWidth + 1, TOTALS_BOTTOM + 22, mark, !current);
    }
  }
}

void GolfScoringActivity::drawFooter() const {
  const auto labels =
      mappedInput.mapLabels(GolfStrings::F_MENU, powerCyclesField() ? GolfStrings::PENALTY : GolfStrings::F_FIELD,
                            GolfStrings::F_PREV, GolfStrings::F_NEXT);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfScoringActivity::drawPenaltyPicker() const {
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int noticeHeight = pickerHoleFull ? SHEET_NOTICE_H : 0;
  const int sheetHeight =
      SHEET_TITLE_H + noticeHeight + SHEET_OPTION_H * 2 + UITheme::getInstance().getMetrics().buttonHintsHeight;
  const int sheetTop = screenHeight - sheetHeight;

  // Dim the hole still showing above the sheet so it reads as modal.
  renderer.fillRectDither(0, 0, screenWidth, sheetTop, Color::LightGray);

  renderer.fillRect(0, sheetTop, screenWidth, sheetHeight, false);
  renderer.fillRect(0, sheetTop, screenWidth, SHEET_BORDER, true);

  int y = sheetTop + SHEET_BORDER;
  renderer.drawText(UI_10_FONT_ID, SHEET_PAD_X, y + 16, GolfStrings::ADD_PENALTY, true, EpdFontFamily::BOLD);
  y += SHEET_TITLE_H - SHEET_BORDER;
  renderer.drawLine(0, y - 1, screenWidth, y - 1, true);

  if (pickerHoleFull) {
    renderer.fillRect(0, y, screenWidth, SHEET_NOTICE_H, true);
    renderer.drawText(UI_10_FONT_ID, SHEET_PAD_X, y + 9, GolfStrings::HOLE_FULL, false, EpdFontFamily::BOLD);
    y += SHEET_NOTICE_H;
  }

  for (int option = 0; option < 2; ++option) {
    const bool selected = (option == 0) == (pickerKind == GolfPenaltyKind::Hazard);
    const int rowTop = y + option * SHEET_OPTION_H;
    if (selected) renderer.fillRect(0, rowTop, screenWidth, SHEET_OPTION_H, true);
    const bool ink = !selected;
    const char* tag = option == 0 ? GolfStrings::HAZARD_TAG : GolfStrings::OB_TAG;
    const char* name = option == 0 ? GolfStrings::HAZARD : GolfStrings::OB;
    const char* cost = option == 0 ? GolfStrings::HAZARD_COST : GolfStrings::OB_COST;
    const int tagWidth = renderer.getTextWidth(UI_12_FONT_ID, tag, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, (SHEET_TAG_COL_W - tagWidth) / 2, rowTop + 40, tag, ink, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, SHEET_TEXT_X, rowTop + 34, name, ink, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, SHEET_TEXT_X, rowTop + 66, cost, ink);
    renderer.drawLine(0, rowTop + SHEET_OPTION_H - 1, screenWidth, rowTop + SHEET_OPTION_H - 1, true);
  }
  const auto labels = mappedInput.mapLabels(GolfStrings::PICK_BACK, GolfStrings::PICK_CONFIRM, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfScoringActivity::render(RenderLock&&) {
  renderer.clearScreen();
  drawStatusBar();
  drawHoleBand();
  drawCounters();
  drawPenaltyBand();
  drawTotals();
  drawNineStrip();
  drawFooter();
  if (pickerOpen) drawPenaltyPicker();
  ++paintCount;
  if (paintCount % 8 == 0) renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  if (carryNotice != nullptr) {
    carryNotice = nullptr;
    requestUpdate();
  }
}

#endif
