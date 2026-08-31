#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfRound.h"

class GolfCardActivity final : public Activity, protected UiAppHost {
 public:
  // Live round: the card copies the in-progress round from the store in onEnter().
  GolfCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GolfCard", renderer, mappedInput), UiAppHost(renderer) {}

  // Archived round: the card renders this copy and never touches the store. The
  // card has no mutation path in either mode (tab switching and Back only), so
  // read-only is structural rather than enforced.
  GolfCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& archivedRound)
      : Activity("GolfCard", renderer, mappedInput), UiAppHost(renderer), round(archivedRound), archived(true) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t MAX_TABLE_ROWS = 3;
  static constexpr uint8_t MAX_TABLE_COLS = 12;
  static constexpr freeink::ui::ActionId ACTION_TAB = 1;
  static constexpr int16_t HEADER_ROW_HEIGHT = 40;
  static constexpr int16_t PAR_ROW_HEIGHT = 44;
  static constexpr int16_t SCORE_ROW_HEIGHT = 56;

  GolfRound round{};
  char cells[MAX_TABLE_ROWS][MAX_TABLE_COLS][8]{};
  uint8_t activeTab = 0;
  bool firstPaint = true;
  bool archived = false;
  int16_t tableLeft = 0;
  int16_t tableTop = 0;
  int16_t tableWidth = 0;
  uint8_t tableColumns = 0;
  uint8_t tableFirstHole = 0;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void tabTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void buildCard(UiScreen& screen, uint8_t firstHole, const char* segmentLabel);
  void changeTab(int delta);
  void drawPenaltySuperscripts() const;
  void drawFooter() const;
};
