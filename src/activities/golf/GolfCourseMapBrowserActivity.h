#pragma once

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUICore.h>

#include <cstddef>
#include <cstdint>

#include "GolfUiLayout.h"
#include "activities/Activity.h"
#include "golf/CourseStore.h"
#include "golf/GolfPaths.h"

// Read-only course information.
class GolfCourseMapBrowserActivity final : public Activity {
 public:
  GolfCourseMapBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* courseName);

  void onEnter() override;
  void loop() override;

 private:
  char courseName[40]{};
  char courseSlug[GOLF_SLUG_BUFFER_SIZE]{};
  bool slugValid = false;
  GolfCourse course{};
  GolfCourseFile courseFile{};
  uint8_t holeCount = 0;
  uint8_t currentHole = 0;

  void changeHole(int delta);
  void openTeeInfo();
  void renderHole();
  void drawHoleBand(freeink::ui::Rect rect) const;
  bool formatTeeYardageLine(uint8_t hole, char* output, size_t size) const;
  void renderMessage(freeink::ui::Rect body, const char* message) const;
  void drawFooter() const;
};

#endif
