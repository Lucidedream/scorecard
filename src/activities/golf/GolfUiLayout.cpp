#include "GolfUiLayout.h"

#if defined(CROSSPOINT_GOLF)

#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalPowerManager.h>

#include "CrossPointSettings.h"
#include "components/UIScale.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

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
  return inset(screen, fui::Insets{static_cast<int16_t>(top), static_cast<int16_t>(right), static_cast<int16_t>(bottom),
                                   static_cast<int16_t>(left)});
}

}  // namespace

ChromeLayout chromeLayout(const GfxRenderer& renderer, const fui::Rect frameSafe, const int topPadding,
                          const fui::Insets bodyPadding) {
  const fui::Rect hardwareSafe = intersect(bezelSafeRect(renderer), hintSafeRect(renderer));
  return makeChromeLayout(frameSafe, hardwareSafe, topPadding, GOLF_HEADER_HEIGHT, bodyPadding);
}

ChromeLayout chromeLayout(const GfxRenderer& renderer, const int topPadding, const fui::Insets bodyPadding) {
  return makeChromeLayout(bezelSafeRect(renderer), hintSafeRect(renderer), topPadding, GOLF_HEADER_HEIGHT, bodyPadding);
}

void drawHeader(const GfxRenderer& renderer, const fui::Rect rect, const char* title, const char* rightLabel) {
  constexpr int16_t batteryNubWidth = 2;
  constexpr int16_t batteryPercentSpacing = 4;

  const auto spec = uiScaleSpec();
  fui::GfxRendererFrame<1> ui(renderer, spec.smallFontId, spec.bodyFontId, spec.titleFontId);
  const fui::ThemeTokens& tokens = refreshSharedUiThemeTokens(ui.target);
  ui.target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  const ThemeMetrics& metrics = UITheme::getInstance().getMetrics();

  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  const uint16_t percentage = powerManager.getBatteryPercentage();
  char percentText[8];
  snprintf(percentText, sizeof(percentText), "%u%%", static_cast<unsigned>(percentage));
  int16_t batteryReserve = static_cast<int16_t>(metrics.batteryWidth + batteryNubWidth);
  if (showBatteryPercentage) {
    batteryReserve = static_cast<int16_t>(
        batteryReserve + batteryPercentSpacing +
        ui.target.measureText(fui::GfxRendererTarget::FONT_SMALL, percentText, tokens.smallText).width);
  }

  const bool batteryLeft = metrics.headerBatterySide == 1;
  const bool batteryDetached = metrics.headerBatteryDetached;
  const bool manualRightLabel = rightLabel != nullptr && !batteryDetached && !batteryLeft;
  fui::Rect content = rect.inset(fui::Insets{0, tokens.headerSidePadding, 0, tokens.headerSidePadding});
  int16_t titleOffsetY = 0;
  if (batteryDetached) {
    const int titleLineHeight = ui.target.lineHeight(fui::GfxRendererTarget::FONT_TITLE);
    const int titleTop = static_cast<int>(rect.height) - tokens.headerUnderline - tokens.spaceMd - titleLineHeight;
    titleOffsetY = static_cast<int16_t>(titleTop - (static_cast<int>(rect.height) - titleLineHeight) / 2);
  } else {
    const int16_t reserve = static_cast<int16_t>(batteryReserve + tokens.spaceMd);
    if (batteryLeft) {
      content.x = static_cast<int16_t>(content.x + reserve);
      content.width = static_cast<int16_t>(content.width - reserve);
    } else {
      content.width = static_cast<int16_t>(content.width - reserve);
    }
  }

  fui::TextStyle titleText = tokens.titleText;
  titleText.align = tokens.headerTitleAlign;
  titleText.bold = false;
  titleText.inverted = false;
  fui::Rect titleRect{content.x, static_cast<int16_t>(content.y + titleOffsetY), content.width, content.height};
  if (rightLabel != nullptr && !manualRightLabel) {
    const fui::Size rightSize = ui.target.measureText(fui::GfxRendererTarget::FONT_SMALL, rightLabel, tokens.smallText);
    const int16_t titleLineHeight = ui.target.lineHeight(fui::GfxRendererTarget::FONT_TITLE);
    const int16_t titleTop = static_cast<int16_t>(content.y + titleOffsetY + (content.height - titleLineHeight) / 2);
    const fui::Rect rightRect{static_cast<int16_t>(content.right() - rightSize.width),
                              static_cast<int16_t>(titleTop + titleLineHeight - rightSize.height), rightSize.width,
                              rightSize.height};
    ui.target.text(rightRect, rightLabel, tokens.smallText);
    titleRect.width = static_cast<int16_t>(titleRect.width - rightSize.width - 6);
  }
  if (title != nullptr) ui.target.text(titleRect, title, titleText);

  const uint8_t borderWidth = tokens.headerUnderline > 0 ? tokens.headerUnderline : 1;
  ui.target.line(fui::Point{rect.x, static_cast<int16_t>(rect.bottom() - borderWidth)},
                 fui::Point{static_cast<int16_t>(rect.right() - 1), static_cast<int16_t>(rect.bottom() - borderWidth)},
                 borderWidth, fui::Paint::solid(fui::Color::Black));

  fui::BatteryIndicatorProps battery;
  battery.percent = static_cast<uint8_t>(percentage > 100 ? 100 : percentage);
  battery.charging = gpio.isUsbConnected();
  battery.label = showBatteryPercentage ? percentText : nullptr;
  battery.text = tokens.smallText;
  battery.glyphWidth = static_cast<int16_t>(metrics.batteryWidth);
  battery.glyphHeight = static_cast<int16_t>(metrics.batteryHeight);
  battery.gap = batteryPercentSpacing;
  const int16_t batteryEdgeInset = batteryDetached ? 12 : tokens.headerSidePadding;
  const int16_t batteryX = batteryLeft ? static_cast<int16_t>(rect.x + batteryEdgeInset)
                                       : static_cast<int16_t>(rect.right() - batteryEdgeInset - batteryReserve);
  fui::batteryIndicator(
      ui.frame, fui::Rect{batteryX, rect.y, batteryReserve, static_cast<int16_t>(metrics.batteryBarHeight)}, battery);

  if (manualRightLabel) {
    const fui::Size labelSize = ui.target.measureText(fui::GfxRendererTarget::FONT_SMALL, rightLabel, tokens.smallText);
    const int16_t labelHeight = ui.target.lineHeight(fui::GfxRendererTarget::FONT_SMALL);
    const fui::Rect labelRect{static_cast<int16_t>(rect.right() - tokens.headerSidePadding - labelSize.width),
                              static_cast<int16_t>(rect.bottom() - borderWidth - tokens.spaceSm - labelHeight),
                              labelSize.width, labelHeight};
    ui.target.text(labelRect, rightLabel, tokens.smallText);
  }
}

}  // namespace golfui

#endif
