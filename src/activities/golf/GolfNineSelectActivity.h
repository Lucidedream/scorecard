#pragma once

#include "GolfUiListActivity.h"
#include "golf/CourseStore.h"

class GolfNineSelectActivity final : public GolfUiListActivity {
 public:
  GolfNineSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const GolfCourseFile& file,
                         GolfNineSelection initial);
  void onEnter() override;

 private:
  enum class Phase : uint8_t { Rows, NineList };
  static constexpr uint8_t ROW_COUNT = 4;
  GolfCourseFile courseFile{};
  GolfCourse course{};
  GolfNineSelection selection{};
  Phase phase = Phase::Rows;
  bool backSlot = false;
  bool loadFailed = false;
  freeink::ui::ListItem rows[ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  char details[GOLF_MAX_NINES][48]{};
  char parLabels[GOLF_MAX_NINES][8]{};

  int listCount() const override;
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  void navigateButtons() override;
  void drawChrome() override;
  golfui::FooterLabels golfFooterLabels() const override;
  void refreshRows();
  void returnToRows();
};
