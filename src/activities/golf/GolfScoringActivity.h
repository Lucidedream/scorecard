#pragma once

#include "GolfReviewFormat.h"
#include "GolfUiLayout.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfConfirm.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRules.h"

class GolfScoringActivity final : public Activity, protected UiAppHost {
 public:
  GolfScoringActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GolfScoring", renderer, mappedInput), UiAppHost(renderer) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  GolfField focusedField = GolfField::Putts;
  const char* carryNotice = nullptr;
  bool saveFailed = false;
  uint8_t paintCount = 0;
  uint32_t lastChangeAt = 0;
  uint32_t lastRepeatAt = 0;
  // Reused by the render task; keeping these in the heap-owned activity avoids
  // a 165-byte automatic status-bar buffer set on every paint.
  mutable char statusTitle[128]{};
  mutable char statusPlayerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  mutable char statusTime[9]{};

  // Penalty picker (CONTRACTS-V2 §12). While pickerOpen the scoring screen
  // still renders in full and the sheet draws over it.
  bool pickerOpen = false;
  GolfPenaltyKind pickerKind = GolfPenaltyKind::Hazard;
  bool pickerHoleFull = false;

  // Seeds the current player's current hole to its par preview if unentered.
  // Mutation paths call this inside their RenderLock before changing the same
  // explicit GolfPlayerScore.
  bool ensureHoleSeeded();

  void mutateCounter(bool increment);
  void removeOrDecrement();
  void handleConfirm();
  void commitAndAdvance();
  void changeTurn(bool forward);
  void resetTurnState();
  void markDirtyForIdle();
  void openCourseMap();
  void openRoundMenu();
  bool flushDirty();
  bool rejectArchivedMutation();

  // True while Power is the field-cycle button; false leaves that role on
  // Confirm and moves the picker to a Confirm long-press (§12.6).
  bool powerCyclesField() const;
  bool confirmFromFrontButton() const;

  void openPenaltyPicker();
  void closePenaltyPicker();
  void handlePickerInput();
  void applyPenaltyPick();

  // Writes the space-separated marker sequence for a hole ("H OB"), in event
  // order, trimming trailing markers to a "+N" tail when the run exceeds
  // maxWidth. fieldFilter < 0 takes every field; otherwise only that GolfField's
  // markers. Returns the pixel width of the text written.
  int formatHoleMarkers(const GolfPlayerScore& score, uint8_t hole, int fieldFilter, int maxWidth, char* out,
                        size_t size) const;

  void drawStatusBar(const golfui::ScoringLayout& layout) const;
  void drawHoleBand(const golfui::ScoringLayout& layout) const;
  void drawCounters(const golfui::ScoringLayout& layout) const;
  void drawPenaltyBand(const golfui::ScoringLayout& layout) const;
  void drawTotals(const golfui::ScoringLayout& layout) const;
  void drawNineStrip(const golfui::ScoringLayout& layout) const;
  void drawFooter() const;
  void drawPenaltyPicker(freeink::ui::Rect safe) const;
};
