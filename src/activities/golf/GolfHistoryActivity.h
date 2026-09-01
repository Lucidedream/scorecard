#pragma once

#include <memory>

#include "GolfReviewFormat.h"
#include "activities/UiListActivity.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"
#include "golf/GolfRound.h"

class GolfHistoryActivity final : public UiListActivity {
 public:
  GolfHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t playerSlot,
                      const char* fallbackName);

  void onEnter() override;

 private:
  static constexpr uint8_t WINDOW_ROWS = 16;

  struct HistoryState {
    GolfHistoryReader history{};
    bool loadError = false;
  };

  struct HistoryLookupScratch {
    HistoryState staging{};
    GolfIndexMigrator recovery{};
    GolfIndexFileLocator locator{};
    GolfRound round{};
    char chunk[128]{};
    char path[sizeof("/golf/rounds/") + GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
    char filename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  };

  // One checked allocation retains the inactive ring plus recovery, locator,
  // round, and I/O scratch. Reloads fill the inactive state and publish it with
  // one pointer swap, including the post-delete reload.
  HistoryState residentState{};
  std::unique_ptr<HistoryLookupScratch> lookupOwner;
  HistoryState* activeState = &residentState;
  HistoryState* stagingState = nullptr;
  freeink::ui::ListItem visibleRows[WINDOW_ROWS]{};
  freeink::ui::ListProps listProps{};
  char visibleValues[WINDOW_ROWS][20]{};
  char visibleDates[WINDOW_ROWS][GOLF_DATE_BUFFER_SIZE]{};
  const uint8_t playerSlot;
  char fallbackName[GolfPlayer::NAME_CAPACITY]{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  int listCount() const override { return activeState->history.count(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void drawFooter() override;
  void loadHistory();
  bool streamIndex(HistoryState& state);
  void publishState();
  void showStagingError();
  // Resolves one selected-slot row in a second streaming pass and loads only
  // that row's shared group-round JSON. Missing/invalid JSON falls back to the
  // already selected CSV entry.
  bool loadArchivedRound(uint8_t newestIndex);
  static void logMalformed(uint32_t lineNumber, void* user);
};
