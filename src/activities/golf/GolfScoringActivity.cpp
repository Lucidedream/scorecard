#include "GolfScoringActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <BoardConfig.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "GolfCourseMapActivity.h"
#include "GolfLargeNumber.h"
#include "GolfNavigation.h"
#include "GolfReviewFormat.h"
#include "GolfRoundMenuActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundStore.h"
#include "golf/GolfScoringDisplay.h"
#include "golf/GolfStats.h"

namespace {

constexpr uint32_t IDLE_SAVE_MS = 5000;
constexpr uint32_t REPEAT_START_MS = 500;
constexpr uint32_t REPEAT_INTERVAL_MS = 250;

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
constexpr unsigned long MAP_LONGPRESS_MS = 500;

void formatToPar(const int16_t value, char* output, const size_t size) {
  golfFormatReviewToPar(value, tr(STR_GOLF_EVEN), tr(STR_GOLF_TO_PAR_POSITIVE_FORMAT),
                        tr(STR_GOLF_TO_PAR_NEGATIVE_FORMAT), output, size);
}

void ellipsize(const GfxRenderer& renderer, char* text, const size_t size, const int maxWidth) {
  if (size == 0) return;
  text[size - 1] = '\0';

  const size_t inputLength = strlen(text);
  size_t offset = 0;
  while (offset < inputLength) {
    const uint8_t lead = static_cast<uint8_t>(text[offset]);
    size_t codepointBytes = 0;
    if (lead < 0x80) {
      codepointBytes = 1;
    } else if ((lead & 0xe0) == 0xc0) {
      codepointBytes = 2;
    } else if ((lead & 0xf0) == 0xe0) {
      codepointBytes = 3;
    } else if ((lead & 0xf8) == 0xf0) {
      codepointBytes = 4;
    }
    if (codepointBytes == 0 || offset + codepointBytes > inputLength) {
      text[offset] = '\0';
      break;
    }
    bool complete = true;
    for (size_t byte = 1; byte < codepointBytes; ++byte) {
      if ((static_cast<uint8_t>(text[offset + byte]) & 0xc0) != 0x80) complete = false;
    }
    if (!complete) {
      text[offset] = '\0';
      break;
    }
    offset += codepointBytes;
  }

  if (renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD) <= maxWidth) return;

  constexpr char ELLIPSIS[] = "...";
  size_t length = strlen(text);
  while (length > 0) {
    do {
      --length;
    } while (length > 0 && (static_cast<uint8_t>(text[length]) & 0xc0) == 0x80);
    text[length] = '\0';
    if (length + sizeof(ELLIPSIS) <= size) {
      memcpy(text + length, ELLIPSIS, sizeof(ELLIPSIS));
      if (renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD) <= maxWidth) return;
      text[length] = '\0';
    }
  }
  snprintf(text, size, "%s", ELLIPSIS);
}

}  // namespace

void GolfScoringActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  if (GOLF_ROUND_STORE.isArchived()) {
    LOG_ERR("GOLF", "Committed archive cannot re-enter scoring");
    openGolfHome(activityManager, renderer, mappedInput);
    return;
  }

  bool currentPlayerChanged = false;
  bool hasEnabledPlayer = true;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    const uint8_t firstPlayer = golfFirstEnabledPlayer(round);
    if (firstPlayer == GolfRound::NO_PLAYER) {
      hasEnabledPlayer = false;
      if (round.currentPlayer >= GolfRound::MAX_PLAYERS) round.currentPlayer = 0;
    } else if (round.currentPlayer >= GolfRound::MAX_PLAYERS ||
               !golfPlayerIsEnabled(round.players[round.currentPlayer])) {
      round.currentPlayer = firstPlayer;
      currentPlayerChanged = true;
    }
    resetTurnState();
  }
  if (!hasEnabledPlayer) LOG_ERR("GOLF", "Scoring round has no enabled player");
  if (currentPlayerChanged) markDirtyForIdle();
  requestUpdate();
}

void GolfScoringActivity::onExit() { Activity::onExit(); }

