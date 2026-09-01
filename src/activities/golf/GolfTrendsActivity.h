#pragma once

#include <memory>

#include "GolfReviewFormat.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"
#include "golf/GolfTrends.h"

class GolfTrendsActivity final : public Activity, protected UiAppHost {
 public:
  GolfTrendsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t playerSlot,
                     const char* fallbackName);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr uint8_t MAX_ROWS = 8;

  struct TrendsState {
    GolfHistoryReader history{};
    GolfTrendStats trends{};
    char subtitle[40]{};
    char message[72]{};
    bool loadError = false;
  };

  struct TrendsStagingScratch {
    TrendsState staging{};
    GolfIndexMigrator recovery{};
    char chunk[128]{};
  };

  // A checked inactive state keeps SD parsing outside RenderLock; one pointer
  // swap publishes the selected slot's reader, statistics, and text together.
  TrendsState residentState{};
  std::unique_ptr<TrendsStagingScratch> stagingOwner;
  TrendsState* activeState = &residentState;
  TrendsState* stagingState = nullptr;
  freeink::ui::TableProps tableProps{};
  char cells[MAX_ROWS][3][24]{};
  const uint8_t playerSlot;
  char fallbackName[GolfPlayer::NAME_CAPACITY]{};
  char playerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  static void screenTrampoline(UiScreen& screen, void* user);
  static void logMalformed(uint32_t lineNumber, void* user);
  void loadHistory();
  bool streamIndex(TrendsState& state);
  void refreshTrends(TrendsState& state);
  void publishState();
  void showStagingError();
  void buildScreen(UiScreen& screen);
};
