#include <gtest/gtest.h>

#include <array>

#include "GolfUiLayout.h"

namespace {

namespace fui = freeink::ui;

constexpr int16_t FONT_MINIMUM = 24;
constexpr int16_t HINT_RESERVE = 40;
constexpr fui::Insets BEZEL{7, 9, 11, 13};

bool inside(const fui::Rect outer, const fui::Rect inner) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.width >= 0 && inner.height >= 0 &&
         inner.x + inner.width <= outer.x + outer.width && inner.y + inner.height <= outer.y + outer.height;
}

bool overlaps(const fui::Rect a, const fui::Rect b) {
  if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) return false;
  return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

void expectOrderedInside(const fui::Rect safe, const fui::Rect* rects, const size_t count) {
  for (size_t index = 0; index < count; ++index) {
    EXPECT_TRUE(inside(safe, rects[index]));
    if (index > 0) {
      EXPECT_FALSE(overlaps(rects[index - 1], rects[index]));
      EXPECT_LE(rects[index - 1].y + rects[index - 1].height, rects[index].y);
    }
  }
}

struct Case {
  int16_t width;
  int16_t height;
  golfui::HintEdge edge;
};

std::array<Case, 8> cases() {
  return {{{480, 800, golfui::HintEdge::Top},
           {480, 800, golfui::HintEdge::Right},
           {480, 800, golfui::HintEdge::Bottom},
           {480, 800, golfui::HintEdge::Left},
           {800, 480, golfui::HintEdge::Top},
           {800, 480, golfui::HintEdge::Right},
           {800, 480, golfui::HintEdge::Bottom},
           {800, 480, golfui::HintEdge::Left}}};
}

TEST(GolfUiLayout, IntersectsEveryHintEdgeWithBezelAndUsesFrameRelativeMargins) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const fui::Rect frameSafe = golfui::inset(screen, BEZEL);
    const fui::Rect hintSafe = golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE);
    const golfui::ChromeLayout layout = golfui::makeChromeLayout(frameSafe, hintSafe, 5, 45);

    EXPECT_TRUE(inside(frameSafe, layout.safe));
    EXPECT_TRUE(inside(hintSafe, layout.safe));
    EXPECT_TRUE(inside(layout.safe, layout.header));
    EXPECT_TRUE(inside(layout.safe, layout.body));
    EXPECT_FALSE(overlaps(layout.header, layout.body));
    EXPECT_EQ(layout.contentMargins.top, layout.body.y - frameSafe.y);
    EXPECT_EQ(layout.contentMargins.left, layout.body.x - frameSafe.x);
    EXPECT_EQ(layout.contentMargins.right, frameSafe.x + frameSafe.width - layout.body.x - layout.body.width);
    EXPECT_EQ(layout.contentMargins.bottom, frameSafe.y + frameSafe.height - layout.body.y - layout.body.height);
  }
}

TEST(GolfUiLayout, CardRowsFitOneThroughFourPlayersInBothOrientationsAndAllHintEdges) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const fui::Rect frameSafe = golfui::inset(screen, BEZEL);
    const golfui::ChromeLayout chrome = golfui::makeChromeLayout(
        frameSafe, golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE), 15, 84);
    for (uint8_t players = 1; players <= 4; ++players) {
      for (const bool hasTabs : {false, true}) {
        for (const bool hasPar : {false, true}) {
          const golfui::CardLayout layout =
              golfui::makeCardLayout(chrome.body, hasTabs, 50, 10, players, hasPar, FONT_MINIMUM);
          ASSERT_TRUE(layout.valid);
          EXPECT_TRUE(inside(chrome.body, layout.tabs));
          EXPECT_TRUE(inside(chrome.body, layout.table));
          EXPECT_FALSE(overlaps(layout.tabs, layout.table));
          for (uint8_t row = 0; row < layout.rowCount; ++row) {
            const fui::Rect rowRect = golfui::evenRow(layout.table, layout.rowCount, row);
            EXPECT_TRUE(inside(layout.table, rowRect));
            EXPECT_GE(rowRect.height, FONT_MINIMUM);
            if (row > 0) {
              EXPECT_FALSE(overlaps(golfui::evenRow(layout.table, layout.rowCount, row - 1), rowRect));
            }
          }
        }
      }
    }
  }
}

