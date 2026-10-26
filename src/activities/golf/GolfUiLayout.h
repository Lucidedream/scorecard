#pragma once

#include <FreeInkUICore.h>

#include <cstdint>

class GfxRenderer;
class MappedInputManager;

namespace golfui {

namespace fui = freeink::ui;

inline constexpr int GOLF_HEADER_HEIGHT = 46;
inline constexpr int GOLF_TOUCH_FOOTER_HEIGHT = 56;
inline constexpr int SCORING_HOLE_DIGIT_MAX_HEIGHT = 48;
inline constexpr int SCORING_HOLE_DIGIT_RULE_GAP = 12;

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
  const int16_t reserve =
      clampValue(amount, 0, edge == HintEdge::Top || edge == HintEdge::Bottom ? screen.height : screen.width);
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
      return fui::Rect{static_cast<int16_t>(screen.x + reserve), screen.y, static_cast<int16_t>(screen.width - reserve),
                       screen.height};
  }
}

inline constexpr int16_t GOLF_TILE_STRIP_HEIGHT = 112;
constexpr fui::Rect tileStripRect(const fui::Rect body) {
  return {body.x, body.y, body.width, static_cast<int16_t>(minValue(body.height, GOLF_TILE_STRIP_HEIGHT))};
}
constexpr fui::Rect tileCell(const fui::Rect strip, const uint8_t count, const uint8_t index) {
  if (!count || index >= count) return {};
  const int left = strip.x + static_cast<int32_t>(strip.width) * index / count;
  const int right = strip.x + static_cast<int32_t>(strip.width) * (index + 1) / count;
  return {static_cast<int16_t>(left), strip.y, static_cast<int16_t>(right - left), strip.height};
}
struct TileGeometry {
  fui::Rect icon{};
  fui::Rect label{};
};
constexpr TileGeometry tileGeometry(const fui::Rect rect) {
  return {{static_cast<int16_t>(rect.x + (rect.width - 32) / 2), static_cast<int16_t>(rect.y + 16), 32, 32},
          {rect.x, static_cast<int16_t>(rect.y + 54), rect.width, static_cast<int16_t>(rect.height - 58)}};
}
struct DetailGeometry {
  fui::Rect box{};
  fui::Rect title{};
  fui::Rect body{};
};
constexpr DetailGeometry detailGeometry(const fui::Rect available, const int titleHeight, const int bodyHeight,
                                        const int spacing) {
  const fui::Rect box{available.x, available.y, available.width,
                      static_cast<int16_t>(minValue(available.height, 36 + titleHeight + spacing + bodyHeight))};
  return {box,
          {static_cast<int16_t>(box.x + 20), static_cast<int16_t>(box.y + 18), static_cast<int16_t>(box.width - 40),
           static_cast<int16_t>(titleHeight)},
          {static_cast<int16_t>(box.x + 20), static_cast<int16_t>(box.y + 18 + titleHeight + spacing),
           static_cast<int16_t>(box.width - 40), static_cast<int16_t>(bodyHeight)}};
}
constexpr bool contains(const fui::Rect rect, const int x, const int y) {
  return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}
constexpr int tileAt(const fui::Rect strip, const uint8_t count, const int x, const int y) {
  for (uint8_t i = 0; i < count; ++i)
    if (contains(tileCell(strip, count, i), x, y)) return i;
  return -1;
}
inline constexpr uint8_t REGION_PAGE_SIZE = 10;
constexpr int pageCount(const int count, const int size) { return size > 0 ? (count + size - 1) / size : 0; }
constexpr uint8_t regionRowsPerPage(const int height) { return height >= 5 * 56 ? 5 : 4; }
constexpr fui::Rect regionCell(const fui::Rect grid, const uint8_t index, const uint8_t rows = 5) {
  if (!rows || index >= rows * 2) return {};
  return tileCell(
      fui::Rect{grid.x, static_cast<int16_t>(grid.y + static_cast<int32_t>(grid.height) * (index / 2) / rows),
                grid.width,
                static_cast<int16_t>(static_cast<int32_t>(grid.height) * (index / 2 + 1) / rows -
                                     static_cast<int32_t>(grid.height) * (index / 2) / rows)},
      2, index % 2);
}
constexpr fui::Rect courseCard(const fui::Rect body, const int index, const int height = 104) {
  return {body.x, static_cast<int16_t>(body.y + index * height), body.width, static_cast<int16_t>(height)};
}
constexpr int courseCardsPerPage(const fui::Rect body) { return maxValue(body.height / 104, 1); }
constexpr bool courseSortVisible(const fui::Rect body, const int selected, const int top) {
  const fui::Rect cards{body.x, body.y, body.width, static_cast<int16_t>(maxValue(body.height - 48, 0))};
  return top <= 1 && selected <= courseCardsPerPage(cards);
}
constexpr fui::Rect sortSegmentCell(const fui::Rect segment, const uint8_t half) { return tileCell(segment, 2, half); }
constexpr int regionAt(const fui::Rect grid, const int x, const int y) {
  for (uint8_t i = 0; i < REGION_PAGE_SIZE; ++i)
    if (contains(regionCell(grid, i), x, y)) return i;
  return -1;
}
constexpr int touchFooterHeight(const bool hasTouch, const bool keepTouch = false) {
  return hasTouch && keepTouch ? GOLF_TOUCH_FOOTER_HEIGHT : 0;
}
constexpr fui::Rect footerSafeRect(const fui::Rect screen, const fui::Rect physicalHintSafe, const bool touch,
                                   const bool keepTouch) {
  if (!touch) return physicalHintSafe;
  return keepTouch ? reserveHintEdge(physicalHintSafe, HintEdge::Bottom, touchFooterHeight(touch, keepTouch)) : screen;
}
constexpr int pagingSwipeDelta(const bool left, const bool right, const bool edgeBack) {
  return edgeBack ? 0 : left ? 1 : right ? -1 : 0;
}
int pagingSwipeDelta(const MappedInputManager& input);
enum class FooterAction : uint8_t { None, Back, Confirm, Previous, Next };

