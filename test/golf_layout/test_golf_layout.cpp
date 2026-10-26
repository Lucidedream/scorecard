#include <gtest/gtest.h>

#include <array>

#include "GolfPlayerSetupPolicy.h"
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
        frameSafe, golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE), 15, golfui::GOLF_HEADER_HEIGHT);
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
    const fui::Rect safe =
        golfui::intersect(golfui::inset(screen, BEZEL), golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE));
    for (uint8_t focused = 0; focused < 3; ++focused) {
      for (const bool penalty : {false, true}) {
        const golfui::ScoringLayout layout = golfui::makeScoringLayout(safe, 15, focused, penalty, FONT_MINIMUM);
        ASSERT_TRUE(layout.valid);
        const fui::Rect rects[] = {layout.header,      layout.hole,    layout.counters[0], layout.counters[1],
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

TEST(GolfUiLayout, GolfHeaderIsFixedAtFortySixPixels) {
  const fui::Rect safe{0, 0, 480, 800};
  const golfui::ChromeLayout chrome = golfui::makeChromeLayout(safe, safe, 15, golfui::GOLF_HEADER_HEIGHT);
  const golfui::ScoringLayout scoring = golfui::makeScoringLayout(safe, 15, 1, true, FONT_MINIMUM);

  EXPECT_EQ(chrome.header.height, 46);
  EXPECT_EQ(scoring.header.height, 46);
}

TEST(GolfUiLayout, LyraHeaderRecoveryIsRedistributedToWeightedScoringBands) {
  const golfui::ScoringLayout layout = golfui::makeScoringLayout(fui::Rect{0, 0, 480, 800}, 15, 1, true, FONT_MINIMUM);

  EXPECT_EQ(layout.header.height, 46);
  EXPECT_EQ(layout.hole.height, 87);
  EXPECT_EQ(layout.counters[0].height, 123);
  EXPECT_EQ(layout.counters[1].height, 175);
  EXPECT_EQ(layout.counters[2].height, 123);
  EXPECT_EQ(layout.penalty.height, 52);
  EXPECT_EQ(layout.totals.height, 91);
  EXPECT_EQ(layout.nineStrip.height, 88);
  EXPECT_EQ(golfui::scoringDigitHeight(layout.counters[1].height, true), 100);
  EXPECT_EQ(golfui::scoringDigitHeight(layout.counters[0].height, false), 66);
  const golfui::ScoringLayout withoutPenalty =
      golfui::makeScoringLayout(fui::Rect{0, 0, 480, 800}, 15, 1, false, FONT_MINIMUM);
  EXPECT_EQ(golfui::scoringDigitHeight(withoutPenalty.counters[1].height, true), 100);
  EXPECT_EQ(golfui::scoringDigitHeight(withoutPenalty.counters[0].height, false), 66);
  EXPECT_EQ(golfui::scoringDigitHeight(124, true), 100);
  EXPECT_EQ(golfui::scoringDigitHeight(123, true), 99);
  EXPECT_EQ(golfui::scoringDigitHeight(90, false), 66);
  EXPECT_EQ(golfui::scoringDigitHeight(89, false), 65);
}

TEST(GolfUiLayout, ScoringHoleNumberIsSmallerAndClearsItsRule) {
  const fui::Rect holeBand{0, 46, 480, 87};
  const int digitHeight = golfui::scoringHoleDigitHeight(holeBand.height, 20);
  const int digitTop = golfui::scoringHoleDigitTop(holeBand, digitHeight);

  EXPECT_EQ(digitHeight, 48);
  EXPECT_EQ(holeBand.y + holeBand.height - digitTop - digitHeight, 12);
}

TEST(GolfUiLayout, CounterTapZonesTileBandWithDeterministicBoundaries) {
  constexpr fui::Rect band{7, 11, 10, 60};

  EXPECT_EQ(golfui::counterTapZoneAt(band, 7, 11), golfui::CounterTapZone::Decrement);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 9, 70), golfui::CounterTapZone::Decrement);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 10, 11), golfui::CounterTapZone::Centre);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 12, 70), golfui::CounterTapZone::Centre);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 13, 11), golfui::CounterTapZone::Increment);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 16, 70), golfui::CounterTapZone::Increment);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 6, 11), golfui::CounterTapZone::None);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 17, 11), golfui::CounterTapZone::None);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 7, 10), golfui::CounterTapZone::None);
  EXPECT_EQ(golfui::counterTapZoneAt(band, 7, 71), golfui::CounterTapZone::None);
}

