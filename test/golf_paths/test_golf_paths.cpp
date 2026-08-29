#include <gtest/gtest.h>

#include <cstring>

#include "GolfArchiveMarker.h"
#include "GolfPaths.h"

TEST(GolfPaths, SlugCollapsesSpacesAndPunctuation) {
  char slug[GOLF_SLUG_BUFFER_SIZE];
  ASSERT_TRUE(golfSlug("  Pebble -- Beach! Links  ", slug, sizeof(slug)));
  EXPECT_STREQ(slug, "pebble-beach-links");
}

TEST(GolfPaths, SlugTreatsAccentsAsSeparators) {
  char slug[GOLF_SLUG_BUFFER_SIZE];
  ASSERT_TRUE(golfSlug("Crème Brûlée", slug, sizeof(slug)));
  EXPECT_STREQ(slug, "cr-me-br-l-e");
}

TEST(GolfPaths, SlugTruncatesAtFortyCharactersWithoutTrailingSeparator) {
  char slug[GOLF_SLUG_BUFFER_SIZE];
  ASSERT_TRUE(golfSlug("ABCDEFGHIJKLMNOPQRSTUVWXYZ 12345678901234567890", slug, sizeof(slug)));
  EXPECT_EQ(strlen(slug), 40);
  EXPECT_STREQ(slug, "abcdefghijklmnopqrstuvwxyz-1234567890123");
}

TEST(GolfPaths, SlugUsesCourseFallbackWhenNoAlphanumericsExist) {
  char slug[GOLF_SLUG_BUFFER_SIZE];
  ASSERT_TRUE(golfSlug(" !!! ", slug, sizeof(slug)));
  EXPECT_STREQ(slug, "course");
}

TEST(GolfPaths, RejectsUndersizedOutputWithoutOverflow) {
  char slug[4] = {'x', 'x', 'x', '\0'};
  EXPECT_FALSE(golfSlug("Pebble Beach", slug, sizeof(slug)));
  EXPECT_STREQ(slug, "");
}

TEST(GolfPaths, FormatsBaseAndCollisionFilenames) {
  uint16_t date = 0;
  ASSERT_TRUE(golfParseDate("2026-08-29", date));
  char filename[GOLF_ROUND_FILENAME_BUFFER_SIZE];
  ASSERT_TRUE(golfRoundFilename(date, "Pebble Beach", 0, filename, sizeof(filename)));
  EXPECT_STREQ(filename, "2026-08-29-pebble-beach.json");
  ASSERT_TRUE(golfRoundFilename(date, "Pebble Beach", 2, filename, sizeof(filename)));
  EXPECT_STREQ(filename, "2026-08-29-pebble-beach-2.json");
}

TEST(GolfPaths, RejectsInvalidDates) {
  uint16_t date = 0;
  EXPECT_FALSE(golfParseDate("2026-02-29", date));
  EXPECT_TRUE(golfParseDate("2028-02-29", date));
}

TEST(GolfArchiveMarker, StateWithoutMarkerIsNotArchived) {
  const GolfArchiveMarker marker{};
  EXPECT_FALSE(isGolfArchiveMarked(marker));
  EXPECT_STREQ(golfArchivedFilename(marker), "");
}

TEST(GolfArchiveMarker, StateCarryingMarkerIsRecognisedAsArchived) {
  GolfArchiveMarker marker{};
  ASSERT_TRUE(setGolfArchiveMarker(marker, "2026-08-29-pebble-beach.json"));
  EXPECT_TRUE(isGolfArchiveMarked(marker));
  EXPECT_STREQ(golfArchivedFilename(marker), "2026-08-29-pebble-beach.json");
  clearGolfArchiveMarker(marker);
  EXPECT_FALSE(isGolfArchiveMarked(marker));
}
