#pragma once

#include <cstdio>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfHistory.h"
#include "golf/GolfPaths.h"

class GolfRoundSummaryActivity final : public Activity, protected UiAppHost {
 public:
  GolfRoundSummaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfHistoryEntry& entry,
                           bool returnToGolfHome = false, const char* filename = nullptr)
      : Activity("GolfRoundSum", renderer, mappedInput),
        UiAppHost(renderer),
        entry(entry),
        returnToGolfHome(returnToGolfHome) {
    if (filename) snprintf(archiveFilename, sizeof(archiveFilename), "%s", filename);
  }

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  GolfHistoryEntry entry{};
  bool returnToGolfHome = false;
  char archiveFilename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  bool exportFailed = false;
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  freeink::ui::TableProps tableProps{};
  char cells[8][2][20]{};

  static void screenTrampoline(UiScreen& screen, void* user);
  void buildScreen(UiScreen& screen);
};