TEST(GolfUiLayout, TotalsBandUsesThreeContiguousCells) {
  const fui::Rect totals{7, 11, 458, 64};
  const fui::Rect first = golfui::totalsCell(totals, 0);
  const fui::Rect second = golfui::totalsCell(totals, 1);
  const fui::Rect third = golfui::totalsCell(totals, 2);

  EXPECT_EQ(first.x, 7);
  EXPECT_EQ(first.width, 152);
  EXPECT_EQ(second.x, 159);
  EXPECT_EQ(second.width, 153);
  EXPECT_EQ(third.x, 312);
  EXPECT_EQ(third.width, 153);
  EXPECT_EQ(first.y, totals.y);
  EXPECT_EQ(second.y, totals.y);
  EXPECT_EQ(third.y, totals.y);
  EXPECT_EQ(first.height, totals.height);
  EXPECT_EQ(second.height, totals.height);
  EXPECT_EQ(third.height, totals.height);
  EXPECT_EQ(third.x + third.width, totals.x + totals.width);
}

TEST(GolfUiLayout, NineStripCellsTileOddWidthAndHitEveryBoundary) {
  constexpr fui::Rect strip{7, 11, 461, 58};
  for (uint8_t index = 0; index < golfui::HOLE_STRIP_PAGE_SIZE; ++index) {
    const auto cell = golfui::nineStripCell(strip, index);
    EXPECT_TRUE(inside(strip, cell));
    EXPECT_EQ(cell.y, strip.y);
    EXPECT_EQ(cell.height, strip.height);
    EXPECT_EQ(cell.x, index == 0 ? strip.x : golfui::nineStripCell(strip, index - 1).right());
    for (const int x : {static_cast<int>(cell.x), cell.x + cell.width / 2, cell.right() - 1}) {
      EXPECT_EQ(golfui::nineStripHoleAt(strip, 0, 18, x, strip.y), index);
      EXPECT_EQ(golfui::nineStripHoleAt(strip, 1, 18, x, strip.bottom() - 1), index + 9);
    }
  }
  EXPECT_EQ(golfui::nineStripCell(strip, 8).right(), strip.right());
  EXPECT_EQ(golfui::nineStripHoleAt(strip, 0, 18, strip.x - 1, strip.y), -1);
  EXPECT_EQ(golfui::nineStripHoleAt(strip, 0, 18, strip.right(), strip.y), -1);
  EXPECT_EQ(golfui::nineStripHoleAt(strip, 0, 18, strip.x, strip.y - 1), -1);
  EXPECT_EQ(golfui::nineStripHoleAt(strip, 0, 18, strip.x, strip.bottom()), -1);
}

TEST(GolfUiLayout, NineStripEmptyCellsAndInvalidPagesMiss) {
  constexpr fui::Rect strip{0, 0, 480, 58};
  for (uint8_t holes = 0; holes <= 18; ++holes) {
    for (uint8_t page = 0; page <= 2; ++page) {
      for (uint8_t index = 0; index < 9; ++index) {
        const auto cell = golfui::nineStripCell(strip, index);
        const int hole = page * 9 + index;
        EXPECT_EQ(golfui::nineStripHoleAt(strip, page, holes, cell.x + cell.width / 2, cell.y),
                  page < golfui::holeStripPageCount(holes) && hole < holes ? hole : -1);
      }
    }
  }
}

TEST(GolfUiLayout, NineStripPageCountAndClampNeverWrap) {
  for (uint8_t holes = 0; holes <= 18; ++holes) {
    const uint8_t lastPage = holes > 9 ? 1 : 0;
    EXPECT_EQ(golfui::holeStripPageCount(holes), lastPage + 1);
    EXPECT_EQ(golfui::clampNineStripPage(-1, holes), 0);
    EXPECT_EQ(golfui::clampNineStripPage(0, holes), 0);
    EXPECT_EQ(golfui::clampNineStripPage(1, holes), lastPage);
    EXPECT_EQ(golfui::clampNineStripPage(2, holes), lastPage);
  }
}

