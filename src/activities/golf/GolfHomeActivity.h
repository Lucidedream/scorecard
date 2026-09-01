#pragma once

#include "activities/UiListActivity.h"

class GolfHomeActivity final : public UiListActivity {
 public:
  GolfHomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfHome", renderer, mappedInput) {}

  void onEnter() override;

 private:
  static constexpr uint8_t MAX_ROWS = 4;
  freeink::ui::ListItem rows[MAX_ROWS]{};
  freeink::ui::ListProps listProps{};
  bool hasOpenRound = false;
  bool showNewRound = true;
  bool stateError = false;
  bool cleanupError = false;

  int listCount() const override { return (hasOpenRound ? 1 : 0) + (showNewRound ? 1 : 0) + 2; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void onBackButton() override { onGoHome(); }
};
