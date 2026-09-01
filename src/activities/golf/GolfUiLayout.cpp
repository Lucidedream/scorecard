#include "GolfUiLayout.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>

#include "components/UITheme.h"

namespace golfui {

namespace {

fui::Rect hintSafeRect(const GfxRenderer& renderer) {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  return fui::Rect{static_cast<int16_t>(safe.x), static_cast<int16_t>(safe.y), static_cast<int16_t>(safe.width),
                   static_cast<int16_t>(safe.height)};
}

fui::Rect bezelSafeRect(const GfxRenderer& renderer) {
  int top = 0;
  int right = 0;
  int bottom = 0;
  int left = 0;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const fui::Rect screen{0, 0, static_cast<int16_t>(renderer.getScreenWidth()),
                         static_cast<int16_t>(renderer.getScreenHeight())};
  return inset(screen, fui::Insets{static_cast<int16_t>(top), static_cast<int16_t>(right),
                                   static_cast<int16_t>(bottom), static_cast<int16_t>(left)});
}

}  // namespace

ChromeLayout chromeLayout(const GfxRenderer& renderer, const fui::Rect frameSafe, const int topPadding,
                          const int headerHeight, const fui::Insets bodyPadding) {
  const fui::Rect hardwareSafe = intersect(bezelSafeRect(renderer), hintSafeRect(renderer));
  return makeChromeLayout(frameSafe, hardwareSafe, topPadding, headerHeight, bodyPadding);
}

ChromeLayout chromeLayout(const GfxRenderer& renderer, const int topPadding, const int headerHeight,
                          const fui::Insets bodyPadding) {
  return makeChromeLayout(bezelSafeRect(renderer), hintSafeRect(renderer), topPadding, headerHeight, bodyPadding);
}

}  // namespace golfui

#endif
