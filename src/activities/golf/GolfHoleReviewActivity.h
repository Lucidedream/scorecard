#pragma once

#include <FreeInkUICore.h>

#include <cstddef>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "golf/GolfRound.h"
#include "golf/GolfRules.h"

class GolfHoleReviewActivity final : public Activity {
 public:
  GolfHoleReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round,
                         uint8_t playerSlot)
      : Activity("GolfHoleReview", renderer, mappedInput), round(round), playerSlot(playerSlot) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int SIDE_PADDING = 18;

  // Heap-owned activity snapshot; playerSlot is the stable History selection.
  GolfRound round{};
  uint8_t currentHole = 0;
  uint8_t playerSlot = GolfRound::NO_PLAYER;
  bool firstPaint = true;
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};
  char roundStatus[20]{};

  const GolfPlayer& selectedPlayer() const { return round.players[playerSlot]; }
  void changeHole(int delta);
  void drawHoleBand(freeink::ui::Rect rect) const;
  void drawScoreBand(freeink::ui::Rect rect) const;
  void drawDetailRow(freeink::ui::Rect rect, const char* label, uint16_t value, GolfField field) const;
  void drawPenaltyBand(freeink::ui::Rect rect) const;
  void formatFieldMarkers(GolfField field, char* output, size_t size) const;
  void drawFooter() const;
};
