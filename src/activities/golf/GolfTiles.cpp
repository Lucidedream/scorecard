#include "GolfTiles.h"
#if defined(CROSSPOINT_GOLF)
#include <I18n.h>

#include <cstdio>
#include <cstring>
namespace golfui {
void drawTile(UiAppHost::UiScreen& screen, fui::Rect tileRect, fui::ActionId action, uint8_t index, bool selected,
              fui::BitmapRef icon, const char* label, const char* subtitle) {
  const auto geometry = tileGeometry(tileRect);
  const bool tileSelected = selected;

  fui::ButtonProps tile{};
  tile.action = action;
  tile.value = index;
  tile.inputMask = fui::InputTouch;
  tile.state = tileSelected ? fui::StateSelected : fui::StateNormal;
  screen.button(tile, tileRect);  // box, selection fill and hit target only

  const fui::Color ink = tileSelected ? fui::Color::White : fui::Color::Black;
  if (icon) screen.target().bitmap(geometry.icon, icon, fui::BitmapMode::Center, fui::Paint::solid(ink));
  fui::TextStyle tileLabel = screen.theme().bodyText;
  tileLabel.bold = true;
  tileLabel.align = fui::TextAlign::Center;
  tileLabel.color = ink;
  tileLabel.maxLines = 2;
  screen.target().text(geometry.label, label, tileLabel);
  if (subtitle) {
    auto style = screen.theme().smallText;
    style.align = fui::TextAlign::Center;
    style.color = ink;
    screen.target().text({tileRect.x, static_cast<int16_t>(tileRect.bottom() - 30), tileRect.width, 26}, subtitle,
                         style);
  }
}
void drawFigure(fui::DrawTarget& target, fui::Rect rect, const char* value, fui::Color color) {
  static constexpr uint8_t MASKS[] = {0x3f, 6, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 7, 0x7f, 0x6f};
  const int height = minValue(rect.height, 38);
  const int width = height * 11 / 20;
  const int thickness = maxValue(height / 10, 1);
  const int advance = width + thickness;
  int x = rect.x;
  for (const char* c = value; *c && x + width <= rect.right(); ++c) {
    if (*c == '.') {
      target.fill({static_cast<int16_t>(x), static_cast<int16_t>(rect.y + height - thickness),
                   static_cast<int16_t>(thickness), static_cast<int16_t>(thickness)},
                  fui::Paint::solid(color));
      x += thickness * 2;
      continue;
    }
    if (*c < '0' || *c > '9') {
      target.fill({static_cast<int16_t>(x), static_cast<int16_t>(rect.y + height / 2), static_cast<int16_t>(width),
                   static_cast<int16_t>(thickness)},
                  fui::Paint::solid(color));
      return;
    }
    const uint8_t mask = MASKS[*c - '0'];
    for (uint8_t seg = 0; seg < 7; ++seg) {
      if (!(mask & (1 << seg))) continue;
      const bool horizontal = seg == 0 || seg == 3 || seg == 6;
      const int sx = horizontal ? x + thickness : (seg == 1 || seg == 2 ? x + width - thickness : x);
      const int sy = horizontal ? rect.y + (seg == 0   ? 0
                                            : seg == 3 ? height - thickness
                                                       : height / 2 - thickness / 2)
                                : rect.y + (seg == 1 || seg == 5 ? thickness : height / 2);
      target.fill({static_cast<int16_t>(sx), static_cast<int16_t>(sy),
                   static_cast<int16_t>(horizontal ? width - 2 * thickness : thickness),
                   static_cast<int16_t>(horizontal ? thickness : height / 2 - thickness)},
                  fui::Paint::solid(color));
    }
    x += advance;
  }
}
void drawRegion(UiAppHost::UiScreen& screen, fui::Rect rect, fui::ActionId action, int index, bool selected,
                const char* name, unsigned count) {
  fui::ButtonProps button{};
  button.action = action;
  button.value = index;
  button.inputMask = fui::InputTouch;
  button.state = selected ? fui::StateSelected : fui::StateNormal;
  screen.button(button, rect);
  auto title = screen.theme().bodyText;
  title.bold = true;
  title.maxLines = 1;
  title.color = selected ? fui::Color::White : fui::Color::Black;
  screen.target().text(
      {static_cast<int16_t>(rect.x + 14), static_cast<int16_t>(rect.y + 8), static_cast<int16_t>(rect.width - 28), 28},
      name, title);
  char number[8];
  snprintf(number, sizeof(number), "%u", count);
  const int16_t height = minValue(32, maxValue(rect.height - 42, 12));
  drawFigure(screen.target(),
             {static_cast<int16_t>(rect.x + 14), static_cast<int16_t>(rect.bottom() - height - 10), 68, height}, number,
             title.color);
  auto label = screen.theme().smallText;
  label.color = title.color;
  screen.target().text({static_cast<int16_t>(rect.x + 88), static_cast<int16_t>(rect.bottom() - 34),
                        static_cast<int16_t>(rect.width - 100), 24},
                       tr(STR_GOLF_COURSES_LABEL), label);
}
void drawCourseCard(UiAppHost::UiScreen& screen, fui::Rect rect, fui::ActionId action, int index, bool selected,
                    const char* name, const char* detail, const char* figure, const char* unit, bool enabled) {
  fui::ButtonProps button{};
  button.action = action;
  button.value = index;
  button.inputMask = enabled ? fui::InputTouch : fui::InputNone;
  button.state = selected ? fui::StateSelected : fui::StateNormal;
  screen.button(button, rect);
  auto title = screen.theme().titleText;
  title.bold = true;
  title.maxLines = 1;
  title.color = selected ? fui::Color::White : fui::Color::Black;
  auto small = screen.theme().smallText;
  small.maxLines = 1;
  small.color = title.color;
  const int16_t textWidth = maxValue(rect.width - 130, 1);
  screen.target().text({static_cast<int16_t>(rect.x + 18), static_cast<int16_t>(rect.y + 16), textWidth, 38}, name,
                       title);
  screen.target().text({static_cast<int16_t>(rect.x + 18), static_cast<int16_t>(rect.y + 60), textWidth, 30}, detail,
                       small);
  title.align = fui::TextAlign::Right;
  small.align = fui::TextAlign::Right;
  drawFigure(screen.target(), {static_cast<int16_t>(rect.right() - 100), static_cast<int16_t>(rect.y + 12), 82, 38},
             figure, title.color);
  screen.target().text({static_cast<int16_t>(rect.right() - 112), static_cast<int16_t>(rect.y + 60), 94, 30}, unit,
                       small);
}
void drawDetail(UiAppHost::UiScreen& screen, const char* label, const char* text, int spacing, uint8_t maxLines) {
  fui::TextStyle title = screen.theme().titleText;
  title.bold = true;
  fui::TextStyle body = screen.theme().bodyText;
  body.maxLines = maxLines;
  const int16_t titleHeight = screen.target().lineHeight(title.font);
  const int16_t bodyWidth = static_cast<int16_t>(screen.body().width - 40);
  const int16_t bodyHeight = fui::measureWrappedText(screen.target(), text, body, bodyWidth).height;
  const auto geometry = detailGeometry(screen.body(), titleHeight, bodyHeight, spacing);
  screen.takeTop(geometry.box.height);
  screen.target().stroke(geometry.box, fui::Paint::solid(fui::Color::Black), 2, screen.theme().controlRadius);
  screen.target().text(geometry.title, label, title);
  screen.target().text(geometry.body, text, body);
}
}  // namespace golfui
#endif