TEST(GolfPlayerSetupPolicy, EveryPlayerCountReviewsRoster) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);

  for (uint8_t count = 1; count <= GOLF_MAX_PLAYERS; ++count) {
    EXPECT_EQ(golfPlayerSetupNext(count), GolfPlayerSetupNext::ReviewRoster);
  }

  golfApplyPlayerCount(round, 3, "Blue");
  for (uint8_t slot = 0; slot < GOLF_MAX_PLAYERS; ++slot) {
    EXPECT_STREQ(round.players[slot].tee, slot < 3 ? "Blue" : "");
  }
  EXPECT_STREQ(round.players[0].name, "Noah");
  EXPECT_STREQ(round.players[1].name, "Player 2");
  EXPECT_STREQ(round.players[2].name, "Player 3");
  EXPECT_STREQ(round.players[3].name, "Player 4");
}

TEST(GolfPlayerSetupPolicy, DefaultSoloSetupReviewsRosterWithPlayerOneEnabled) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  uint8_t playerCount = 1;
  golfSetPlayerCount(round, playerCount, 1, "Blue");

  ASSERT_EQ(golfPlayerSetupNext(playerCount), GolfPlayerSetupNext::ReviewRoster);
  EXPECT_STREQ(round.players[0].tee, "Blue");
  for (uint8_t slot = 1; slot < GOLF_MAX_PLAYERS; ++slot) {
    EXPECT_STREQ(round.players[slot].tee, "");
  }
}

TEST(GolfPlayerSetupPolicy, SteppingAwayAndBackLeavesExactlyOneEnabledPlayer) {
  GolfRound round{};
  initializeGolfPlayerDefaults(round);
  uint8_t playerCount = 1;
  golfSetPlayerCount(round, playerCount, 1, "Blue");
  golfSetPlayerCount(round, playerCount, golfStepPlayerCount(playerCount, 1), "Blue");
  golfSetPlayerCount(round, playerCount, golfStepPlayerCount(playerCount, -1), "Blue");

  EXPECT_EQ(playerCount, 1);
  for (uint8_t slot = 0; slot < GOLF_MAX_PLAYERS; ++slot) {
    EXPECT_STREQ(round.players[slot].tee, slot == 0 ? "Blue" : "");
  }
}

TEST(GolfPlayerSetupPolicy, CountsEnableExactlyThatManyPlayers) {
  for (uint8_t count = 2; count <= GOLF_MAX_PLAYERS; ++count) {
    GolfRound round{};
    initializeGolfPlayerDefaults(round);
    uint8_t playerCount = 1;
    golfSetPlayerCount(round, playerCount, count, "Blue");

    EXPECT_EQ(playerCount, count);
    for (uint8_t slot = 0; slot < GOLF_MAX_PLAYERS; ++slot) {
      EXPECT_STREQ(round.players[slot].tee, slot < count ? "Blue" : "");
    }
  }
}

TEST(GolfPlayerSetupPolicy, CountStepsStayWithinOneAndFour) {
  EXPECT_EQ(golfClampPlayerCount(0), 1);
  EXPECT_EQ(golfClampPlayerCount(GOLF_MAX_PLAYERS + 1), GOLF_MAX_PLAYERS);
  EXPECT_EQ(golfStepPlayerCount(1, -1), 1);
  EXPECT_EQ(golfStepPlayerCount(1, 1), 2);
  EXPECT_EQ(golfStepPlayerCount(GOLF_MAX_PLAYERS, 1), GOLF_MAX_PLAYERS);
  EXPECT_EQ(golfStepPlayerCount(GOLF_MAX_PLAYERS, -1), GOLF_MAX_PLAYERS - 1);
}

