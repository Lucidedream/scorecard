#pragma once

#include <cstdint>

#include "GolfReviewFormat.h"
#include "activities/UiListActivity.h"
#include "golf/GolfHistory.h"

// Player-scoped chooser between the two History destinations (CONTRACTS-V2
// §29): "Trends" and "Rounds" open the unchanged GolfTrendsActivity /
// GolfHistoryActivity for the same (slot, playerName) the player picker
// already resolved.
class GolfHistoryChoiceActivity final : public UiListActivity {
 public:
  GolfHistoryChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t playerSlot,
                            const char* playerName);

  void onEnter() override;

 private:
  static constexpr uint8_t ROW_COUNT = 2;

  const uint8_t playerSlot;
  char playerName[GolfPlayer::NAME_CAPACITY]{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  freeink::ui::ListProps listProps{};

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void drawFooter() override;
};
