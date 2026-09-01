#pragma once

#include <FreeInkUICore.h>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "golf/GolfRound.h"

class GolfStatisticsActivity final : public Activity {
 public:
  GolfStatisticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round,
                         uint8_t playerSlot)
      : Activity("GolfStatistics", renderer, mappedInput), round(round), playerSlot(playerSlot) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int SIDE_PADDING = 18;

  // Heap-owned activity snapshot; playerSlot is the stable History selection.
  GolfRound round{};
  uint8_t playerSlot = GolfRound::NO_PLAYER;
  bool firstPaint = true;
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  char roundStatus[20]{};

  const GolfPlayer& selectedPlayer() const { return round.players[playerSlot]; }
  void drawSection(freeink::ui::Rect rect, const char* label, const char* rightLabel = nullptr) const;
  void drawStat(freeink::ui::Rect rect, const char* label, uint16_t value, bool withPercent) const;
  void drawFooter() const;
};
