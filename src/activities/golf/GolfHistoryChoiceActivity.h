#pragma once

#include <cstdint>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfRound.h"

// Player-scoped chooser between the three History destinations (CONTRACTS-V2
// §29, §33.4, §34.3): "Trends" / "Stats" / "Rounds" open GolfTrendsActivity,
// GolfCareerStatsActivity and GolfHistoryActivity respectively for the same
// (slot, playerName) the player picker already resolved. Re-shaped to match the
// main-menu tile layout (GolfHomeActivity, §25.3 / §28): a three-tile strip over
// a bordered detail box, no quote band. The side rocker and front Left/Right
// move the focus (wrapping); Confirm opens; Back returns to the player picker.
class GolfHistoryChoiceActivity final : public Activity, protected UiAppHost {
 public:
  GolfHistoryChoiceActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t playerSlot,
                            const char* playerName);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t TILE_COUNT = 3;
  static constexpr freeink::ui::ActionId ACTION_TILE = 1;

  const uint8_t playerSlot;
  char playerName[GolfPlayer::NAME_CAPACITY]{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  char roundsDetail[40]{};
  uint8_t selected = 0;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void actionTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void moveSelection(int delta);
  void activateIndex(int index);
  const char* tileLabel(uint8_t index) const;
  const char* tileDetail(uint8_t index) const;
  freeink::ui::BitmapRef tileIcon(uint8_t index) const;
  uint32_t readRoundCount() const;
  void drawFooter() const;
};
