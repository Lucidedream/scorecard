#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfHistory.h"
#include "golf/GolfTrends.h"

class GolfTrendsActivity final : public Activity, protected UiAppHost {
 public:
  GolfTrendsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GolfTrends", renderer, mappedInput), UiAppHost(renderer) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t MAX_ROWS = 7;
  GolfHistoryReader history;
  GolfTrendStats trends{};
  char cells[MAX_ROWS][3][24]{};
  char subtitle[40]{};
  char message[72]{};
  bool loadError = false;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void logMalformed(uint32_t lineNumber, void* user);
  void loadHistory();
  void buildScreen(UiScreen& screen);
};