bool GolfScoringActivity::rejectArchivedMutation() {
  if (!GOLF_ROUND_STORE.isArchived()) return false;
  LOG_ERR("GOLF", "Rejected mutation of committed archive");
  {
    RenderLock lock(*this);
    saveFailed = true;
  }
  requestUpdate();
  return true;
}

bool GolfScoringActivity::flushDirty() {
  const bool success = flushGolfRoundIfDirty();
  const bool failed = !success;
  bool failureStateChanged = false;
  {
    RenderLock lock(*this);
    failureStateChanged = saveFailed != failed;
    saveFailed = failed;
  }
  if (failureStateChanged) requestUpdate();
  return success;
}

void GolfScoringActivity::resetTurnState() {
  focusedField = GolfField::Putts;
  carryNotice = nullptr;
  pickerOpen = false;
  pickerKind = GolfPenaltyKind::Hazard;
  pickerHoleFull = false;
  lastRepeatAt = 0;
}

void GolfScoringActivity::markDirtyForIdle() {
  if (rejectArchivedMutation()) return;
  markGolfRoundDirty();
  lastChangeAt = millis();
  RenderLock lock(*this);
  saveFailed = false;
}

bool GolfScoringActivity::ensureHoleSeeded() {
  GolfRound& round = GOLF_ROUND_STORE.getRound();
  GolfPlayerScore& score = round.players[round.currentPlayer].score;
  return seedGolfHoleAtPar(score, round.currentHole, round.par[round.currentHole]);
}

void GolfScoringActivity::mutateCounter(const bool increment) {
  if (rejectArchivedMutation()) return;
  GolfMutationResult result{};
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    GolfPlayerScore& score = round.players[round.currentPlayer].score;
    const bool seeded = ensureHoleSeeded();
    result = increment ? incrementGolfCounter(score, round.currentHole, focusedField)
                       : decrementGolfCounter(score, round.currentHole, focusedField);
    if (seeded && !result.changed) result.changed = true;
    if (result.carriedIn100) carryNotice = tr(STR_GOLF_INSIDE_100_CARRY);
    if (result.loweredPutts) carryNotice = tr(STR_GOLF_PUTTS_CARRY);
  }
  if (result.changed) markDirtyForIdle();
  if (result.changed) requestUpdate();
}

void GolfScoringActivity::removeOrDecrement() {
  if (rejectArchivedMutation()) return;
  // Down on a field that has markers removes that field's most recent marker,
  // its shot, and its penalty strokes. Otherwise it is a plain decrement,
  // exactly as before. CONTRACTS-V2 §12.4.
  GolfPenaltyMutationStatus status;
  bool seeded = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    GolfPlayerScore& score = round.players[round.currentPlayer].score;
    seeded = ensureHoleSeeded();
    status = golfRemoveLatestPenalty(score, round.currentHole, focusedField);
  }
  if (status == GolfPenaltyMutationStatus::NoMarker) {
    // No marker on this field: fall through to a plain decrement, which seeds
    // and logs on its own path.
    mutateCounter(false);
    return;
  }
  if (seeded || status == GolfPenaltyMutationStatus::Changed) {
    markDirtyForIdle();
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

bool GolfScoringActivity::confirmFromFrontButton() const {
  // Power is deliberately overloaded as the field-cycle button on this
  // screen (§12.6); exclude its combined Confirm event so one click cannot
  // also activate the penalty picker.
  return golfConfirmFromFrontButton(mappedInput.wasReleased(MappedInputManager::Button::Confirm),
                                    mappedInput.wasReleased(MappedInputManager::Button::Power));
}

void GolfScoringActivity::openPenaltyPicker() {
  {
    RenderLock lock(*this);
    pickerKind = GolfPenaltyKind::Hazard;
    pickerHoleFull = false;
    pickerOpen = true;
  }
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  requestUpdate();
}

void GolfScoringActivity::closePenaltyPicker() {
  // Cancel path: touches no round state at all.
  {
    RenderLock lock(*this);
    pickerOpen = false;
    pickerHoleFull = false;
  }
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  requestUpdate();
}

void GolfScoringActivity::handlePickerInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closePenaltyPicker();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) && pickerKind != GolfPenaltyKind::Hazard) {
    {
      RenderLock lock(*this);
      pickerKind = GolfPenaltyKind::Hazard;
      pickerHoleFull = false;
    }
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && pickerKind != GolfPenaltyKind::Ob) {
    {
      RenderLock lock(*this);
      pickerKind = GolfPenaltyKind::Ob;
      pickerHoleFull = false;
    }
    requestUpdate();
    return;
  }
  if (confirmFromFrontButton()) {
    applyPenaltyPick();
    return;
  }
}