TEST(GolfUiLayout, ReviewStatisticsAndMenuRowsUseAvailableSafeHeight) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const fui::Rect frameSafe = golfui::inset(screen, BEZEL);
    const golfui::ChromeLayout chrome = golfui::makeChromeLayout(
        frameSafe, golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE), 15, golfui::GOLF_HEADER_HEIGHT);

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

TEST(GolfUiLayout, TouchFooterCellsTileTheBezelSafeWidth) {
  constexpr fui::Rect safe{17, 23, 451, 731};
  constexpr golfui::TouchFooterLayout layout = golfui::makeTouchFooterLayout(safe);

  EXPECT_EQ(layout.footer.x, safe.x);
  EXPECT_EQ(layout.footer.y, safe.y + safe.height - golfui::GOLF_TOUCH_FOOTER_HEIGHT);
  EXPECT_EQ(layout.footer.width, safe.width);
  EXPECT_EQ(layout.footer.height, golfui::GOLF_TOUCH_FOOTER_HEIGHT);
  EXPECT_EQ(layout.cells[0].x, safe.x);
  EXPECT_EQ(layout.cells[3].x + layout.cells[3].width, safe.x + safe.width);
  for (uint8_t index = 1; index < 4; ++index) {
    EXPECT_EQ(layout.cells[index - 1].x + layout.cells[index - 1].width, layout.cells[index].x);
    EXPECT_FALSE(overlaps(layout.cells[index - 1], layout.cells[index]));
  }
}

TEST(GolfUiLayout, TouchFooterHitTestRoundTripsAndRejectsBodyAndBezel) {
  constexpr fui::Rect safe{13, 19, 455, 762};
  constexpr golfui::TouchFooterLayout layout = golfui::makeTouchFooterLayout(safe);

  for (uint8_t index = 0; index < 4; ++index) {
    const fui::Rect cell = layout.cells[index];
    EXPECT_EQ(golfui::touchFooterCellAt(layout, cell.x + cell.width / 2, cell.y + cell.height / 2), index);
  }
  EXPECT_EQ(golfui::touchFooterCellAt(layout, safe.x, layout.footer.y - 1), -1);
  EXPECT_EQ(golfui::touchFooterCellAt(layout, safe.x - 1, layout.footer.y), -1);
  EXPECT_EQ(golfui::touchFooterCellAt(layout, safe.x + safe.width, layout.footer.y), -1);
  EXPECT_EQ(golfui::touchFooterCellAt(layout, safe.x, safe.y + safe.height), -1);
}

TEST(GolfUiLayout, TouchFooterEmptyLabelsAreInert) {
  constexpr golfui::TouchFooterLayout layout = golfui::makeTouchFooterLayout(fui::Rect{10, 20, 460, 760});
  constexpr golfui::FooterLabels labels{"Back", "", "Previous", "Next"};

  EXPECT_EQ(golfui::touchFooterActionAt(layout, labels, layout.cells[0].x + 1, layout.cells[0].y + 1),
            golfui::FooterAction::Back);
  EXPECT_EQ(golfui::touchFooterActionAt(layout, labels, layout.cells[1].x + 1, layout.cells[1].y + 1),
            golfui::FooterAction::None);
}

}  // namespace

