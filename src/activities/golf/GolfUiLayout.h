#pragma once

#include <FreeInkUICore.h>

#include <cstdint>

class GfxRenderer;

namespace golfui {

namespace fui = freeink::ui;

constexpr int minValue(const int a, const int b) { return a < b ? a : b; }
constexpr int maxValue(const int a, const int b) { return a > b ? a : b; }
constexpr int16_t clampValue(const int value, const int minimum, const int maximum) {
  return static_cast<int16_t>(value < minimum ? minimum : (value > maximum ? maximum : value));
}

constexpr fui::Rect intersect(const fui::Rect a, const fui::Rect b) {
  const int left = a.x > b.x ? a.x : b.x;
  const int top = a.y > b.y ? a.y : b.y;
  const int rightA = a.x + (a.width > 0 ? a.width : 0);
  const int rightB = b.x + (b.width > 0 ? b.width : 0);
  const int bottomA = a.y + (a.height > 0 ? a.height : 0);
  const int bottomB = b.y + (b.height > 0 ? b.height : 0);
  const int right = rightA < rightB ? rightA : rightB;
  const int bottom = bottomA < bottomB ? bottomA : bottomB;
  return fui::Rect{static_cast<int16_t>(left), static_cast<int16_t>(top),
                   static_cast<int16_t>(right > left ? right - left : 0),
                   static_cast<int16_t>(bottom > top ? bottom - top : 0)};
}

constexpr fui::Rect inset(const fui::Rect rect, const fui::Insets amount) {
  const int horizontal = amount.left + amount.right;
  const int vertical = amount.top + amount.bottom;
  const int width = rect.width > horizontal ? rect.width - horizontal : 0;
  const int height = rect.height > vertical ? rect.height - vertical : 0;
  const int left = rect.x + (amount.left > rect.width ? rect.width : maxValue(amount.left, 0));
  const int top = rect.y + (amount.top > rect.height ? rect.height : maxValue(amount.top, 0));
  return fui::Rect{static_cast<int16_t>(left), static_cast<int16_t>(top), static_cast<int16_t>(width),
                   static_cast<int16_t>(height)};
}

constexpr fui::Insets relativeInsets(const fui::Rect frame, const fui::Rect content) {
  const int frameRight = frame.x + frame.width;
  const int frameBottom = frame.y + frame.height;
  const int contentRight = content.x + content.width;
  const int contentBottom = content.y + content.height;
  return fui::Insets{static_cast<int16_t>(content.y > frame.y ? content.y - frame.y : 0),
                     static_cast<int16_t>(frameRight > contentRight ? frameRight - contentRight : 0),
                     static_cast<int16_t>(frameBottom > contentBottom ? frameBottom - contentBottom : 0),
                     static_cast<int16_t>(content.x > frame.x ? content.x - frame.x : 0)};
}

enum class HintEdge : uint8_t { Top, Right, Bottom, Left };

constexpr fui::Rect reserveHintEdge(const fui::Rect screen, const HintEdge edge, const int16_t amount) {
  const int16_t reserve = clampValue(amount, 0, edge == HintEdge::Top || edge == HintEdge::Bottom ? screen.height
                                                                                                  : screen.width);
  switch (edge) {
    case HintEdge::Top:
      return fui::Rect{screen.x, static_cast<int16_t>(screen.y + reserve), screen.width,
                       static_cast<int16_t>(screen.height - reserve)};
    case HintEdge::Right:
      return fui::Rect{screen.x, screen.y, static_cast<int16_t>(screen.width - reserve), screen.height};
    case HintEdge::Bottom:
      return fui::Rect{screen.x, screen.y, screen.width, static_cast<int16_t>(screen.height - reserve)};
    case HintEdge::Left:
    default:
      return fui::Rect{static_cast<int16_t>(screen.x + reserve), screen.y,
                       static_cast<int16_t>(screen.width - reserve), screen.height};
  }
}

struct ChromeLayout {
  fui::Rect safe{};
  fui::Rect header{};
  fui::Rect body{};
  fui::Insets contentMargins{};
};

// frameSafe is the FreeInkUI/bezel-safe frame; hintSafe is UITheme's physical
// button-hint-safe frame. Margins are deliberately relative to frameSafe,
// because Screen::setContentMargin() already starts at frame.safeRect().
constexpr ChromeLayout makeChromeLayout(const fui::Rect frameSafe, const fui::Rect hintSafe, const int topPadding,
                                        const int headerHeight, const fui::Insets bodyPadding = {}) {
  ChromeLayout result{};
  result.safe = intersect(frameSafe, hintSafe);
  const int16_t topPad = clampValue(topPadding, 0, result.safe.height);
  const int16_t availableAfterPad = static_cast<int16_t>(result.safe.height - topPad);
  const int16_t header = clampValue(headerHeight, 0, availableAfterPad);
  result.header = fui::Rect{result.safe.x, static_cast<int16_t>(result.safe.y + topPad), result.safe.width, header};
  const fui::Rect rawBody{result.safe.x, static_cast<int16_t>(result.header.y + result.header.height),
                          result.safe.width,
                          static_cast<int16_t>(result.safe.y + result.safe.height - result.header.y -
                                               result.header.height)};
  result.body = inset(rawBody, bodyPadding);
  result.contentMargins = relativeInsets(frameSafe, result.body);
  return result;
}

// Runtime adapters obtain both sources of hardware clearance. The first form
// uses a FreeInkUI frame-safe rect; the second derives the bezel-safe rect
// directly from GfxRenderer for legacy/raw drawing screens.
ChromeLayout chromeLayout(const GfxRenderer& renderer, fui::Rect frameSafe, int topPadding, int headerHeight,
                          fui::Insets bodyPadding = {});
ChromeLayout chromeLayout(const GfxRenderer& renderer, int topPadding, int headerHeight,
                          fui::Insets bodyPadding = {});

constexpr fui::Rect evenRow(const fui::Rect rect, const uint8_t count, const uint8_t index) {
  if (count == 0 || index >= count) return {};
  const int top = rect.y + (static_cast<int32_t>(rect.height) * index) / count;
  const int bottom = rect.y + (static_cast<int32_t>(rect.height) * (index + 1)) / count;
  return fui::Rect{rect.x, static_cast<int16_t>(top), rect.width, static_cast<int16_t>(bottom - top)};
}

struct CardLayout {
  fui::Rect tabs{};
  fui::Rect table{};
  uint8_t rowCount = 0;
  int16_t minimumRowHeight = 0;
  bool valid = false;
};

constexpr CardLayout makeCardLayout(const fui::Rect content, const bool hasTabs, const int tabHeight,
                                    const int tabGap, const uint8_t playerCount, const bool hasPar,
                                    const int fontMinimumRowHeight) {
  CardLayout result{};
  result.tabs = fui::Rect{content.x, content.y, content.width, 0};
  const uint8_t players = playerCount > 4 ? 4 : playerCount;
  result.rowCount = static_cast<uint8_t>(1 + (hasPar ? 1 : 0) + players);
  result.minimumRowHeight = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  int cursor = content.y;
  int remaining = content.height;
  if (hasTabs) {
    const int requiredTable = result.rowCount * result.minimumRowHeight;
    const int gap = clampValue(tabGap, 0, remaining);
    const int maxTab = remaining > requiredTable + gap ? remaining - requiredTable - gap : 0;
    const int height = clampValue(tabHeight, 0, maxTab);
    result.tabs = fui::Rect{content.x, static_cast<int16_t>(cursor), content.width, static_cast<int16_t>(height)};
    cursor += height;
    remaining -= height;
    const int appliedGap = remaining > requiredTable ? minValue(static_cast<int16_t>(gap),
                                                                 static_cast<int16_t>(remaining - requiredTable))
                                                     : 0;
    cursor += appliedGap;
    remaining -= appliedGap;
  }
  result.table = fui::Rect{content.x, static_cast<int16_t>(cursor), content.width,
                           static_cast<int16_t>(remaining > 0 ? remaining : 0)};
  result.valid = result.rowCount > 0 && result.table.width > 0 &&
                 result.table.height >= result.rowCount * result.minimumRowHeight;
  return result;
}

struct MenuInfoLayout {
  fui::Rect menu{};
  fui::Rect info{};
  int16_t rowHeight = 0;
  bool valid = false;
};

constexpr MenuInfoLayout makeMenuInfoLayout(const fui::Rect body, const uint8_t rowCount,
                                            const int fontMinimumRowHeight, const int infoMinimumHeight) {
  MenuInfoLayout result{};
  if (rowCount == 0) return result;
  const int rowMinimum = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  const int infoMinimum = maxValue(static_cast<int16_t>(infoMinimumHeight), 0);
  int infoHeight = body.height / 5;
  if (infoHeight < infoMinimum) infoHeight = infoMinimum;
  if (infoHeight > 90) infoHeight = 90;
  const int maximumInfo = body.height - rowCount * rowMinimum;
  if (infoHeight > maximumInfo) infoHeight = maximumInfo > 0 ? maximumInfo : 0;
  const int menuHeight = body.height - infoHeight;
  result.menu = fui::Rect{body.x, body.y, body.width, static_cast<int16_t>(menuHeight)};
  result.info = fui::Rect{body.x, static_cast<int16_t>(body.y + menuHeight), body.width,
                          static_cast<int16_t>(infoHeight)};
  result.rowHeight = static_cast<int16_t>(menuHeight / rowCount);
  result.valid = body.width > 0 && infoHeight >= infoMinimum && result.rowHeight >= rowMinimum;
  return result;
}

struct ScoringLayout {
  fui::Rect safe{};
  fui::Rect header{};
  fui::Rect hole{};
  fui::Rect counters[3]{};
  fui::Rect penalty{};
  fui::Rect totals{};
  fui::Rect nineStrip{};
  bool valid = false;
};

constexpr ScoringLayout makeScoringLayout(const fui::Rect safe, const int topPadding, const int preferredHeaderHeight,
                                          const uint8_t focusedCounter, const bool hasPenalty,
                                          const int fontMinimumRowHeight) {
  ScoringLayout result{};
  result.safe = safe;
  const int fontMin = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  const int topPad = clampValue(topPadding, 0, safe.height);
  const bool compact = safe.width >= safe.height;
  const int headerFloor = fontMin + 8 > 36 ? fontMin + 8 : 36;
  const int headerCap = compact ? (fontMin + 28 > 56 ? fontMin + 28 : 56) : preferredHeaderHeight;
  const int header = clampValue(preferredHeaderHeight, headerFloor, headerCap > headerFloor ? headerCap : headerFloor);
  const int hole = fontMin + 28 > 52 ? fontMin + 28 : 52;
  const int counter = fontMin + 28 > 52 ? fontMin + 28 : 52;
  const int penalty = hasPenalty ? (fontMin + 10 > 32 ? fontMin + 10 : 32) : 0;
  const int totals = fontMin * 2 + 8 > 48 ? fontMin * 2 + 8 : 48;
  const int nine = fontMin * 2 + 4 > 42 ? fontMin * 2 + 4 : 42;
  const int minima[8] = {header, hole, counter, counter, counter, penalty, totals, nine};
  int weights[8] = {0, 2, 4, 4, 4, hasPenalty ? 1 : 0, 2, 2};
  if (focusedCounter < 3) weights[focusedCounter + 2] = 7;
  int minimumTotal = 0;
  int weightTotal = 0;
  for (uint8_t index = 0; index < 8; ++index) {
    minimumTotal += minima[index];
    weightTotal += weights[index];
  }
  const int available = safe.height > topPad ? safe.height - topPad : 0;
  const bool fitsMinimum = available >= minimumTotal;
  const int extra = fitsMinimum ? available - minimumTotal : 0;
  int cursor = safe.y + topPad;
  int cumulativeWeight = 0;
  int assignedExtra = 0;
  fui::Rect* bands[8] = {&result.header,      &result.hole,   &result.counters[0], &result.counters[1],
                         &result.counters[2], &result.penalty, &result.totals,      &result.nineStrip};
  for (uint8_t index = 0; index < 8; ++index) {
    int height = 0;
    if (fitsMinimum) {
      cumulativeWeight += weights[index];
      const int nextExtra = weightTotal > 0 ? extra * cumulativeWeight / weightTotal : 0;
      height = minima[index] + nextExtra - assignedExtra;
      assignedExtra = nextExtra;
    } else if (minimumTotal > 0) {
      height = available * minima[index] / minimumTotal;
    }
    if (index == 7) height = safe.y + safe.height - cursor;
    *bands[index] = fui::Rect{safe.x, static_cast<int16_t>(cursor), safe.width,
                              static_cast<int16_t>(height > 0 ? height : 0)};
    cursor += height;
  }
  result.valid = safe.width > 0 && fitsMinimum;
  return result;
}

struct HoleReviewLayout {
  fui::Rect hole{};
  fui::Rect score{};
  fui::Rect details[3]{};
  fui::Rect penalty{};
  bool valid = false;
};

constexpr HoleReviewLayout makeHoleReviewLayout(const fui::Rect body, const bool hasPenalty,
                                                const int fontMinimumRowHeight) {
  HoleReviewLayout result{};
  const int fontMin = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  const int minima[6] = {fontMin * 2 + 12 > 60 ? fontMin * 2 + 12 : 60,
                         fontMin * 2 + 16 > 64 ? fontMin * 2 + 16 : 64,
                         fontMin + 18 > 42 ? fontMin + 18 : 42,
                         fontMin + 18 > 42 ? fontMin + 18 : 42,
                         fontMin + 18 > 42 ? fontMin + 18 : 42,
                         hasPenalty ? (fontMin + 10 > 32 ? fontMin + 10 : 32) : 0};
  constexpr int weights[6] = {2, 3, 1, 1, 1, 1};
  int minimumTotal = 0;
  int weightTotal = 0;
  for (uint8_t index = 0; index < 6; ++index) {
    minimumTotal += minima[index];
    if (minima[index] > 0) weightTotal += weights[index];
  }
  const bool fitsMinimum = body.height >= minimumTotal;
  const int extra = fitsMinimum ? body.height - minimumTotal : 0;
  int cursor = body.y;
  int cumulativeWeight = 0;
  int assignedExtra = 0;
  fui::Rect* bands[6] = {&result.hole,       &result.score,      &result.details[0],
                         &result.details[1], &result.details[2], &result.penalty};
  for (uint8_t index = 0; index < 6; ++index) {
    int height = 0;
    if (fitsMinimum) {
      if (minima[index] > 0) cumulativeWeight += weights[index];
      const int nextExtra = weightTotal > 0 ? extra * cumulativeWeight / weightTotal : 0;
      height = minima[index] + nextExtra - assignedExtra;
      assignedExtra = nextExtra;
    } else if (minimumTotal > 0) {
      height = body.height * minima[index] / minimumTotal;
    }
    if (index == 5) height = body.y + body.height - cursor;
    *bands[index] = fui::Rect{body.x, static_cast<int16_t>(cursor), body.width,
                              static_cast<int16_t>(height > 0 ? height : 0)};
    cursor += height;
  }
  result.valid = body.width > 0 && fitsMinimum;
  return result;
}

struct StatisticsLayout {
  static constexpr uint8_t ROW_COUNT = 11;
  fui::Rect rows[ROW_COUNT]{};
  bool section[ROW_COUNT]{};
  int16_t minimumSectionHeight = 0;
  int16_t minimumStatHeight = 0;
  bool valid = false;
};

constexpr StatisticsLayout makeStatisticsLayout(const fui::Rect body, const int fontMinimumRowHeight) {
  StatisticsLayout result{};
  constexpr bool sections[StatisticsLayout::ROW_COUNT] = {true,  false, false, false, false, true,
                                                           false, false, true,  false, false};
  const int fontMin = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  result.minimumSectionHeight = static_cast<int16_t>(fontMin > 24 ? fontMin : 24);
  result.minimumStatHeight = static_cast<int16_t>(fontMin + 6 > 32 ? fontMin + 6 : 32);
  int minimumTotal = 0;
  int weightTotal = 0;
  for (uint8_t index = 0; index < StatisticsLayout::ROW_COUNT; ++index) {
    result.section[index] = sections[index];
    minimumTotal += sections[index] ? result.minimumSectionHeight : result.minimumStatHeight;
    weightTotal += sections[index] ? 1 : 2;
  }
  const bool fitsMinimum = body.height >= minimumTotal;
  const int extra = fitsMinimum ? body.height - minimumTotal : 0;
  int cursor = body.y;
  int cumulativeWeight = 0;
  int assignedExtra = 0;
  for (uint8_t index = 0; index < StatisticsLayout::ROW_COUNT; ++index) {
    const int minimum = sections[index] ? result.minimumSectionHeight : result.minimumStatHeight;
    const int weight = sections[index] ? 1 : 2;
    int height = 0;
    if (fitsMinimum) {
      cumulativeWeight += weight;
      const int nextExtra = extra * cumulativeWeight / weightTotal;
      height = minimum + nextExtra - assignedExtra;
      assignedExtra = nextExtra;
    } else if (minimumTotal > 0) {
      height = body.height * minimum / minimumTotal;
    }
    if (index + 1 == StatisticsLayout::ROW_COUNT) height = body.y + body.height - cursor;
    result.rows[index] = fui::Rect{body.x, static_cast<int16_t>(cursor), body.width,
                                   static_cast<int16_t>(height > 0 ? height : 0)};
    cursor += height;
  }
  result.valid = body.width > 0 && fitsMinimum;
  return result;
}

}  // namespace golfui