void GolfScoringActivity::applyPenaltyPick() {
  if (rejectArchivedMutation()) return;
  GolfPenaltyMutationStatus status;
  bool seeded = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    GolfPlayerScore& score = round.players[round.currentPlayer].score;
    seeded = ensureHoleSeeded();
    status = golfAppendPenalty(score, round.currentHole, focusedField, pickerKind);
  }
  if (seeded || status == GolfPenaltyMutationStatus::Changed) markDirtyForIdle();
  switch (status) {
    case GolfPenaltyMutationStatus::Changed:
      closePenaltyPicker();
      break;
    case GolfPenaltyMutationStatus::HoleFull: {
      // Never silently drop the event: keep the sheet open and say so.
      {
        RenderLock lock(*this);
        pickerHoleFull = true;
      }
      requestUpdate();
      break;
    }
    default:
      LOG_ERR("GOLF", "penalty add rejected: %d", static_cast<int>(status));
      closePenaltyPicker();
      break;
  }
}

void GolfScoringActivity::handleConfirm() {
  if (rejectArchivedMutation()) return;
  GolfConfirmAction action = GolfConfirmAction::CycleFocus;
  {
    RenderLock lock(*this);
    const GolfRound& round = GOLF_ROUND_STORE.getRound();
    const GolfPlayerScore& score = round.players[round.currentPlayer].score;
    const uint8_t hole = round.currentHole;
    const bool logged = static_cast<uint16_t>(score.in100[hole]) + score.out100[hole] != 0;
    const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score, hole);
    action = golfConfirmPress(focusedField, logged, display.seeded);
    if (action == GolfConfirmAction::CycleFocus) focusedField = nextGolfField(focusedField);
  }
  if (action == GolfConfirmAction::CommitAndAdvance) {
    commitAndAdvance();
    return;
  }
  if (action == GolfConfirmAction::AdvanceWithoutCommit) {
    changeTurn(true);
    return;
  }
  requestUpdate();
}

void GolfScoringActivity::commitAndAdvance() {
  if (rejectArchivedMutation()) return;
  bool committed = false;
  {
    RenderLock lock(*this);
    committed = ensureHoleSeeded();
  }
  if (committed) markDirtyForIdle();
  changeTurn(true);
}

void GolfScoringActivity::changeTurn(const bool forward) {
  if (rejectArchivedMutation()) return;
  bool changed = false;
  bool holeChanged = false;
  {
    RenderLock lock(*this);
    GolfRound& round = GOLF_ROUND_STORE.getRound();
    const uint8_t previousHole = round.currentHole;
    const uint8_t previousPlayer = round.currentPlayer;
    const bool traversed = forward ? advanceGolfTurn(round) : retreatGolfTurn(round);
    changed = traversed && (round.currentHole != previousHole || round.currentPlayer != previousPlayer);
    holeChanged = changed && round.currentHole != previousHole;
    if (changed) resetTurnState();
  }
  if (!changed) {
    LOG_ERR("GOLF", "Turn change rejected");
    return;
  }

  markDirtyForIdle();
  if (holeChanged) flushDirty();
  requestUpdate();
}

void GolfScoringActivity::openRoundMenu() {
  if (rejectArchivedMutation()) {
    openGolfHome(activityManager, renderer, mappedInput);
    return;
  }
  if (!flushDirty()) return;
  auto menu = makeUniqueNoThrow<GolfRoundMenuActivity>(renderer, mappedInput);
  if (!menu) {
    LOG_ERR("GOLF", "OOM: round menu");
    return;
  }
  startActivityForResult(std::move(menu), nullptr);
}

