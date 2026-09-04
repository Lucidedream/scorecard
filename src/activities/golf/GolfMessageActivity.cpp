#include "GolfMessageActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <GfxRenderer.h>
#include <I18n.h>

#include "GolfUiLayout.h"
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
  const auto layout = golfui::chromeLayout(renderer, metrics.topPadding, freeink::ui::Insets{12, 24, 12, 24});
  renderer.clearScreen();
  golfui::drawHeader(renderer, layout.header, title);
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  UITheme::drawCenteredText(renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height},
                            UI_10_FONT_ID, layout.body.y + (layout.body.height - lineHeight) / 2, message);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

#endif
