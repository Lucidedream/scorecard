#pragma once

#include <memory>

#include "activities/UiListActivity.h"
#include "golf/GolfTips.h"

// The note list (CONTRACTS-V2 §25.3): one row per /golf/tips/*.txt file, titled
// by the note's first line, with its section count. Empty and unreadable are
// kept distinct, as §22.4 established for the player picker.
class GolfTipListActivity final : public UiListActivity {
 public:
  GolfTipListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("GolfTipList", renderer, mappedInput) {}

  void onEnter() override;

 private:
  struct Scratch {
    GolfTipEntry entries[GOLF_MAX_TIPS];
    freeink::ui::ListItem rows[GOLF_MAX_TIPS];
    char subtitles[GOLF_MAX_TIPS][16];
  };

  std::unique_ptr<Scratch> scratch_;
  freeink::ui::ListProps listProps_{};
  GolfTipsListState state_ = GolfTipsListState::Empty;
  uint8_t noteCount_ = 0;

  int listCount() const override { return state_ == GolfTipsListState::Ready ? noteCount_ : 0; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void loadNotes();
};