void GolfScoringActivity::openCourseMap() {
  if (!flushDirty()) return;
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  auto map = makeUniqueNoThrow<GolfCourseMapActivity>(renderer, mappedInput, round.currentHole, round.holeCount,
                                                       round.courseName);
  if (!map) {
    LOG_ERR("GOLF", "OOM: course map");
    return;
  }
  startActivityForResult(std::move(map), nullptr);
}

bool GolfScoringActivity::handleHomeGesture() { return !flushDirty(); }

void GolfScoringActivity::loop() {
  if (GOLF_ROUND_STORE.isArchived()) {
    LOG_ERR("GOLF", "Leaving committed archive scoring surface");
    openGolfHome(activityManager, renderer, mappedInput);
    return;
  }
  if (pickerOpen) {
    handlePickerInput();
    return;
  }

  if (mappedInput.wasLongPressed(MappedInputManager::Button::Back, MAP_LONGPRESS_MS)) {
    openCourseMap();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    openRoundMenu();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeTurn(golfFrontNavDelta(swapped, true) > 0);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    changeTurn(golfFrontNavDelta(swapped, false) > 0);
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
    if (confirmFromFrontButton()) {
      openPenaltyPicker();
      return;
    }
  } else {
    if (mappedInput.wasLongPressed(MappedInputManager::Button::Confirm, PICKER_LONGPRESS_MS)) {
      openPenaltyPicker();
      return;
    }
    if (confirmFromFrontButton()) {
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
  if (isGolfRoundDirty() && millis() - lastChangeAt >= IDLE_SAVE_MS) flushDirty();
}

void GolfScoringActivity::drawStatusBar(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayer& player = round.players[round.currentPlayer];
  golfFormatPlayerLabel(round.currentPlayer, player.name, tr(STR_GOLF_PLAYER_LABEL_FORMAT), statusPlayerLabel,
                        sizeof(statusPlayerLabel));
  snprintf(statusTitle, sizeof(statusTitle), tr(STR_GOLF_PLAYER_CONTEXT_FORMAT), statusPlayerLabel,
           (saveFailed || hasGolfRoundSaveFailed()) ? tr(STR_GOLF_SAVE_ERROR) : round.courseName);
  ellipsize(renderer, statusTitle, sizeof(statusTitle),
            layout.header.width > 135 ? layout.header.width - 135 : layout.header.width);
  const char* right = nullptr;
  if (halClock.isAvailable() && halClock.formatTime(statusTime, sizeof(statusTime))) right = statusTime;
  golfui::drawHeader(renderer, layout.header, statusTitle, right);
}

void GolfScoringActivity::drawHoleBand(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayer& player = round.players[round.currentPlayer];
  const uint8_t hole = round.currentHole;
  const freeink::ui::Rect rect = layout.hole;
  const int padding = golfui::minValue(18, static_cast<int16_t>(rect.width / 8));
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + 4, tr(STR_GOLF_HOLE), true, EpdFontFamily::BOLD);
  const int digitHeight = golfui::scoringHoleDigitHeight(rect.height, lineHeight);
  golfDrawLargeNumber(renderer, rect.x + rect.width / 4, golfui::scoringHoleDigitTop(rect, digitHeight), digitHeight,
                      hole + 1);
  char text[32];
  const int right = rect.x + rect.width - padding;
  if (rect.height < lineHeight * 3 + 4) {
    if (round.par[hole] != 0) {
      snprintf(text, sizeof(text), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_PAR),
               static_cast<unsigned>(round.par[hole]));
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, text), rect.y + 3, text, true,
                        EpdFontFamily::BOLD);
    }
    if (player.yards[hole] != 0 || round.hasSi) {
      if (player.yards[hole] != 0 && round.hasSi) {
        snprintf(text, sizeof(text), tr(STR_GOLF_DISTANCE_STROKE_INDEX_FORMAT),
                 static_cast<unsigned>(player.yards[hole]), tr(STR_GOLF_YARDS_UNIT), tr(STR_GOLF_STROKE_INDEX),
                 static_cast<unsigned>(round.si[hole]));
      } else if (player.yards[hole] != 0) {
        snprintf(text, sizeof(text), tr(STR_GOLF_DISTANCE_FORMAT), static_cast<unsigned>(player.yards[hole]),
                 tr(STR_GOLF_YARDS_UNIT));
      } else {
        snprintf(text, sizeof(text), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_STROKE_INDEX),
                 static_cast<unsigned>(round.si[hole]));
      }
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, text),
                        rect.y + rect.height - lineHeight - 3, text);
    }
  } else {
    int y = rect.y + 4;
    const int lineStep = golfui::clampValue((rect.height - 4) / 3, lineHeight, lineHeight + 8);
    if (round.par[hole] != 0) {
      snprintf(text, sizeof(text), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_PAR),
               static_cast<unsigned>(round.par[hole]));
      renderer.drawText(UI_12_FONT_ID, right - renderer.getTextWidth(UI_12_FONT_ID, text), y, text, true,
                        EpdFontFamily::BOLD);
      y += lineStep;
    }
    if (player.yards[hole] != 0) {
      snprintf(text, sizeof(text), tr(STR_GOLF_DISTANCE_FORMAT), static_cast<unsigned>(player.yards[hole]),
               tr(STR_GOLF_YARDS_UNIT));
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, text), y, text);
      y += lineStep;
    }
    if (round.hasSi && y + lineHeight <= rect.y + rect.height) {
      snprintf(text, sizeof(text), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_STROKE_INDEX),
               static_cast<unsigned>(round.si[hole]));
      renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, text), y, text);
    }
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