TEST(GolfBlocks, X4MainMenuMatchesPreExtractionRects) {
  for (const Case test : cases()) {
    const fui::Rect frame = golfui::inset({0, 0, test.width, test.height}, BEZEL);
    const auto chrome = golfui::makeChromeLayout(frame, golfui::reserveHintEdge(frame, test.edge, HINT_RESERVE), 5, 46);
    for (int spacing : {8, 12, 16}) {
      const fui::Rect strip{chrome.body.x, static_cast<int16_t>(chrome.body.y + spacing), chrome.body.width, 112};
      for (uint8_t count : {3, 4}) {
        for (uint8_t i = 0; i < count; ++i) {
          const int left = strip.x + static_cast<int32_t>(strip.width) * i / count;
          const int right = strip.x + static_cast<int32_t>(strip.width) * (i + 1) / count;
          const auto rect = golfui::tileCell(strip, count, i);
          EXPECT_EQ(rect.x, left);
          EXPECT_EQ(rect.y, strip.y);
          EXPECT_EQ(rect.width, right - left);
          EXPECT_EQ(rect.height, 112);
          const auto geometry = golfui::tileGeometry(rect);
          EXPECT_EQ(geometry.icon.x, left + (right - left - 32) / 2);
          EXPECT_EQ(geometry.icon.y, strip.y + 16);
          EXPECT_EQ(geometry.icon.width, 32);
          EXPECT_EQ(geometry.icon.height, 32);
          EXPECT_EQ(geometry.label.x, left);
          EXPECT_EQ(geometry.label.y, strip.y + 54);
          EXPECT_EQ(geometry.label.width, right - left);
          EXPECT_EQ(geometry.label.height, 54);
        }
      }
      const fui::Rect available{strip.x, static_cast<int16_t>(strip.bottom() + spacing), strip.width, 350};
      for (int bodyHeight : {24, 48, 72}) {
        const auto detail = golfui::detailGeometry(available, 28, bodyHeight, spacing);
        EXPECT_EQ(detail.box.x, available.x);
        EXPECT_EQ(detail.box.y, available.y);
        EXPECT_EQ(detail.box.width, available.width);
        EXPECT_EQ(detail.box.height, 36 + 28 + spacing + bodyHeight);
        EXPECT_EQ(detail.title.x, available.x + 20);
        EXPECT_EQ(detail.title.y, available.y + 18);
        EXPECT_EQ(detail.title.width, available.width - 40);
        EXPECT_EQ(detail.title.height, 28);
        EXPECT_EQ(detail.body.x, available.x + 20);
        EXPECT_EQ(detail.body.y, available.y + 18 + 28 + spacing);
        EXPECT_EQ(detail.body.width, available.width - 40);
        EXPECT_EQ(detail.body.height, bodyHeight);
      }
    }
  }
}

TEST(GolfBlocks, TilesGridCardsAndSortHaveExactHitBoundaries) {
  constexpr fui::Rect strip{13, 47, 453, 190};
  EXPECT_EQ(golfui::tileAt(strip, 3, 13, 47), 0);
  EXPECT_EQ(golfui::tileAt(strip, 3, 164, 47), 1);
  EXPECT_EQ(golfui::tileAt(strip, 3, 466, 47), -1);
  EXPECT_EQ(golfui::tileAt(strip, 3, 13, 237), -1);
  EXPECT_EQ(golfui::tileAt(strip, 0, 13, 47), -1);
  for (const fui::Rect grid : {fui::Rect{13, 47, 453, 680}, fui::Rect{9, 50, 760, 350}}) {
    for (uint8_t i = 0; i < 10; ++i) {
      const auto cell = golfui::regionCell(grid, i);
      EXPECT_TRUE(inside(grid, cell));
      EXPECT_EQ(golfui::regionAt(grid, cell.x, cell.y), i);
      EXPECT_EQ(golfui::regionAt(grid, cell.right() - 1, cell.bottom() - 1), i);
      if (i % 2) EXPECT_EQ(golfui::regionCell(grid, i - 1).right(), cell.x);
    }
    EXPECT_EQ(golfui::regionAt(grid, grid.right(), grid.y), -1);
    EXPECT_EQ(golfui::regionAt(grid, grid.x, grid.bottom()), -1);
    const int count = golfui::courseCardsPerPage(grid);
    for (int i = 0; i < count; ++i) {
      const auto card = golfui::courseCard(grid, i);
      EXPECT_GE(card.height, 96);
      EXPECT_TRUE(inside(grid, card));
      EXPECT_TRUE(golfui::contains(card, card.x, card.y));
      EXPECT_FALSE(golfui::contains(card, card.x, card.bottom()));
    }
  }
  EXPECT_EQ(golfui::sortSegmentCell(strip, 0).right(), golfui::sortSegmentCell(strip, 1).x);
  EXPECT_EQ(golfui::tileAt(strip, 2, golfui::sortSegmentCell(strip, 1).x, strip.y), 1);
  EXPECT_EQ(golfui::pageCount(31, 10), 4);
  EXPECT_EQ(golfui::pageCount(64, 10), 7);
  EXPECT_EQ(golfui::pageCount(0, 10), 0);
}

