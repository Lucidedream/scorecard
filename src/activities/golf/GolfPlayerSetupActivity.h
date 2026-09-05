#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "GolfReviewFormat.h"
#include "activities/UiListActivity.h"
#include "golf/CourseStore.h"

class GolfPlayerSetupActivity final : public UiListActivity {
 public:
  GolfPlayerSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfCourseFile& courseFile,
                          const GolfCourse& course);

  void onEnter() override;

 private:
  enum class Phase : uint8_t { Count, Players, TeeChoice };

  static constexpr uint8_t PLAYER_ROW_COUNT = GolfRound::MAX_PLAYERS;
  static constexpr uint8_t TEE_OPTION_COUNT = 3;

  GolfCourseFile courseFile{};
  GolfCourse course{};
  GolfRound draft{};
  freeink::ui::ListItem playerRows[PLAYER_ROW_COUNT + 1]{};
  freeink::ui::ListItem teeRows[TEE_OPTION_COUNT]{};
  freeink::ui::ListProps listProps{};
  Phase phase = Phase::Count;
  uint8_t playerCount = 1;
  uint8_t editingPlayer = 0;
  TeeSelection defaultTee = TeeSelection::NotPlay;
  bool saveFailed = false;
  bool teeResolutionFailed = false;
  char teeChoicePlayerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void onBackButton() override;
  bool handleButtons() override;
  const char* headerTitle() const override;
  void navigateButtons() override;
  void drawChrome() override;
  void drawFooter() override;

  void refreshPlayerRows();
  void initializeTeeRows();
  bool rowIsEnabled(int index) const;
  int nextFocusableIndex(int current, int direction) const;
  void stepPlayerCount(int direction);
  void showPlayers();
  void openTeeChoice(uint8_t player);
  void selectTee(TeeSelection tee);
  void editPlayerName(uint8_t player);
  bool applyPlayerName(uint8_t player, std::string_view name);
  void returnToPlayers();
  void completeRound();

  static const char* teeLabel(TeeSelection tee);
};