int GolfScoringActivity::formatHoleMarkers(const GolfPlayerScore& score, const uint8_t hole, const int fieldFilter,
                                           const int maxWidth, char* out, const size_t size) const {
  out[0] = '\0';
  const uint8_t count = score.penaltyCount[hole] < GolfRound::MAX_PENALTIES_PER_HOLE
                            ? score.penaltyCount[hole]
                            : GolfRound::MAX_PENALTIES_PER_HOLE;
  const char* tokens[GolfRound::MAX_PENALTIES_PER_HOLE];
  uint8_t n = 0;
  for (uint8_t i = 0; i < count; ++i) {
    GolfPenaltyEvent event{};
    if (!golfPenaltyEventAt(score, hole, i, event)) continue;
    if (fieldFilter >= 0 && static_cast<int>(event.field) != fieldFilter) continue;
    tokens[n++] = event.kind == GolfPenaltyKind::Ob ? tr(STR_GOLF_OUT_OF_BOUNDS_TAG) : tr(STR_GOLF_HAZARD_TAG);
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
      len += snprintf(buf + len, sizeof(buf) - len, "%s+%u", shown == 0 ? "" : " ", static_cast<unsigned>(n - shown));
    }
    const int width = renderer.getTextWidth(MARKER_FONT_ID, buf, EpdFontFamily::BOLD);
    if (width <= maxWidth || shown == 0) {
      snprintf(out, size, "%s", buf);
      return width;
    }
  }
}

