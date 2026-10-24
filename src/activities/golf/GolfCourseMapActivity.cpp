#include "GolfCourseMapActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseMapImage.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/CourseStore.h"

GolfCourseMapActivity::GolfCourseMapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const uint8_t holeIndex, const uint8_t holeCount, const char* courseName,
                                             uint8_t courseHoleNumber)
    : Activity("GolfCourseMap", renderer, mappedInput),
      holeIndex(holeIndex),
      holeCount(holeCount),
      courseHoleNumber(courseHoleNumber) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

void GolfCourseMapActivity::onEnter() {
  Activity::onEnter();
  // The course is larger than the task stack budget and lives only during resolution.
  auto course = makeUniqueNoThrow<GolfCourse>();
  GolfCourseFile file{};
  if (!course) LOG_ERR("GOLF", "OOM: glance course lookup");
  slugValid = course && CourseStore::findByName(courseName, file, *course)
                  ? golfCourseMapsDir(*course, courseSlug, sizeof(courseSlug))
                  : golfSlug(courseName, courseSlug, sizeof(courseSlug));
  loadAndRender();
}

void GolfCourseMapActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapActivity::loop() {
  golfui::FooterInput input(mappedInput, {tr(STR_GOLF_FOOTER_BACK), "", "", ""});
  if (input.wasReleased(golfui::FooterAction::Back)) finish();
}

void GolfCourseMapActivity::drawChrome(const char* message) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  char title[32];
  snprintf(title, sizeof(title), tr(STR_GOLF_MAP_TITLE_FORMAT), static_cast<unsigned>(holeIndex + 1));
  golfui::drawHeader(renderer, layout.header, title);
  if (message != nullptr) {
    UITheme::drawCenteredWrappedText(
        renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height}, UI_10_FONT_ID, message, 3,
        true, EpdFontFamily::REGULAR, UITheme::TextVerticalAlignment::CENTER);
  }
  golfui::drawFooter(renderer, mappedInput, {tr(STR_GOLF_FOOTER_BACK), "", "", ""});
}

void GolfCourseMapActivity::renderMissing() const {
  renderer.clearScreen();
  drawChrome(tr(STR_GOLF_MAP_MISSING));
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapActivity::renderLoadFailed() const {
  renderer.clearScreen();
  drawChrome(tr(STR_GOLF_MAP_LOAD_FAILED));
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapActivity::loadAndRender() {
  if (!slugValid || holeIndex >= holeCount) {
    LOG_ERR("GOLF", "Course map load failed: invalid slug or hole index");
    renderLoadFailed();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  renderer.clearScreen();
  drawChrome();

  const GolfCourseMapImageResult result = golfRenderCourseMapImage(renderer, courseSlug, courseHoleNumber, layout.body);
  switch (result) {
    case GolfCourseMapImageResult::Rendered:
      return;
    case GolfCourseMapImageResult::Missing:
      renderMissing();
      return;
    case GolfCourseMapImageResult::LoadFailed:
      renderLoadFailed();
      return;
  }
}

#endif
