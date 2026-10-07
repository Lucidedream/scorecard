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
  golfui::FooterInput input(mappedInput, {tr(STR_BACK), "", "", ""});
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout =
      golfui::chromeLayout(renderer, mappedInput, metrics.topPadding, freeink::ui::Insets{12, 24, 12, 24});
  if (input.wasReleased(golfui::FooterAction::Back) || input.wasReleased(golfui::FooterAction::Confirm) ||
      input.tapIn(layout.body)) {
    finish();
  }
}

void GolfMessageActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto layout =
      golfui::chromeLayout(renderer, mappedInput, metrics.topPadding, freeink::ui::Insets{12, 24, 12, 24});
  renderer.clearScreen();
  golfui::drawHeader(renderer, layout.header, title);
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  UITheme::drawCenteredText(renderer, Rect{layout.body.x, layout.body.y, layout.body.width, layout.body.height},
                            UI_10_FONT_ID, layout.body.y + (layout.body.height - lineHeight) / 2, message);
  golfui::drawFooter(renderer, mappedInput, {tr(STR_BACK), "", "", ""});
  renderer.displayBuffer();
}

#endif
