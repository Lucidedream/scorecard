#pragma once

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfRound.h"

class GolfCardActivity final : public Activity, protected UiAppHost {
 public:
  // Live round: the card copies the in-progress group round from the store in onEnter().
  GolfCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GolfCard", renderer, mappedInput), UiAppHost(renderer) {}

  // Archived round: the card renders this group copy and never touches the store.
  GolfCardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& archivedRound)
      : Activity("GolfCard", renderer, mappedInput), UiAppHost(renderer), round(archivedRound), archived(true) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t MAX_TABLE_ROWS = GolfRound::MAX_PLAYERS + 2;
  static constexpr uint8_t MAX_DATA_COLS = 11;
  static constexpr uint8_t LABEL_COLUMN_UNITS = 2;
  static constexpr freeink::ui::ActionId ACTION_TAB = 1;

  // GolfRound is 906 bytes. Activities are heap-owned, so the live/archive snapshot
  // stays here rather than consuming the embedded activity task stack.
  GolfRound round{};
  char dataCells[MAX_TABLE_ROWS][MAX_DATA_COLS][8]{};
  const char* dataPointers[MAX_DATA_COLS]{};
  const char* labelPointer[1]{};
  freeink::ui::TabItem segmentTabs[2]{};
  freeink::ui::TabBarProps tabProps{};
  freeink::ui::TableProps tableProps{};
  uint8_t playerSlots[GolfRound::MAX_PLAYERS]{};
  char playerLabels[GolfRound::MAX_PLAYERS][GOLF_PLAYER_LABEL_CAPACITY]{};
  uint8_t playerCount = 0;
  uint8_t activeTab = 0;
  bool firstPaint = true;
  bool archived = false;
  int16_t tableTop = 0;
  int16_t tableHeight = 0;
  int16_t dataLeft = 0;
  int16_t dataWidth = 0;
  uint8_t tableRows = 0;
  uint8_t tableHeaderRows = 0;
  uint8_t tableDataColumns = 0;
  uint8_t tableFirstHole = 0;

  static void screenTrampoline(UiScreen& screen, void* user);
  static void tabTrampoline(const freeink::ui::ActionEvent& event, void* user);
  void buildScreen(UiScreen& screen);
  void buildCard(UiScreen& screen, freeink::ui::Rect tableRect, uint8_t firstHole, const char* segmentLabel);
  void collectPlayers();
  void changeTab(int delta);
  void drawPenaltyMarkers() const;
  void drawFooter() const;
};
