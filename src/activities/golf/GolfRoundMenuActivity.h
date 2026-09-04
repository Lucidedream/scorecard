#pragma once

#include "activities/UiListActivity.h"

class GolfRoundMenuActivity final : public UiListActivity {
 public:
  GolfRoundMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfRoundMenu", renderer, mappedInput) {}

  void onEnter() override;

 private:
  enum class PendingAction : uint8_t { None, Finish, Abandon };
  // View card, Abandon round, Finish round, Tips — Tips last because it is the
  // one reached repeatedly while the others end or inspect the round
  // (CONTRACTS-V2 §25.3).
  static constexpr int ROW_COUNT = 4;
  freeink::ui::ListItem rows[ROW_COUNT]{};
  freeink::ui::ListProps listProps{};
  PendingAction pendingAction = PendingAction::None;
  const char* errorMessage = nullptr;

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void confirmAction(PendingAction action);
  void completeAction(bool confirmed);
};