struct FooterLabels {
  const char* back = "";
  const char* confirm = "";
  const char* previous = "";
  const char* next = "";
};

struct TouchFooterLayout {
  fui::Rect safe{};
  fui::Rect footer{};
  fui::Rect cells[4]{};
};

constexpr TouchFooterLayout makeTouchFooterLayout(const fui::Rect safe) {
  TouchFooterLayout result{};
  result.safe = safe;
  const int16_t height = clampValue(GOLF_TOUCH_FOOTER_HEIGHT, 0, safe.height);
  result.footer = fui::Rect{safe.x, static_cast<int16_t>(safe.y + safe.height - height), safe.width, height};
  for (uint8_t index = 0; index < 4; ++index) {
    const int left = safe.x + static_cast<int32_t>(safe.width) * index / 4;
    const int right = safe.x + static_cast<int32_t>(safe.width) * (index + 1) / 4;
    result.cells[index] = fui::Rect{static_cast<int16_t>(left), result.footer.y, static_cast<int16_t>(right - left),
                                    result.footer.height};
  }
  return result;
}

constexpr int touchFooterCellAt(const TouchFooterLayout& layout, const int x, const int y) {
  if (x < layout.footer.x || x >= layout.footer.x + layout.footer.width || y < layout.footer.y ||
      y >= layout.footer.y + layout.footer.height) {
    return -1;
  }
  for (uint8_t index = 0; index < 4; ++index) {
    const fui::Rect cell = layout.cells[index];
    if (x >= cell.x && x < cell.x + cell.width && y >= cell.y && y < cell.y + cell.height) return index;
  }
  return -1;
}

constexpr const char* footerLabel(const FooterLabels labels, const FooterAction action) {
  switch (action) {
    case FooterAction::Back:
      return labels.back;
    case FooterAction::Confirm:
      return labels.confirm;
    case FooterAction::Previous:
      return labels.previous;
    case FooterAction::Next:
      return labels.next;
    case FooterAction::None:
    default:
      return "";
  }
}

constexpr FooterAction touchFooterActionAt(const TouchFooterLayout& layout, const FooterLabels labels, const int x,
                                           const int y) {
  const int cell = touchFooterCellAt(layout, x, y);
  if (cell < 0) return FooterAction::None;
  constexpr FooterAction actions[4] = {FooterAction::Back, FooterAction::Confirm, FooterAction::Previous,
                                       FooterAction::Next};
  const FooterAction action = actions[cell];
  const char* label = footerLabel(labels, action);
  return label != nullptr && label[0] != '\0' ? action : FooterAction::None;
}

class FooterInput {
 public:
  FooterInput(MappedInputManager& mappedInput, FooterLabels labels, bool keepTouch = false);

  bool wasReleased(FooterAction action) const;
  bool wasLongPressed(FooterAction action, unsigned long thresholdMs) const;
  bool wasTouchReleased(FooterAction action) const { return tapAction == action; }
  bool wasTouchLongPressed(FooterAction action) const { return longPressAction == action; }
  bool hasTap() const { return tapped; }
  fui::Point tapPoint() const { return fui::Point{tapX, tapY}; }
  bool tapIn(fui::Rect rect) const;
  int releasedFrontNavigationDelta() const;

 private:
  MappedInputManager& mappedInput;
  FooterAction tapAction = FooterAction::None;
  FooterAction longPressAction = FooterAction::None;
  int16_t tapX = 0;
  int16_t tapY = 0;
  bool tapped = false;
};

