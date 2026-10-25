#pragma once

#include "GolfRoundMenuRows.h"
#include "GolfUiListActivity.h"

class GolfRoundMenuActivity final : public GolfUiListActivity {
 public:
  GolfRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : GolfUiListActivity("GolfRoundMenu", renderer, mappedInput) {}

  void onEnter() override;

 private:
  enum class PendingAction : uint8_t { None, Finish, Abandon };
  freeink::ui::ListItem rows[golfmenu::ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  PendingAction pendingAction = PendingAction::None;
  const char* errorMessage = nullptr;

  int listCount() const override { return golfmenu::ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void confirmAction(PendingAction action);
  void completeAction(bool confirmed);
};
