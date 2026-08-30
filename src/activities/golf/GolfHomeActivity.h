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
  bool hasOpenRound = false;
  bool stateError = false;

  int listCount() const override { return hasOpenRound ? 4 : 3; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void onBackButton() override { onGoHome(); }
};