TEST(GolfUiLayout, ScoringBandsStayInsideSafeAreaAtLandscapeHeight) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const fui::Rect safe = golfui::intersect(golfui::inset(screen, BEZEL),
                                              golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE));
    for (uint8_t focused = 0; focused < 3; ++focused) {
      for (const bool penalty : {false, true}) {
        const golfui::ScoringLayout layout = golfui::makeScoringLayout(safe, 15, 84, focused, penalty, FONT_MINIMUM);
        ASSERT_TRUE(layout.valid);
        const fui::Rect rects[] = {layout.header,      layout.hole,   layout.counters[0], layout.counters[1],
                                   layout.counters[2], layout.penalty, layout.totals,      layout.nineStrip};
        expectOrderedInside(safe, rects, std::size(rects));
        EXPECT_GE(layout.header.height, FONT_MINIMUM);
        EXPECT_GE(layout.hole.height, FONT_MINIMUM);
        for (const fui::Rect counter : layout.counters) EXPECT_GE(counter.height, FONT_MINIMUM);
        EXPECT_GE(layout.totals.height, FONT_MINIMUM);
        EXPECT_GE(layout.nineStrip.height, FONT_MINIMUM);
        EXPECT_EQ(layout.penalty.height > 0, penalty);
      }
    }
  }
}

TEST(GolfUiLayout, ReviewStatisticsAndMenuRowsUseAvailableSafeHeight) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const fui::Rect frameSafe = golfui::inset(screen, BEZEL);
    const golfui::ChromeLayout chrome = golfui::makeChromeLayout(
        frameSafe, golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE), 15, 84);

    const golfui::HoleReviewLayout review = golfui::makeHoleReviewLayout(chrome.body, true, FONT_MINIMUM);
    ASSERT_TRUE(review.valid);
    const fui::Rect reviewRects[] = {review.hole,       review.score,      review.details[0],
                                     review.details[1], review.details[2], review.penalty};
    expectOrderedInside(chrome.body, reviewRects, std::size(reviewRects));
    for (const fui::Rect rect : reviewRects) EXPECT_GE(rect.height, FONT_MINIMUM);

    const golfui::StatisticsLayout statistics = golfui::makeStatisticsLayout(chrome.body, FONT_MINIMUM);
    ASSERT_TRUE(statistics.valid);
    expectOrderedInside(chrome.body, statistics.rows, std::size(statistics.rows));
    for (uint8_t row = 0; row < golfui::StatisticsLayout::ROW_COUNT; ++row) {
      EXPECT_GE(statistics.rows[row].height,
                statistics.section[row] ? statistics.minimumSectionHeight : statistics.minimumStatHeight);
    }

    const golfui::MenuInfoLayout menu = golfui::makeMenuInfoLayout(chrome.body, 4, FONT_MINIMUM, 48);
    ASSERT_TRUE(menu.valid);
    EXPECT_TRUE(inside(chrome.body, menu.menu));
    EXPECT_TRUE(inside(chrome.body, menu.info));
    EXPECT_FALSE(overlaps(menu.menu, menu.info));
    EXPECT_GE(menu.rowHeight, FONT_MINIMUM);
    for (uint8_t row = 0; row < 4; ++row) {
      const fui::Rect rowRect = golfui::evenRow(menu.menu, 4, row);
      EXPECT_GE(rowRect.height, FONT_MINIMUM);
      EXPECT_TRUE(inside(menu.menu, rowRect));
    }
  }
}

}  // namespace