void GolfScoringActivity::drawCounters(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayerScore& score = round.players[round.currentPlayer].score;
  const uint8_t hole = round.currentHole;
  const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score, hole);
  const char* labels[] = {tr(STR_GOLF_PUTTS), tr(STR_GOLF_INSIDE_100), tr(STR_GOLF_SCORING_ZONE)};

  for (uint8_t index = 0; index < 3; ++index) {
    const freeink::ui::Rect rect = layout.counters[index];
    const bool focused = focusedField == static_cast<GolfField>(index);
    const bool inverse = focused;
    const int padding = golfui::minValue(16, static_cast<int16_t>(rect.width / 8));
    if (inverse) renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
    renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + 4, labels[index], !inverse, EpdFontFamily::BOLD);
    if (index != 2 && carryNotice != nullptr && index == static_cast<uint8_t>(focusedField)) {
      renderer.drawText(UI_10_FONT_ID,
                        rect.x + rect.width - padding - renderer.getTextWidth(UI_10_FONT_ID, carryNotice), rect.y + 4,
                        carryNotice, !inverse, EpdFontFamily::BOLD);
    }
    const int digitHeight = golfui::scoringDigitHeight(rect.height, focused);
    golfDrawLargeNumber(renderer, rect.x + rect.width / 2, rect.y + (rect.height - digitHeight) / 2 + 12, digitHeight,
                        display.counters[index], !inverse, display.seeded);

    const int markerLeft = rect.x + rect.width / 2 + digitHeight + MARKER_GUTTER;
    const int markerMaxWidth = rect.x + rect.width - MARKER_RIGHT_MARGIN - markerLeft;
    if (markerMaxWidth > 0) {
      char markers[48];
      const int markerWidth = formatHoleMarkers(score, hole, index, markerMaxWidth, markers, sizeof(markers));
      if (markerWidth > 0) {
        renderer.drawText(MARKER_FONT_ID, rect.x + rect.width - MARKER_RIGHT_MARGIN - markerWidth,
                          rect.y + (rect.height - renderer.getLineHeight(MARKER_FONT_ID)) / 2, markers, !inverse,
                          EpdFontFamily::BOLD);
      }
    }

    renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, !inverse);
  }
}

void GolfScoringActivity::drawPenaltyBand(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayerScore& score = round.players[round.currentPlayer].score;
  const uint8_t hole = round.currentHole;
  const freeink::ui::Rect rect = layout.penalty;
  if (score.penaltyCount[hole] == 0 || rect.height <= 0) return;

  const int padding = golfui::minValue(18, static_cast<int16_t>(rect.width / 8));
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, true);
  char label[24];
  snprintf(label, sizeof(label), tr(STR_GOLF_PENALTY_STROKES_FORMAT), tr(STR_GOLF_PENALTY),
           static_cast<unsigned>(golfPenaltyStrokesForHole(score, hole)));
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, rect.x + padding, textY, label, false, EpdFontFamily::BOLD);

  const int labelRight = rect.x + padding + renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  char sequence[48];
  const int sequenceWidth =
      formatHoleMarkers(score, hole, -1, rect.x + rect.width - padding - labelRight - 12, sequence, sizeof(sequence));
  if (sequenceWidth > 0) {
    renderer.drawText(MARKER_FONT_ID, rect.x + rect.width - padding - sequenceWidth,
                      rect.y + (rect.height - renderer.getLineHeight(MARKER_FONT_ID)) / 2, sequence, false,
                      EpdFontFamily::BOLD);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, true);
}

