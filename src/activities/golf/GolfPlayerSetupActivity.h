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
  enum class Phase : uint8_t { Players, TeeChoice };

  static constexpr uint8_t PLAYER_ROW_COUNT = GolfRound::MAX_PLAYERS * 2 + 1;
  static constexpr uint8_t TEE_OPTION_COUNT = 3;
  static constexpr uint8_t COMPLETE_ROW = PLAYER_ROW_COUNT - 1;

  GolfCourseFile courseFile{};
  GolfCourse course{};
  GolfRound draft{};
  freeink::ui::ListItem playerRows[PLAYER_ROW_COUNT]{};
  freeink::ui::ListItem teeRows[TEE_OPTION_COUNT]{};
  freeink::ui::ListProps listProps{};
  Phase phase = Phase::Players;
  uint8_t editingPlayer = 0;
  bool saveFailed = false;
  bool teeResolutionFailed = false;
  char teeChoicePlayerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void navigateButtons() override;
  void drawChrome() override;

  void refreshPlayerRows();
  void initializeTeeRows();
  bool hasEnabledPlayer() const;
  bool rowIsEnabled(int index) const;
  int nextFocusableIndex(int current, int direction) const;
  void openTeeChoice(uint8_t player);
  void selectTee(TeeSelection tee);
  void editPlayerName(uint8_t player);
  bool applyPlayerName(uint8_t player, std::string_view name);
  void returnToPlayers();
  void completeRound();

  static const char* teeLabel(TeeSelection tee);
};
