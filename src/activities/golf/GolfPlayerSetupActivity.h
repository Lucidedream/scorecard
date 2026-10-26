#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "GolfPlayerSetupPolicy.h"
#include "GolfReviewFormat.h"
#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"

class GolfPlayerSetupActivity final : public GolfUiListActivity {
 public:
  GolfPlayerSetupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfCourseFile& courseFile,
                          GolfNineSelection selection);

  void onEnter() override;

 private:
  using Phase = GolfPlayerSetupPhase;

  static constexpr uint8_t PLAYER_ROW_COUNT = GolfRound::MAX_PLAYERS;
  // Row 0 edits the player name; rows 1.. are one per resolved course tee.
  static constexpr uint8_t TEE_ROW_COUNT = 1 + GOLF_MAX_TEES;

  GolfCourseFile courseFile{};
  GolfCourse course{};
  GolfNineSelection selection{};
  bool loadFailed = false;
  GolfRound draft{};
  freeink::ui::ListItem playerRows[PLAYER_ROW_COUNT + 1]{};
  freeink::ui::ListItem teeRows[TEE_ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  Phase phase = Phase::Count;
  uint8_t playerCount = 1;
  uint8_t editingPlayer = 0;
  char defaultTee[GOLF_TEE_CAPACITY] = {};
  bool saveFailed = false;
  bool teeResolutionFailed = false;
  char teeChoicePlayerLabel[GOLF_PLAYER_LABEL_CAPACITY]{};

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  void onBackButton() override;
  bool keepTouchFooter() const override { return golfPlayerSetupKeepsTouchFooter(phase); }
  golfui::FooterLabels golfFooterLabels() const override;
  void onFooterConfirm() override;
  void onFooterPrevious() override;
  void onFooterNext() override;
  const char* headerTitle() const override;
  void navigateButtons() override;
  void drawChrome() override;

  void refreshPlayerRows();
  void initializeTeeRows();
  bool rowIsEnabled(int index) const;
  int nextFocusableIndex(int current, int direction) const;
  void stepPlayerCount(int direction);
  void onCountTouch(const freeink::ui::ActionEvent& event);
  void drawTouchCount(UiScreen& screen);
  void showPlayers();
  void openTeeChoice(uint8_t player);
  void selectTee(const char* tee);
  void editPlayerName(uint8_t player);
  bool applyPlayerName(uint8_t player, std::string_view name);
  void returnToPlayers();
  void completeRound();

  uint8_t teeRowCount() const;
  const GolfCourseTee* findTee(const char* name) const;
};
