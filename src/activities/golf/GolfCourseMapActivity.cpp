#include "GolfCourseMapActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr size_t MAP_PATH_CAPACITY = 64;

}  // namespace

GolfCourseMapActivity::GolfCourseMapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const uint8_t holeIndex, const uint8_t holeCount, const char* courseName)
    : Activity("GolfCourseMap", renderer, mappedInput), holeIndex(holeIndex), holeCount(holeCount) {
  snprintf(this->courseName, sizeof(this->courseName), "%s", courseName != nullptr ? courseName : "");
}

void GolfCourseMapActivity::onEnter() {
  Activity::onEnter();
  // golfSlug's ASCII-only fallback is intentionally shared by non-ASCII course names (CONTRACTS-V2 §25).
  slugValid = golfSlug(courseName, courseSlug, sizeof(courseSlug));
  loadAndRender();
}

void GolfCourseMapActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) finish();
}

void GolfCourseMapActivity::drawChrome(const char* message) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  char title[32];
  snprintf(title, sizeof(title), tr(STR_GOLF_MAP_TITLE_FORMAT), static_cast<unsigned>(holeIndex + 1));
  golfui::drawHeader(renderer, layout.header, title);
  if (message != nullptr) {
    UITheme::drawCenteredWrappedText(
        renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height}, UI_10_FONT_ID, message, 3,
        true, EpdFontFamily::REGULAR, UITheme::TextVerticalAlignment::CENTER);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_FOOTER_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, "", "", "");
}

void GolfCourseMapActivity::renderMissing() const {
  renderer.clearScreen();
  drawChrome(tr(STR_GOLF_MAP_MISSING));
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void GolfCourseMapActivity::renderLoadFailed(const char* path) const {
  LOG_ERR("GOLF", "Course map load failed: %s", path != nullptr ? path : "invalid path");
  renderer.clearScreen();
  drawChrome(tr(STR_GOLF_MAP_LOAD_FAILED));
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

bool GolfCourseMapActivity::renderBmp(const char* path, const freeink::ui::Rect& body) const {
  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) return false;

  Bitmap bitmap(file, true);
  if (bitmap.parseHeaders() != BmpReaderError::Ok || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) return false;

  float scale = 1.0f;
  if (bitmap.getWidth() > body.width) scale = static_cast<float>(body.width) / bitmap.getWidth();
  if (bitmap.getHeight() * scale > body.height) scale = static_cast<float>(body.height) / bitmap.getHeight();
  const int width = static_cast<int>(bitmap.getWidth() * scale);
  const int height = static_cast<int>(bitmap.getHeight() * scale);
  const int x = body.x + (body.width - width) / 2;
  const int y = body.y + (body.height - height) / 2;
  renderer.drawBitmap(bitmap, x, y, body.width, body.height, 0, 0);
  drawChrome();

  if (!bitmap.hasGreyscale()) {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return true;
  }

  // The gray nudge LUT is calibrated against the charge state left by a HALF base refresh.
  renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);

  bitmap.rewindToData();
  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  renderer.drawBitmap(bitmap, x, y, body.width, body.height, 0, 0);
  renderer.copyGrayscaleLsbBuffers();

  bitmap.rewindToData();
  renderer.clearScreen(0x00);
  renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
  renderer.drawBitmap(bitmap, x, y, body.width, body.height, 0, 0);
  renderer.copyGrayscaleMsbBuffers();

  renderer.displayGrayBuffer();
  renderer.setRenderMode(GfxRenderer::BW);
  return true;
}

void GolfCourseMapActivity::loadAndRender() {
  if (!slugValid || holeIndex >= holeCount) {
    renderLoadFailed(nullptr);
    return;
  }

  char path[MAP_PATH_CAPACITY];
  const unsigned holeNumber = static_cast<unsigned>(holeIndex + 1);
  int length = snprintf(path, sizeof(path), "/golf/maps/%s/hole-%u.bmp", courseSlug, holeNumber);
  if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) {
    renderLoadFailed(nullptr);
    return;
  }

  if (!Storage.exists(path)) {
    renderMissing();
    return;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding);
  renderer.clearScreen();
  if (!renderBmp(path, layout.body)) {
    renderLoadFailed(path);
  }
}

#endif
