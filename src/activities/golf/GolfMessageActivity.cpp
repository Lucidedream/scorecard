#include "GolfMessageActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>

#include "GolfStrings.h"
#include "components/UITheme.h"
#include "fontIds.h"

void GolfMessageActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void GolfMessageActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();
  }
}

void GolfMessageActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, title);
  UITheme::drawCenteredText(renderer, Rect{24, metrics.headerHeight + 40, width - 48, height - 160}, UI_10_FONT_ID,
                            height / 2 - 20, message);
  const auto labels = mappedInput.mapLabels(GolfStrings::BACK, "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