void GolfScoringActivity::drawTotals(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayerScore& score = round.players[round.currentPlayer].score;
  const freeink::ui::Rect rect = layout.totals;
  const uint8_t hole = round.currentHole;
  const GolfScoringHoleDisplay display = golfScoringHoleDisplay(round, score, hole);
  const char* labels[] = {tr(STR_GOLF_THRU), tr(STR_GOLF_HOLE), tr(STR_GOLF_TOTAL)};
  char values[3][8];
  snprintf(values[0], sizeof(values[0]), "%u", static_cast<unsigned>(golfThru(round, score)));
  if (round.par[hole] != 0) {
    formatToPar(static_cast<int16_t>(display.score) - round.par[hole], values[1], sizeof(values[1]));
  } else {
    snprintf(values[1], sizeof(values[1]), "%u", static_cast<unsigned>(display.score));
  }
  snprintf(values[2], sizeof(values[2]), "%u", static_cast<unsigned>(golfScore(round, score)));
  const int labelHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int valueHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int textTop = rect.y + (rect.height - labelHeight - valueHeight) / 2;
  for (uint8_t index = 0; index < 3; ++index) {
    const freeink::ui::Rect cell = golfui::totalsCell(rect, index);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, labels[index], EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, cell.x + (cell.width - labelWidth) / 2, textTop, labels[index], true,
                      EpdFontFamily::BOLD);
    const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, values[index], EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, cell.x + (cell.width - textWidth) / 2, textTop + labelHeight, values[index], true,
                      EpdFontFamily::BOLD);
    if (index != 0) renderer.drawLine(cell.x, rect.y + 4, cell.x, rect.y + rect.height - 5);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfScoringActivity::drawNineStrip(const golfui::ScoringLayout& layout) const {
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayerScore& playerScore = round.players[round.currentPlayer].score;
  const uint8_t first = static_cast<uint8_t>((round.currentHole / 9) * 9);
  const freeink::ui::Rect rect = layout.nineStrip;
  const int numberHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int scoreHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int textTop = rect.y + (rect.height - numberHeight - scoreHeight) / 2;
  for (uint8_t offset = 0; offset < 9 && first + offset < round.holeCount; ++offset) {
    const uint8_t hole = first + offset;
    const int left = rect.x + static_cast<int32_t>(rect.width) * offset / 9;
    const int right = rect.x + static_cast<int32_t>(rect.width) * (offset + 1) / 9;
    const int cellWidth = right - left;
    const bool current = hole == round.currentHole;
    if (current) renderer.fillRect(left, rect.y, cellWidth, rect.height, true);
    char number[4];
    snprintf(number, sizeof(number), "%u", static_cast<unsigned>(hole + 1));
    renderer.drawText(SMALL_FONT_ID, left + (cellWidth - renderer.getTextWidth(SMALL_FONT_ID, number)) / 2, textTop,
                      number, !current, EpdFontFamily::BOLD);
    const uint16_t holeScore = golfHoleScore(round, playerScore, hole);
    if (holeScore == 0) {
      snprintf(number, sizeof(number), ".");
    } else {
      snprintf(number, sizeof(number), "%u", static_cast<unsigned>(holeScore));
    }
    const int scoreWidth = renderer.getTextWidth(UI_10_FONT_ID, number, EpdFontFamily::BOLD);
    const bool superscript = holeScore != 0 && playerScore.penaltyCount[hole] > 0;
    const char* mark = superscript && golfObsForHole(playerScore, hole) > 0 ? tr(STR_GOLF_OUT_OF_BOUNDS_SHORT_TAG)
                                                                            : tr(STR_GOLF_HAZARD_TAG);
    const int markWidth = superscript ? renderer.getTextWidth(SMALL_FONT_ID, mark) + 1 : 0;
    const int scoreX = left + (cellWidth - scoreWidth - markWidth) / 2;
    renderer.drawText(UI_10_FONT_ID, scoreX, textTop + numberHeight, number, !current, EpdFontFamily::BOLD);
    if (superscript) {
      renderer.drawText(SMALL_FONT_ID, scoreX + scoreWidth + 1, textTop + numberHeight - 5, mark, !current);
    }
  }
}