TEST(GolfBlocks, RestoredCourseWindowOnlyShowsSortOnFirstPage) {
  constexpr fui::Rect body{0, 46, 480, 680};
  EXPECT_TRUE(golfui::courseSortVisible(body, 0, 0));
  EXPECT_TRUE(golfui::courseSortVisible(body, 6, 1));
  EXPECT_FALSE(golfui::courseSortVisible(body, 10, 0));
  EXPECT_FALSE(golfui::courseSortVisible(body, 4, 3));
}

TEST(GolfFooterless, HeightAndPagingExcludeEdgeBack) {
  EXPECT_EQ(golfui::touchFooterHeight(true), 0);
  EXPECT_EQ(golfui::touchFooterHeight(true, true), 56);
  EXPECT_EQ(golfui::touchFooterHeight(false), 0);
  EXPECT_EQ(golfui::touchFooterHeight(false, true), 0);
  EXPECT_EQ(golfui::pagingSwipeDelta(true, false, false), 1);
  EXPECT_EQ(golfui::pagingSwipeDelta(false, true, false), -1);
  EXPECT_EQ(golfui::pagingSwipeDelta(false, false, false), 0);
  EXPECT_EQ(golfui::pagingSwipeDelta(true, false, true), 0);
  EXPECT_EQ(golfui::pagingSwipeDelta(false, true, true), 0);
}

TEST(GolfFooterless, ChromeUsesFullTouchBodyAndRetainsX4HintsOnEveryEdge) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const auto physical = golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE);
    const auto touch = golfui::footerSafeRect(screen, physical, true, false);
    EXPECT_EQ(touch.x, screen.x);
    EXPECT_EQ(touch.y, screen.y);
    EXPECT_EQ(touch.width, screen.width);
    EXPECT_EQ(touch.height, screen.height);
    const auto x4 = golfui::footerSafeRect(screen, physical, false, false);
    EXPECT_EQ(x4.x, physical.x);
    EXPECT_EQ(x4.y, physical.y);
    EXPECT_EQ(x4.width, physical.width);
    EXPECT_EQ(x4.height, physical.height);
    const auto scoring = golfui::footerSafeRect(screen, physical, true, true);
    EXPECT_EQ(scoring.height, physical.height - 56);
    EXPECT_EQ(scoring.width, physical.width);
  }
  const auto clipped = golfui::detailGeometry({13, 20, 440, 90}, 28, 72, 12);
  EXPECT_EQ(clipped.box.height, 90);
}

TEST(GolfBlocks, HubAndCourseInfoUseMainMenuStripHeightAndLabelGeometry) {
  for (const Case test : cases()) {
    const fui::Rect body{13, 72, static_cast<int16_t>(test.width - 26), static_cast<int16_t>(test.height - 100)};
    const auto strip = golfui::tileStripRect(body);
    EXPECT_EQ(strip.height, 112);
    EXPECT_EQ(strip.x, body.x);
    EXPECT_EQ(strip.y, body.y);
    EXPECT_EQ(strip.width, body.width);
    for (uint8_t tiles : {2, 3, 4}) {
      for (uint8_t i = 0; i < tiles; ++i) {
        const auto rect = golfui::tileCell(strip, tiles, i);
        const auto geometry = golfui::tileGeometry(rect);
        EXPECT_EQ(rect.height, 112);
        EXPECT_EQ(geometry.icon.y, body.y + 16);
        EXPECT_EQ(geometry.label.y - geometry.icon.bottom(), 6);
        EXPECT_EQ(geometry.label.height, 54);
      }
    }
  }
  EXPECT_EQ(golfui::tileStripRect({0, 0, 480, 80}).height, 80);
}

