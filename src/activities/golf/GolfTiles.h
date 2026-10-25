#pragma once
#include "GolfUiLayout.h"
#include "components/UiAppHost.h"
namespace golfui {
fui::Rect takeTileStrip(UiAppHost::UiScreen& screen, int16_t spacing);
void drawTile(UiAppHost::UiScreen& screen, fui::Rect tileRect, fui::ActionId action, uint8_t index, bool selected,
              fui::BitmapRef icon, const char* label);
void drawRegion(UiAppHost::UiScreen& screen, fui::Rect rect, fui::ActionId action, int index, bool selected,
                const char* name, unsigned count);
void drawFigure(fui::DrawTarget& target, fui::Rect rect, const char* value, fui::Color color);
void drawCourseCard(UiAppHost::UiScreen& screen, fui::Rect rect, fui::ActionId action, int index, bool selected,
                    const char* name, const char* detail, const char* figure, const char* unit, bool enabled = true);
void drawDetail(UiAppHost::UiScreen& screen, const char* label, const char* text, int spacing, uint8_t maxLines = 3);
}  // namespace golfui
