#pragma once

#include "activities/UiListActivity.h"

class GolfRoundMenuActivity final : public UiListActivity {
 public:
  GolfRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfRoundMenu", renderer, mappedInput) {}

  void onEnter() override;

 private:
  enum class PendingAction : uint8_t { None, Finish, Abandon };
  freeink::ui::ListItem rows[3]{};
  freeink::ui::ListProps listProps{};
  PendingAction pendingAction = PendingAction::None;
  const char* errorMessage = nullptr;

  int listCount() const override { return 3; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void confirmAction(PendingAction action);
  void completeAction(bool confirmed);
};
