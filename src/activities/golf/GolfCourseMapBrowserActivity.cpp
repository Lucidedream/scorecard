#include "GolfCourseMapBrowserActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "GolfCourseMapImage.h"
#include "GolfLargeNumber.h"
#include "GolfNavigation.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr int16_t HOLE_BAND_HEIGHT = 84;

}  // namespace

GolfCourseMapBrowserActivity::GolfCourseMapBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                           const char* courseName)
    : Activity("GolfCourseMapBrowser", renderer, mappedInput) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

void GolfCourseMapBrowserActivity::onEnter() {
  Activity::onEnter();
  if (!CourseStore::resolveAllTees(courseName, teeSet)) {
    LOG_ERR("GOLF", "Course map browser: course not found: %s", courseName);
  }
  holeCount = teeSet.primary.holeCount;
  // golfSlug's ASCII-only fallback is intentionally shared by non-ASCII course names (CONTRACTS-V2 §25).
  slugValid = golfSlug(courseName, courseSlug, sizeof(courseSlug));
  currentHole = 0;
  renderHole();
}

void GolfCourseMapBrowserActivity::changeHole(const int delta) {
  if (holeCount == 0) return;
  const int next = static_cast<int>(currentHole) + delta;
  if (next < 0 || next >= holeCount) return;
  currentHole = static_cast<uint8_t>(next);
  renderHole();
}

void GolfCourseMapBrowserActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool swapped = mappedInput.isNavDirectionSwapped();
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    changeHole(golfFrontNavDelta(swapped, true));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    changeHole(golfFrontNavDelta(swapped, false));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    changeHole(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) changeHole(1);
}

bool GolfCourseMapBrowserActivity::formatTeeYardageLine(const uint8_t hole, char* output, const size_t size) const {
  output[0] = '\0';
  const bool hasBlueYards = teeSet.hasBlue && teeSet.blue.hasYards;
  const bool hasWhiteYards = teeSet.hasWhite && teeSet.white.hasYards;
  char blueSeg[32]{};
  char whiteSeg[32]{};
  if (hasBlueYards) {
    snprintf(blueSeg, sizeof(blueSeg), tr(STR_GOLF_TEE_YARDS_FORMAT), tr(STR_GOLF_BLUE),
             static_cast<unsigned>(teeSet.blue.yards[hole]), tr(STR_GOLF_YARDS_UNIT));
  }
  if (hasWhiteYards) {
    snprintf(whiteSeg, sizeof(whiteSeg), tr(STR_GOLF_TEE_YARDS_FORMAT), tr(STR_GOLF_WHITE),
             static_cast<unsigned>(teeSet.white.yards[hole]), tr(STR_GOLF_YARDS_UNIT));
  }
  if (hasBlueYards && hasWhiteYards) {
    snprintf(output, size, tr(STR_GOLF_TEE_YARDS_JOIN_FORMAT), blueSeg, whiteSeg);
    return true;
  }
  if (hasBlueYards) {
    snprintf(output, size, "%s", blueSeg);
    return true;
  }
  if (hasWhiteYards) {
    snprintf(output, size, "%s", whiteSeg);
    return true;
  }
  return false;
}

void GolfCourseMapBrowserActivity::drawHoleBand(const freeink::ui::Rect rect) const {
  const int padding = golfui::minValue(18, static_cast<int16_t>(rect.width / 8));
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, rect.y + 4, tr(STR_GOLF_HOLE), true, EpdFontFamily::BOLD);
  const int digitHeight = golfui::clampValue(rect.height - lineHeight - 10, 24, golfui::SCORING_HOLE_DIGIT_MAX_HEIGHT);
  golfDrawLargeNumber(renderer, rect.x + rect.width / 4, rect.y + rect.height - digitHeight - 5, digitHeight,
                      static_cast<uint16_t>(currentHole + 1));

  const int right = rect.x + rect.width - padding;
  int y = rect.y + 4;
  const uint8_t par = currentHole < teeSet.primary.holeCount ? teeSet.primary.par[currentHole] : 0;
  char line[64];
  if (par != 0) {
    snprintf(line, sizeof(line), tr(STR_GOLF_LABEL_VALUE_FORMAT), tr(STR_GOLF_PAR), static_cast<unsigned>(par));
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), y, line, true,
                      EpdFontFamily::BOLD);
    y += lineHeight;
  }
  if (formatTeeYardageLine(currentHole, line, sizeof(line))) {
    renderer.drawText(UI_10_FONT_ID, right - renderer.getTextWidth(UI_10_FONT_ID, line), y, line);
  }
  renderer.drawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1);
}

void GolfCourseMapBrowserActivity::renderMessage(const freeink::ui::Rect body, const char* message) const {
  UITheme::drawCenteredWrappedText(renderer, Rect{body.x, body.y, body.width, body.height}, UI_10_FONT_ID, message, 3,
                                   true, EpdFontFamily::REGULAR, UITheme::TextVerticalAlignment::CENTER);
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapBrowserActivity::drawFooter() const {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void GolfCourseMapBrowserActivity::renderHole() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, metrics.topPadding);
  const int16_t bandHeight = golfui::minValue(HOLE_BAND_HEIGHT, chrome.body.height);
  const freeink::ui::Rect holeBand{chrome.body.x, chrome.body.y, chrome.body.width, bandHeight};
  const freeink::ui::Rect imageBody{chrome.body.x, static_cast<int16_t>(chrome.body.y + bandHeight), chrome.body.width,
                                    static_cast<int16_t>(chrome.body.height - bandHeight)};

  renderer.clearScreen();
  golfui::drawHeader(renderer, chrome.header, courseName);
  drawHoleBand(holeBand);
  drawFooter();

  if (!slugValid) {
    LOG_ERR("GOLF", "Course map load failed: invalid slug");
    renderMessage(imageBody, tr(STR_GOLF_MAP_LOAD_FAILED));
    return;
  }

  const GolfCourseMapImageResult result =
      golfRenderCourseMapImage(renderer, courseSlug, static_cast<uint8_t>(currentHole + 1), imageBody);
  switch (result) {
    case GolfCourseMapImageResult::Rendered:
      return;
    case GolfCourseMapImageResult::Missing:
      renderMessage(imageBody, tr(STR_GOLF_MAP_MISSING));
      return;
    case GolfCourseMapImageResult::LoadFailed:
      renderMessage(imageBody, tr(STR_GOLF_MAP_LOAD_FAILED));
      return;
  }
}

#endif