TouchFooterLayout touchFooterLayout(const GfxRenderer& renderer);
void drawFooter(GfxRenderer& renderer, MappedInputManager& mappedInput, FooterLabels labels, bool keepTouch = false);
void drawFooterTwo(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* back, const char* confirm,
                   bool keepTouch = false);

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
  const fui::Rect rawBody{
      result.safe.x, static_cast<int16_t>(result.header.y + result.header.height), result.safe.width,
      static_cast<int16_t>(result.safe.y + result.safe.height - result.header.y - result.header.height)};
  result.body = inset(rawBody, bodyPadding);
  result.contentMargins = relativeInsets(frameSafe, result.body);
  return result;
}

// Runtime adapters obtain both sources of hardware clearance. The first form
// uses a FreeInkUI frame-safe rect; the second derives the bezel-safe rect
// directly from GfxRenderer for legacy/raw drawing screens.
ChromeLayout chromeLayout(const GfxRenderer& renderer, const MappedInputManager& mappedInput, fui::Rect frameSafe,
                          int topPadding, fui::Insets bodyPadding = {}, bool keepTouch = false);
ChromeLayout chromeLayout(const GfxRenderer& renderer, const MappedInputManager& mappedInput, int topPadding,
                          fui::Insets bodyPadding = {}, bool keepTouch = false);
void drawHeader(const GfxRenderer& renderer, fui::Rect rect, const char* title, const char* rightLabel = nullptr);

constexpr int scoringDigitHeight(const int rowHeight, const bool focused) {
  const int preferred = focused ? 100 : 66;
  const int maximum = maxValue(rowHeight - 24, 1);
  return clampValue(preferred, 1, maximum);
}

constexpr int scoringHoleDigitHeight(const int rowHeight, const int lineHeight) {
  return clampValue(rowHeight - lineHeight - 16, 24, SCORING_HOLE_DIGIT_MAX_HEIGHT);
}

constexpr int scoringHoleDigitTop(const fui::Rect rect, const int digitHeight) {
  return rect.y + rect.height - digitHeight - SCORING_HOLE_DIGIT_RULE_GAP;
}

enum class CounterTapZone : uint8_t { None, Decrement, Centre, Increment };

constexpr CounterTapZone counterTapZoneAt(const fui::Rect band, const int x, const int y) {
  if (band.width <= 0 || band.height <= 0 || x < band.x || x >= band.x + band.width || y < band.y ||
      y >= band.y + band.height) {
    return CounterTapZone::None;
  }
  const int offset = x - band.x;
  if (offset < static_cast<int32_t>(band.width) / 3) return CounterTapZone::Decrement;
  if (offset < static_cast<int32_t>(band.width) * 2 / 3) return CounterTapZone::Centre;
  return CounterTapZone::Increment;
}

inline constexpr uint8_t HOLE_STRIP_PAGE_SIZE = 9;
constexpr uint8_t holeStripPageCount(const uint8_t holeCount) { return holeCount > HOLE_STRIP_PAGE_SIZE ? 2 : 1; }

constexpr uint8_t clampNineStripPage(const int page, const uint8_t holeCount) {
  return static_cast<uint8_t>(clampValue(page, 0, holeStripPageCount(holeCount) - 1));
}

constexpr fui::Rect nineStripCell(const fui::Rect strip, const uint8_t index) {
  if (index >= HOLE_STRIP_PAGE_SIZE) return {};
  const int left = strip.x + static_cast<int32_t>(strip.width) * index / HOLE_STRIP_PAGE_SIZE;
  const int right = strip.x + static_cast<int32_t>(strip.width) * (index + 1) / HOLE_STRIP_PAGE_SIZE;
  return fui::Rect{static_cast<int16_t>(left), strip.y, static_cast<int16_t>(right - left), strip.height};
}

constexpr int nineStripHoleAt(const fui::Rect strip, const uint8_t page, const uint8_t holeCount, const int x,
                              const int y) {
  if (page >= holeStripPageCount(holeCount)) return -1;
  for (uint8_t index = 0; index < HOLE_STRIP_PAGE_SIZE; ++index) {
    const fui::Rect cell = nineStripCell(strip, index);
    if (x < cell.x || x >= cell.x + cell.width || y < cell.y || y >= cell.y + cell.height) continue;
    const int hole = static_cast<int>(page) * HOLE_STRIP_PAGE_SIZE + index;
    return hole < holeCount ? hole : -1;
  }
  return -1;
}

constexpr fui::Rect evenRow(const fui::Rect rect, const uint8_t count, const uint8_t index) {
  if (count == 0 || index >= count) return {};
  const int top = rect.y + (static_cast<int32_t>(rect.height) * index) / count;
  const int bottom = rect.y + (static_cast<int32_t>(rect.height) * (index + 1)) / count;
  return fui::Rect{rect.x, static_cast<int16_t>(top), rect.width, static_cast<int16_t>(bottom - top)};
}

