#include "GolfCourseMapImage.h"

#if defined(CROSSPOINT_GOLF)

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

namespace {

constexpr size_t MAP_PATH_CAPACITY = 64;

}  // namespace

GolfCourseMapImageResult golfRenderCourseMapImage(GfxRenderer& renderer, const char* courseSlug,
                                                  const uint8_t holeNumber, const freeink::ui::Rect& body) {
  char path[MAP_PATH_CAPACITY];
  const int length =
      snprintf(path, sizeof(path), "/golf/maps/%s/hole-%u.bmp", courseSlug != nullptr ? courseSlug : "",
               static_cast<unsigned>(holeNumber));
  if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) {
    LOG_ERR("GOLF", "Course map load failed: path too long");
    return GolfCourseMapImageResult::LoadFailed;
  }

  if (!Storage.exists(path)) return GolfCourseMapImageResult::Missing;

  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) {
    LOG_ERR("GOLF", "Course map load failed: %s", path);
    return GolfCourseMapImageResult::LoadFailed;
  }

  Bitmap bitmap(file, true);
  if (bitmap.parseHeaders() != BmpReaderError::Ok || bitmap.getWidth() <= 0 || bitmap.getHeight() <= 0) {
    LOG_ERR("GOLF", "Course map load failed: %s", path);
    return GolfCourseMapImageResult::LoadFailed;
  }

  float scale = 1.0f;
  if (bitmap.getWidth() > body.width) scale = static_cast<float>(body.width) / bitmap.getWidth();
  if (bitmap.getHeight() * scale > body.height) scale = static_cast<float>(body.height) / bitmap.getHeight();
  const int width = static_cast<int>(bitmap.getWidth() * scale);
  const int height = static_cast<int>(bitmap.getHeight() * scale);
  const int x = body.x + (body.width - width) / 2;
  const int y = body.y + (body.height - height) / 2;
  renderer.drawBitmap(bitmap, x, y, body.width, body.height, 0, 0);

  if (!bitmap.hasGreyscale()) {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return GolfCourseMapImageResult::Rendered;
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
  return GolfCourseMapImageResult::Rendered;
}

#endif