TEST(GolfFooterless, PlayerCountKeepsOriginalFooterAndRosterReleasesItsSpace) {
  constexpr fui::Rect screen{0, 0, 480, 800};
  constexpr fui::Rect physicalHints{0, 0, 480, 760};
  for (const auto phase :
       {GolfPlayerSetupPhase::Count, GolfPlayerSetupPhase::Players, GolfPlayerSetupPhase::TeeChoice}) {
    const bool keep = golfPlayerSetupKeepsTouchFooter(phase);
    EXPECT_EQ(keep, phase == GolfPlayerSetupPhase::Count);
    const auto safe = golfui::footerSafeRect(screen, physicalHints, true, keep);
    EXPECT_EQ(safe.height, phase == GolfPlayerSetupPhase::Count ? 704 : 800);
    EXPECT_EQ(golfui::touchFooterHeight(true, keep), phase == GolfPlayerSetupPhase::Count ? 56 : 0);
    const auto x4 = golfui::footerSafeRect(screen, physicalHints, false, keep);
    EXPECT_EQ(x4.height, physicalHints.height);
  }
  const auto footer = golfui::makeTouchFooterLayout(screen);
  constexpr golfui::FooterLabels labels{"Back", "Next", "Previous", "Next"};
  constexpr golfui::FooterAction actions[] = {golfui::FooterAction::Back, golfui::FooterAction::Confirm,
                                              golfui::FooterAction::Previous, golfui::FooterAction::Next};
  for (uint8_t i = 0; i < 4; ++i) {
    const auto cell = footer.cells[i];
    EXPECT_EQ(golfui::touchFooterActionAt(footer, labels, cell.x + cell.width / 2, cell.y + cell.height / 2),
              actions[i]);
  }
}

TEST(GolfLayout, RegionSearchBarChoosesRowsByRemainingHeight) {
  EXPECT_EQ(golfui::regionRowsPerPage(280), 5);
  EXPECT_EQ(golfui::regionRowsPerPage(279), 4);
  EXPECT_EQ(golfui::regionRowsPerPage(400), 5);
  for (const int height : {224, 279, 280, 340}) {
    const uint8_t rows = golfui::regionRowsPerPage(height);
    const freeink::ui::Rect grid{0, 48, 400, static_cast<int16_t>(height)};
    const auto last = golfui::regionCell(grid, rows * 2 - 1, rows);
    EXPECT_EQ(last.bottom(), grid.bottom());
    EXPECT_EQ(last.right(), grid.right());
    EXPECT_EQ(golfui::regionCell(grid, rows * 2, rows).height, 0);
  }
}

TEST(GolfFairwayLayout, ChipAndFourPlayerPromptFitScoringSafeArea) {
  for (const auto safe :
       {freeink::ui::Rect{0, 0, 480, 700}, freeink::ui::Rect{0, 0, 800, 380}, freeink::ui::Rect{12, 16, 456, 680}}) {
    const auto scoring = golfui::makeScoringLayout(safe, 0, 2, false, 24);
    const auto row = scoring.counters[2];
    const auto chip = golfui::fairwayChipRect(row);
    EXPECT_GE(chip.width, 44);
    EXPECT_GE(chip.height, 44);
    EXPECT_GE(chip.x, row.x + row.width * 2 / 3);
    EXPECT_EQ(chip.y, row.y);
    EXPECT_EQ(chip.right(), row.right());
    EXPECT_EQ(chip.bottom(), row.bottom());
    const auto sheet = golfui::fairwayPromptSheet(safe, 15);
    for (uint8_t row = 0; row < 4; ++row) {
      const auto playerRow = golfui::fairwayPromptRow(sheet, row);
      EXPECT_EQ(playerRow.x, sheet.x);
      EXPECT_EQ(playerRow.width, sheet.width);
      EXPECT_GE(playerRow.height, 44);
      EXPECT_LE(playerRow.bottom(), sheet.bottom());
    }
  }
}