void GolfScoringActivity::drawFooter() const {
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_FOOTER_MENU),
                                            powerCyclesField() ? tr(STR_GOLF_PENALTY) : tr(STR_GOLF_FOOTER_FIELD),
                                            tr(STR_GOLF_FOOTER_PREVIOUS), tr(STR_GOLF_FOOTER_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfScoringActivity::drawPenaltyPicker(const freeink::ui::Rect safe) const {
  const int noticeHeight = pickerHoleFull ? SHEET_NOTICE_H : 0;
  const int desiredHeight = SHEET_TITLE_H + noticeHeight + SHEET_OPTION_H * 2;
  const int sheetHeight = golfui::clampValue(desiredHeight, 0, safe.height);
  const int sheetTop = safe.y + safe.height - sheetHeight;
  const int titleHeight = golfui::clampValue(SHEET_TITLE_H, 0, sheetHeight);
  const int appliedNotice = golfui::clampValue(noticeHeight, 0, sheetHeight - titleHeight);
  const int optionAreaHeight = sheetHeight - titleHeight - appliedNotice;

  renderer.fillRectDither(safe.x, safe.y, safe.width, sheetTop - safe.y, Color::LightGray);
  renderer.fillRect(safe.x, sheetTop, safe.width, sheetHeight, false);
  renderer.fillRect(safe.x, sheetTop, safe.width, golfui::minValue(SHEET_BORDER, titleHeight), true);

  int y = sheetTop + golfui::minValue(SHEET_BORDER, titleHeight);
  renderer.drawText(UI_10_FONT_ID, safe.x + SHEET_PAD_X, y + (titleHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                    tr(STR_GOLF_ADD_PENALTY), true, EpdFontFamily::BOLD);
  y = sheetTop + titleHeight;
  renderer.drawLine(safe.x, y - 1, safe.x + safe.width - 1, y - 1, true);

  if (appliedNotice > 0) {
    renderer.fillRect(safe.x, y, safe.width, appliedNotice, true);
    renderer.drawText(UI_10_FONT_ID, safe.x + SHEET_PAD_X,
                      y + (appliedNotice - renderer.getLineHeight(UI_10_FONT_ID)) / 2, tr(STR_GOLF_HOLE_FULL), false,
                      EpdFontFamily::BOLD);
    y += appliedNotice;
  }

  for (int option = 0; option < 2; ++option) {
    const int rowTop = y + static_cast<int32_t>(optionAreaHeight) * option / 2;
    const int rowBottom = y + static_cast<int32_t>(optionAreaHeight) * (option + 1) / 2;
    const int rowHeight = rowBottom - rowTop;
    const bool selected = (option == 0) == (pickerKind == GolfPenaltyKind::Hazard);
    if (selected) renderer.fillRect(safe.x, rowTop, safe.width, rowHeight, true);
    const bool ink = !selected;
    const char* tag = option == 0 ? tr(STR_GOLF_HAZARD_TAG) : tr(STR_GOLF_OUT_OF_BOUNDS_TAG);
    const char* name = option == 0 ? tr(STR_GOLF_HAZARD) : tr(STR_GOLF_OUT_OF_BOUNDS);
    const char* cost = option == 0 ? tr(STR_GOLF_HAZARD_COST) : tr(STR_GOLF_OUT_OF_BOUNDS_COST);
    const int tagColumn = golfui::minValue(SHEET_TAG_COL_W, static_cast<int16_t>(safe.width / 4));
    const int textX = safe.x + golfui::minValue(SHEET_TEXT_X, static_cast<int16_t>(safe.width / 3));
    const int tagWidth = renderer.getTextWidth(UI_12_FONT_ID, tag, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, safe.x + (tagColumn - tagWidth) / 2,
                      rowTop + (rowHeight - renderer.getLineHeight(UI_12_FONT_ID)) / 2, tag, ink, EpdFontFamily::BOLD);
    const int nameHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const int costHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int textTop = rowTop + (rowHeight - nameHeight - costHeight) / 2;
    renderer.drawText(UI_12_FONT_ID, textX, textTop, name, ink, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, textX, textTop + nameHeight, cost, ink);
    renderer.drawLine(safe.x, rowBottom - 1, safe.x + safe.width - 1, rowBottom - 1, true);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_FOOTER_BACK), tr(STR_GOLF_FOOTER_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfScoringActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, 0);
  const GolfRound& round = GOLF_ROUND_STORE.getRound();
  const GolfPlayerScore& score = round.players[round.currentPlayer].score;
  const bool hasPenalty = score.penaltyCount[round.currentHole] > 0;
  const auto layout = golfui::makeScoringLayout(chrome.safe, metrics.topPadding, static_cast<uint8_t>(focusedField),
                                                hasPenalty, renderer.getLineHeight(UI_10_FONT_ID));
  drawStatusBar(layout);
  drawHoleBand(layout);
  drawCounters(layout);
  drawPenaltyBand(layout);
  drawTotals(layout);
  drawNineStrip(layout);
  drawFooter();
  if (pickerOpen) drawPenaltyPicker(layout.safe);
  ++paintCount;
  if (paintCount % 8 == 0) renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  if (carryNotice != nullptr) {
    carryNotice = nullptr;
    requestUpdate();
  }
}

#endif
