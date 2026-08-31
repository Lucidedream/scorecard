#pragma once

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
  uint8_t si[GolfRound::MAX_HOLES]{};
  bool hasSi = false;
  const char* carryNotice = nullptr;
  bool saveFailed = false;
  uint8_t paintCount = 0;
  uint32_t lastCounterChangeAt = 0;
  uint32_t lastRepeatAt = 0;

  // Penalty picker (CONTRACTS-V2 §12). While pickerOpen the scoring screen
  // still renders in full and the sheet draws over it.
  bool pickerOpen = false;
  GolfPenaltyKind pickerKind = GolfPenaltyKind::Hazard;
  bool pickerHoleFull = false;

  // Seeds the current hole to its par preview if it is still unlogged
  // (CONTRACTS-V2 §13.1). Every path that mutates a hole — counter change,
  // penalty append, penalty removal, commit-and-advance — calls this first,
  // inside its RenderLock, so the displayed preview is persisted before a
  // mutation flips the hole to "entered". Returns true if it seeded; seeding
  // alone dirties the round, so callers fold this into their "changed" result.
  bool ensureHoleSeeded();

  void mutateCounter(bool increment);
  void removeOrDecrement();
  void handleConfirm();
  void commitAndAdvance();
  void changeHole(int delta);
  void openRoundMenu();
  bool flushDirty();
  void loadCourseDisplayData();

  // True while Power is the field-cycle button; false leaves that role on
  // Confirm and moves the picker to a Confirm long-press (§12.6).
  bool powerCyclesField() const;

  void openPenaltyPicker();
  void closePenaltyPicker();
  void handlePickerInput();
  void applyPenaltyPick();

  // Bottom of the counter region: shrinks by the penalty band's height on a
  // hole that has penalties, so everything below the band keeps a fixed layout.
  int countersRegionBottom() const;
  // Writes the space-separated marker sequence for a hole ("H OB"), in event
  // order, trimming trailing markers to a "+N" tail when the run exceeds
  // maxWidth. fieldFilter < 0 takes every field; otherwise only that GolfField's
  // markers. Returns the pixel width of the text written.
  int formatHoleMarkers(const GolfRound& round, uint8_t hole, int fieldFilter, int maxWidth, char* out,
                        size_t size) const;

  void drawStatusBar() const;
  void drawHoleBand() const;
  void drawCounters() const;
  void drawPenaltyBand() const;
  void drawTotals() const;
  void drawNineStrip() const;
  void drawFooter() const;
  void drawPenaltyPicker() const;
};
