#include "GolfCourseMapBrowserActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "GolfCourseMapImage.h"
#include "GolfCourseTeeInfoActivity.h"
#include "GolfLargeNumber.h"
#include "GolfNavigation.h"
#include "GolfReviewFormat.h"
#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "golf/GolfGlanceTees.h"

namespace {

constexpr int16_t HOLE_BAND_HEIGHT = 84;

}  // namespace

GolfCourseMapBrowserActivity::GolfCourseMapBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                           const char* courseName)
    : Activity("GolfCourseMapBrowser", renderer, mappedInput) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

GolfCourseMapBrowserActivity::GolfCourseMapBrowserActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                           const GolfCourseFile& file)
    : GolfCourseMapBrowserActivity(renderer, mappedInput, static_cast<const char*>(nullptr)) {
  courseFile = file;
  hasCourseFile = true;
}

void GolfCourseMapBrowserActivity::onEnter() {
  Activity::onEnter();
  if (!(hasCourseFile ? CourseStore::load(courseFile, course)
                      : CourseStore::findByName(courseName, courseFile, course))) {
    LOG_ERR("GOLF", "Course map browser: course not found: %s", courseName);
  }
  if (course.courseName[0]) snprintf(courseName, sizeof(courseName), "%s", course.courseName);
  holeCount = (9 * course.nineCount);
  slugValid = golfCourseMapsDir(course, courseSlug, sizeof(courseSlug));
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
  golfui::FooterInput input(mappedInput,
                            {tr(STR_BACK), tr(STR_GOLF_TEE), tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT)});
  if (input.wasReleased(golfui::FooterAction::Back)) {
    finish();
    return;
  }
  const int swipeDelta = golfui::pagingSwipeDelta(mappedInput);
  if (swipeDelta) {
    changeHole(swipeDelta);
    return;
  }
  if (input.hasTap() && holeCount) {
    const auto chrome = golfui::chromeLayout(renderer, mappedInput, UITheme::getInstance().getMetrics().topPadding);
    if (input.tapIn({chrome.body.x, chrome.body.y, chrome.body.width,
                     static_cast<int16_t>(golfui::minValue(HOLE_BAND_HEIGHT, chrome.body.height))})) {
      openTeeInfo();
      return;
    }
  }
  const int frontDelta = input.releasedFrontNavigationDelta();
  if (frontDelta != 0) {
    changeHole(frontDelta);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
    changeHole(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    changeHole(1);
    return;
  }
  if (input.wasReleased(golfui::FooterAction::Confirm)) openTeeInfo();
}

void GolfCourseMapBrowserActivity::openTeeInfo() {
  auto page = makeUniqueNoThrow<GolfCourseTeeInfoActivity>(renderer, mappedInput, courseFile);
  if (!page) {
    LOG_ERR("GOLF", "OOM: tee info");
    return;
  }
  startActivityForResult(std::move(page), nullptr);
}

bool GolfCourseMapBrowserActivity::formatTeeYardageLine(const uint8_t hole, char* output, const size_t size) const {
  output[0] = '\0';
  const GolfCourseTee* picked[2];
  const uint8_t pickedCount = pickGlanceTees(course, picked);
  bool any = false;
  for (uint8_t i = 0; i < pickedCount; ++i) {
    const GolfCourseTee& tee = *picked[i];
    char segment[40];
    if (snprintf(segment, sizeof(segment), tr(STR_GOLF_TEE_YARDS_FORMAT), golfTeeDisplayLabel(tee.name),
                 static_cast<unsigned>(tee.yards[hole / 9][hole % 9]), tr(STR_GOLF_YARDS_UNIT)) < 0) {
      continue;
    }
    if (!any) {
      snprintf(output, size, "%s", segment);
      any = true;
      continue;
    }
    char joined[128];
    if (snprintf(joined, sizeof(joined), tr(STR_GOLF_TEE_YARDS_JOIN_FORMAT), output, segment) >= 0) {
      snprintf(output, size, "%s", joined);
    }
  }
  return any;
}

void GolfCourseMapBrowserActivity::drawHoleBand(const freeink::ui::Rect rect) const {
  const int padding = golfui::minValue(18, static_cast<int16_t>(rect.width / 8));
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int ascender = renderer.getFontAscenderSize(UI_10_FONT_ID);
  constexpr int digitHeight = golfui::SCORING_HOLE_DIGIT_MAX_HEIGHT;
  constexpr int labelTopMargin = 8;
  const int labelY = rect.y + labelTopMargin;
  // "HOLE" carries no descenders in this font/style (every glyph's ink bottom
  // sits exactly on the baseline), so its baseline is its ink bottom. Start the
  // big digit there to keep the label and digit reading as one tight group.
  const int digitY = labelY + ascender;
  renderer.drawText(UI_10_FONT_ID, rect.x + padding, labelY,
                    course.nineCount >= 2 ? course.nines[currentHole / 9].name : tr(STR_GOLF_HOLE), true,
                    EpdFontFamily::BOLD);
  golfDrawLargeNumber(renderer, rect.x + rect.width / 4, digitY, digitHeight,
                      static_cast<uint16_t>(course.nineCount >= 2 ? currentHole % 9 + 1 : currentHole + 1));

  const int right = rect.x + rect.width - padding;
  int y = rect.y + 4;
  const uint8_t par = currentHole < (9 * course.nineCount) ? course.nines[currentHole / 9].par[currentHole % 9] : 0;
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
  golfui::drawFooter(renderer, mappedInput,
                     {tr(STR_BACK), tr(STR_GOLF_TEE), tr(STR_GOLF_BUTTON_PREVIOUS), tr(STR_GOLF_BUTTON_NEXT)});
}

void GolfCourseMapBrowserActivity::renderHole() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, mappedInput, metrics.topPadding);
  const int16_t bandHeight = golfui::minValue(HOLE_BAND_HEIGHT, chrome.body.height);
  const freeink::ui::Rect holeBand{chrome.body.x, chrome.body.y, chrome.body.width, bandHeight};
  const freeink::ui::Rect imageBody{chrome.body.x, static_cast<int16_t>(chrome.body.y + bandHeight), chrome.body.width,
                                    static_cast<int16_t>(chrome.body.height - bandHeight)};

  renderer.clearScreen();
  golfui::drawHeader(renderer, chrome.header, courseName);
  if (holeCount) drawHoleBand(holeBand);
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
