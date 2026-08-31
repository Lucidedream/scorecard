#pragma once

#include "activities/Activity.h"
#include "golf/GolfRound.h"

class GolfStatisticsActivity final : public Activity {
 public:
  GolfStatisticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round)
      : Activity("GolfStatistics", renderer, mappedInput), round(round) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int SECTION_HEIGHT = 34;
  static constexpr int STAT_ROW_HEIGHT = 62;
  static constexpr int SIDE_PADDING = 18;

  GolfRound round{};
  bool firstPaint = true;
  char roundStatus[20]{};

  void drawSection(int top, const char* label) const;
  void drawStat(int top, const char* label, uint16_t value, bool withPercent) const;
  void drawFooter() const;
};
