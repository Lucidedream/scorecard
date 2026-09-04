#pragma once

#include <memory>

#include "activities/Activity.h"
#include "golf/GolfTips.h"

// Reads one note, paging by SECTION (CONTRACTS-V2 §25.2). Left/Right and the
// side rocker turn pages; only the current section is resident. A section that
// does not fit the screen is marked, never silently truncated.
class GolfTipNoteActivity final : public Activity {
 public:
  GolfTipNoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* filename, const char* title);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  char filename_[GOLF_TIP_FILENAME_BUFFER_SIZE]{};
  char title_[GOLF_TIP_TITLE_BUFFER_SIZE]{};
  std::unique_ptr<GolfTipSection> section_;  // the single resident section
  uint16_t current_ = 0;
  uint16_t sectionCount_ = 0;
  bool loadError_ = false;

  void loadSection(uint16_t index);
  void turnPage(int delta);
  void drawSection() const;
};
