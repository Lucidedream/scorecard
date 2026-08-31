#pragma once

#include "activities/UiListActivity.h"
#include "golf/GolfPaths.h"
#include "golf/GolfRound.h"

class GolfHistoryRoundMenuActivity final : public UiListActivity {
 public:
  GolfHistoryRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfRound& round,
                               const char* filename);

  void onEnter() override;

 private:
  static constexpr uint8_t ROW_COUNT = 4;
  static constexpr int16_t MENU_ROW_HEIGHT = 82;
  static constexpr int16_t INFO_BAND_HEIGHT = 70;

  GolfRound round{};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  char archiveFilename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  char status[20]{};
  char infoLine1[48]{};
  char infoLine2[48]{};
  char deletePrompt[96]{};
  bool deleteFailed = false;

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void drawChrome() override;
  void drawFooter() override;
  void confirmDelete();
  void completeDelete(bool confirmed);
};
