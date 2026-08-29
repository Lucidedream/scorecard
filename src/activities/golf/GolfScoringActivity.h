#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfConfirm.h"
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

  void mutateCounter(bool increment);
  void handleConfirm();
  void commitAndAdvance();
  void changeHole(int delta);
  void openRoundMenu();
  bool flushDirty();
  void loadCourseDisplayData();

  void drawStatusBar() const;
  void drawHoleBand() const;
  void drawCounters() const;
  void drawTotals() const;
  void drawNineStrip() const;
  void drawFooter() const;
};
