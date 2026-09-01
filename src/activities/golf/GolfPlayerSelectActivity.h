#pragma once

#include <atomic>
#include <cstdint>

#include "GolfReviewFormat.h"
#include "activities/UiListActivity.h"
#include "golf/GolfHistory.h"
#include "golf/GolfIndexMigrate.h"

class GolfPlayerSelectActivity final : public UiListActivity {
 public:
  enum class Mode : uint8_t { History, Trends };

  GolfPlayerSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode)
      : UiListActivity("GolfPlayerSel", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void loop() override;

 private:
  static constexpr uint8_t ROW_COUNT = GolfRound::MAX_PLAYERS;
  static_assert(sizeof(std::atomic<bool>) == 1, "Selector refresh flag must stay one byte");

  const Mode mode;
  GolfPlayerNamesReader playerNames{};
  GolfIndexMigrator recovery{};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  char playerNamesSnapshot[ROW_COUNT][GolfPlayer::NAME_CAPACITY]{};
  char playerLabels[ROW_COUNT][GOLF_PLAYER_LABEL_CAPACITY]{};
  char chunk[128]{};
  std::atomic<bool> refreshPending{true};
  uint8_t presentMask = 0;
  bool loadError = false;

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void navigateButtons() override;
  void drawChrome() override;
  void drawFooter() override;
  const char* headerTitle() const override;

  void scanPlayers();
  void publishPlayers(bool success);
  bool rowIsEnabled(int index) const;
};
