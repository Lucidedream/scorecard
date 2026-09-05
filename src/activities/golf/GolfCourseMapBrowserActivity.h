#pragma once

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUICore.h>

#include <cstddef>
#include <cstdint>

#include "GolfUiLayout.h"
#include "activities/Activity.h"
#include "golf/CourseStore.h"
#include "golf/GolfPaths.h"

// Read-only, full-course hole browser (CONTRACTS-V2 §30): opened from
// GolfCourseMapListActivity with just a course name, it resolves every tee's data for that
// course once via CourseStore::resolveAllTees() and pages hole-by-hole with Prev/Next,
// showing the hole number, par, every tee's yardage, and the course-map image (via the
// shared golfRenderCourseMapImage(), same source as GolfCourseMapActivity's in-round
// glance screen). No GolfRound is ever in scope; nothing here can start or mutate a round.
class GolfCourseMapBrowserActivity final : public Activity {
 public:
  GolfCourseMapBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* courseName);

  void onEnter() override;
  void loop() override;

 private:
  char courseName[40]{};
  char courseSlug[GOLF_SLUG_BUFFER_SIZE]{};
  bool slugValid = false;
  GolfCourseTeeSet teeSet{};
  uint8_t holeCount = 0;
  uint8_t currentHole = 0;

  void changeHole(int delta);
  void renderHole();
  void drawHoleBand(freeink::ui::Rect rect) const;
  bool formatTeeYardageLine(uint8_t hole, char* output, size_t size) const;
  void renderMessage(freeink::ui::Rect body, const char* message) const;
  void drawFooter() const;
};

#endif