TEST(GolfFairwayLayout, CounterThirdsAndChipHaveExactEdges) {
  constexpr fui::Rect row{7, 11, 480, 160};
  constexpr auto chip = golfui::fairwayChipRect(row);
  static_assert(chip.width == 80);
  static_assert(golfui::counterTapZoneAt(row, chip.x, row.y, true) == golfui::CounterTapZone::Fairway);
  for (const bool visible : {false, true}) {
    EXPECT_EQ(golfui::counterTapZoneAt(row, 166, 11, visible), golfui::CounterTapZone::Decrement);
    EXPECT_EQ(golfui::counterTapZoneAt(row, 167, 11, visible), golfui::CounterTapZone::Centre);
    EXPECT_EQ(golfui::counterTapZoneAt(row, 326, 11, visible), golfui::CounterTapZone::Centre);
    EXPECT_EQ(golfui::counterTapZoneAt(row, 327, 11, visible), golfui::CounterTapZone::Increment);
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.x - 1, 11, visible), golfui::CounterTapZone::Increment);
    const auto expected = visible ? golfui::CounterTapZone::Fairway : golfui::CounterTapZone::Increment;
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.x, 11, visible), expected);
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.right() - 1, row.bottom() - 1, visible), expected);
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.right(), 11, visible), golfui::CounterTapZone::None);
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.x, row.y - 1, visible), golfui::CounterTapZone::None);
    EXPECT_EQ(golfui::counterTapZoneAt(row, chip.x, row.bottom(), visible), golfui::CounterTapZone::None);
  }
}

TEST(GolfFairwayLayout, ChipLeavesCentreUntouchedAtEverySupportedWidthAndHeight) {
  for (const Case test : cases()) {
    const fui::Rect screen{0, 0, test.width, test.height};
    const auto safe =
        golfui::intersect(golfui::inset(screen, BEZEL), golfui::reserveHintEdge(screen, test.edge, HINT_RESERVE));
    const int rightThirdStart = safe.x + safe.width * 2 / 3;
    const int rightThird = safe.right() - rightThirdStart;
    ASSERT_GE(rightThird, 56) << "56 px floor must fit at the narrowest supported width";
    for (int16_t height = 1; height <= 1024; ++height) {
      const fui::Rect row{safe.x, safe.y, safe.width, height};
      const auto chip = golfui::fairwayChipRect(row);
      EXPECT_EQ(chip.width, golfui::maxValue(56, golfui::minValue(height, rightThird / 2)));
      EXPECT_GE(chip.x, rightThirdStart);
      EXPECT_EQ(chip.y, row.y);
      EXPECT_EQ(chip.height, row.height);
      EXPECT_EQ(chip.right(), row.right());
      EXPECT_EQ(golfui::counterTapZoneAt(row, rightThirdStart - 1, row.y, true), golfui::CounterTapZone::Centre);
    }
  }
}

TEST(GolfFairwayLayout, ChipTapKeepsSelectionRegardlessOfFocusedCounter) {
  constexpr fui::Rect row{0, 0, 480, 120};
  const auto chip = golfui::fairwayChipRect(row);
  const auto zone = golfui::counterTapZoneAt(row, chip.x, chip.y, true);
  ASSERT_EQ(zone, golfui::CounterTapZone::Fairway);
  for (uint8_t selected = 0; selected < 3; ++selected) {
    EXPECT_EQ(golfui::counterSelectionAfterTap(selected, 2, zone), selected);
    EXPECT_EQ(golfui::counterSelectionAfterTap(selected, 2, golfui::CounterTapZone::Centre), 2);
    EXPECT_EQ(golfui::counterSelectionAfterTap(selected, 2, golfui::CounterTapZone::Increment), 2);
  }
}

TEST(GolfFairwayLayout, PromptFooterAlwaysOffersSkipDoneHitMissed) {
  constexpr auto labels = golfui::fairwayPromptFooterLabels("Skip", "Done", "Hit", "Missed");
  static_assert(labels.previous[0] == 'H' && labels.next[0] == 'M');
  EXPECT_STREQ(labels.back, "Skip");
  EXPECT_STREQ(labels.confirm, "Done");
  EXPECT_STREQ(labels.previous, "Hit");
  EXPECT_STREQ(labels.next, "Missed");
  const auto layout = golfui::makeTouchFooterLayout({0, 0, 480, 800});
  constexpr golfui::FooterAction actions[] = {golfui::FooterAction::Back, golfui::FooterAction::Confirm,
                                              golfui::FooterAction::Previous, golfui::FooterAction::Next};
  for (uint8_t cell = 0; cell < 4; ++cell) {
    EXPECT_EQ(golfui::touchFooterActionAt(layout, labels, layout.cells[cell].x + 1, layout.cells[cell].y + 1),
              actions[cell]);
  }
}
