#pragma once

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfHistory.h"

class GolfRoundSummaryActivity final : public Activity, protected UiAppHost {
 public:
  GolfRoundSummaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfHistoryEntry& entry)
      : Activity("GolfRoundSum", renderer, mappedInput), UiAppHost(renderer), entry(entry) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  GolfHistoryEntry entry{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  freeink::ui::TableProps tableProps{};
  char cells[8][2][20]{};

  static void screenTrampoline(UiScreen& screen, void* user);
  void buildScreen(UiScreen& screen);
};
