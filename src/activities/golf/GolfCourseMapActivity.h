#pragma once

#if defined(CROSSPOINT_GOLF)

#include "GolfUiLayout.h"
#include "activities/Activity.h"
#include "golf/GolfPaths.h"

class GolfCourseMapActivity final : public Activity {
 public:
  GolfCourseMapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint8_t holeIndex, uint8_t holeCount,
                        const char* courseName);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  uint8_t holeIndex;
  uint8_t holeCount;
  char courseName[40]{};
  char courseSlug[GOLF_SLUG_BUFFER_SIZE]{};
  bool slugValid = false;

  void drawChrome(const char* message = nullptr) const;
  void renderMissing() const;
  void renderLoadFailed() const;
  void loadAndRender();
};

#endif
