#pragma once

#include "activities/UiListActivity.h"
#include "golf/GolfHistory.h"
#include "golf/GolfRound.h"

class GolfHistoryActivity final : public UiListActivity {
 public:
  GolfHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfHistory", renderer, mappedInput) {}

  void onEnter() override;

 private:
  static constexpr uint8_t WINDOW_ROWS = 16;

  GolfHistoryReader history;
  freeink::ui::ListItem visibleRows[WINDOW_ROWS]{};
  char visibleValues[WINDOW_ROWS][20]{};
  char visibleDates[WINDOW_ROWS][GOLF_DATE_BUFFER_SIZE]{};
  bool loadError = false;

  int listCount() const override { return history.count(); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void loadHistory();
  // Resolves the round file for the newest-first row `newestIndex` (second pass of
  // index.csv, per CONTRACTS-V2 §8) and loads it. Returns false — falling the
  // caller back to the CSV-only summary — when the row has no file, the file is
  // gone, or the file fails validation.
  bool loadArchivedRound(uint8_t newestIndex, GolfRound& out, char* filename, size_t filenameSize);
  static void logMalformed(uint32_t lineNumber, void* user);
};
