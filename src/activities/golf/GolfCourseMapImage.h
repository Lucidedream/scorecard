#pragma once

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUICore.h>

#include <cstdint>

class GfxRenderer;

enum class GolfCourseMapImageResult : uint8_t { Rendered, Missing, LoadFailed };

// Resolves /golf/maps/<courseSlug>/hole-<holeNumber>.bmp and, on Rendered, draws it into
// `body` and drives the device's grayscale display pipeline (base pass, LSB/MSB nudge
// passes, final push) itself. Draws nothing outside `body` and performs no display push on
// Missing/LoadFailed -- callers own their own chrome and message text for those cases, and
// must draw any chrome that should appear under a successfully rendered image *before*
// calling this function, since the grayscale base pass captures whatever is already in the
// framebuffer.
GolfCourseMapImageResult golfRenderCourseMapImage(GfxRenderer& renderer, const char* courseSlug, uint8_t holeNumber,
                                                  const freeink::ui::Rect& body);

#endif