constexpr fui::Rect totalsCell(const fui::Rect rect, const uint8_t index) {
  if (index >= 3) return {};
  const int left = rect.x + static_cast<int32_t>(rect.width) * index / 3;
  const int right = rect.x + static_cast<int32_t>(rect.width) * (index + 1) / 3;
  return fui::Rect{static_cast<int16_t>(left), rect.y, static_cast<int16_t>(right - left), rect.height};
}

struct CardLayout {
  fui::Rect tabs{};
  fui::Rect table{};
  uint8_t rowCount = 0;
  int16_t minimumRowHeight = 0;
  bool valid = false;
};

constexpr CardLayout makeCardLayout(const fui::Rect content, const bool hasTabs, const int tabHeight, const int tabGap,
                                    const uint8_t playerCount, const bool hasPar, const int fontMinimumRowHeight) {
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
    const int appliedGap = remaining > requiredTable
                               ? minValue(static_cast<int16_t>(gap), static_cast<int16_t>(remaining - requiredTable))
                               : 0;
    cursor += appliedGap;
    remaining -= appliedGap;
  }
  result.table = fui::Rect{content.x, static_cast<int16_t>(cursor), content.width,
                           static_cast<int16_t>(remaining > 0 ? remaining : 0)};
  result.valid =
      result.rowCount > 0 && result.table.width > 0 && result.table.height >= result.rowCount * result.minimumRowHeight;
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
  result.info =
      fui::Rect{body.x, static_cast<int16_t>(body.y + menuHeight), body.width, static_cast<int16_t>(infoHeight)};
  result.rowHeight = static_cast<int16_t>(menuHeight / rowCount);
  result.valid = body.width > 0 && infoHeight >= infoMinimum && result.rowHeight >= rowMinimum;
  return result;
}

constexpr fui::Rect fairwayPromptSheet(const fui::Rect safe, const uint8_t players) {
  int count = 0;
  for (uint8_t slot = 0; slot < 4; ++slot)
    if (players & (1 << slot)) ++count;
  const int height = minValue(static_cast<int16_t>(44 + 64 * count), safe.height);
  return {safe.x, static_cast<int16_t>(safe.y + safe.height - height), safe.width, static_cast<int16_t>(height)};
}

constexpr fui::Rect fairwayPromptCell(const fui::Rect sheet, const uint8_t row, const uint8_t cell) {
  const int width = sheet.width / 4;
  return {static_cast<int16_t>(sheet.x + sheet.width / 2 + width * cell), static_cast<int16_t>(sheet.y + 44 + row * 64),
          static_cast<int16_t>(width), 64};
}

constexpr fui::Rect fairwayChipRect(const fui::Rect hole) {
  return {static_cast<int16_t>(hole.x + hole.width * 3 / 8), static_cast<int16_t>(hole.y + (hole.height - 44) / 2), 72,
          44};
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

constexpr ScoringLayout makeScoringLayout(const fui::Rect safe, const int topPadding, const uint8_t focusedCounter,
                                          const bool hasPenalty, const int fontMinimumRowHeight) {
  ScoringLayout result{};
  result.safe = safe;
  const int fontMin = maxValue(static_cast<int16_t>(fontMinimumRowHeight), 1);
  const int topPad = clampValue(topPadding, 0, safe.height);
  constexpr int header = GOLF_HEADER_HEIGHT;
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
  fui::Rect* bands[8] = {&result.header,      &result.hole,    &result.counters[0], &result.counters[1],
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
    *bands[index] =
        fui::Rect{safe.x, static_cast<int16_t>(cursor), safe.width, static_cast<int16_t>(height > 0 ? height : 0)};
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
  const int minima[6] = {
      fontMin * 2 + 12 > 60 ? fontMin * 2 + 12 : 60, fontMin * 2 + 16 > 64 ? fontMin * 2 + 16 : 64,
      fontMin + 18 > 42 ? fontMin + 18 : 42,         fontMin + 18 > 42 ? fontMin + 18 : 42,
      fontMin + 18 > 42 ? fontMin + 18 : 42,         hasPenalty ? (fontMin + 10 > 32 ? fontMin + 10 : 32) : 0};
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
    *bands[index] =
        fui::Rect{body.x, static_cast<int16_t>(cursor), body.width, static_cast<int16_t>(height > 0 ? height : 0)};
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
    result.rows[index] =
        fui::Rect{body.x, static_cast<int16_t>(cursor), body.width, static_cast<int16_t>(height > 0 ? height : 0)};
    cursor += height;
  }
  result.valid = body.width > 0 && fitsMinimum;
  return result;
}

}  // namespace golfui
