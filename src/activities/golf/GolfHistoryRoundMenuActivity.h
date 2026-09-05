#pragma once

#include "activities/UiListActivity.h"
#include "golf/GolfPaths.h"
#include "golf/GolfRound.h"

class GolfHistoryRoundMenuActivity final : public UiListActivity {
 public:
  GolfHistoryRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round,
                               const char* filename, uint8_t playerSlot);

  void onEnter() override;

 private:
  static constexpr uint8_t ROW_COUNT = 5;

  // The archived group snapshot is activity-owned (heap), never an automatic
  // 906-byte task-stack value. playerSlot identifies the History-selected row.
  GolfRound round{};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  char archiveFilename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  char status[20]{};
  char infoLine1[48]{};
  char infoLine2[48]{};
  char deletePrompt[128]{};
  uint8_t playerSlot = GolfRound::NO_PLAYER;
  bool deleteFailed = false;
  bool exportFailed = false;

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void drawChrome() override;
  void drawFooter() override;
  const GolfPlayer& selectedPlayer() const { return round.players[playerSlot]; }
  void confirmDelete();
  void completeDelete(bool confirmed);
};
