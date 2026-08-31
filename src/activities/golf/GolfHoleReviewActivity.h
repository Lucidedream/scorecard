#pragma once

#include <cstddef>

#include "activities/Activity.h"
#include "golf/GolfRound.h"
#include "golf/GolfRules.h"

class GolfHoleReviewActivity final : public Activity {
 public:
  GolfHoleReviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round)
      : Activity("GolfHoleReview", renderer, mappedInput), round(round) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int HOLE_BAND_HEIGHT = 120;
  static constexpr int SCORE_BAND_HEIGHT = 180;
  static constexpr int DETAIL_ROW_HEIGHT = 76;
  static constexpr int PENALTY_BAND_HEIGHT = 60;
  static constexpr int SIDE_PADDING = 18;

  GolfRound round{};
  uint8_t currentHole = 0;
  uint8_t si[GolfRound::MAX_HOLES]{};
  bool hasSi = false;
  bool firstPaint = true;
  char roundStatus[20]{};

  void loadStrokeIndexes();
  void changeHole(int delta);
  void drawHoleBand(int top) const;
  void drawScoreBand(int top) const;
  void drawDetailRow(int top, const char* label, uint16_t value, GolfField field) const;
  void drawPenaltyBand(int top) const;
  void formatFieldMarkers(GolfField field, char* output, size_t size) const;
  void drawFooter() const;
};
