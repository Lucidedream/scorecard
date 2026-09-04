#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "GolfTips.h"

namespace {

std::string fixture(const char* name) {
  std::string path = std::string(GOLF_TIPS_FIXTURES_DIR) + "/" + name;
  FILE* file = std::fopen(path.c_str(), "rb");
  EXPECT_NE(file, nullptr) << "missing fixture " << path;
  std::string data;
  if (file != nullptr) {
    char buffer[512];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) data.append(buffer, read);
    std::fclose(file);
  }
  return data;
}

// Feeds text through a streaming reader in small, uneven chunks so a bug that
// only shows up across a chunk boundary is caught.
template <typename Reader>
void feedChunked(Reader& reader, const std::string& text, size_t chunk) {
  for (size_t offset = 0; offset < text.size(); offset += chunk) {
    reader.feed(text.data() + offset, std::min(chunk, text.size() - offset));
  }
  reader.finish();
}

GolfTipSection readSection(const std::string& text, uint16_t index, size_t chunk = 7) {
  GolfTipSection section;
  GolfTipSectionReader reader;
  reader.reset(section, index);
  feedChunked(reader, text, chunk);
  return section;
}

uint8_t bulletCount(const GolfTipSection& section) {
  uint8_t bullets = 0;
  for (uint8_t line = 0; line < section.lineCount; ++line) {
    if (section.lines[line].kind == GolfTipLineKind::Bullet) ++bullets;
  }
  return bullets;
}

}  // namespace

TEST(GolfTips, ParsesSlopeStrategyIntoNineSections) {
  const std::string text = fixture("slope-strategy.txt");

  GolfTipScanner scanner;
  scanner.reset();
  feedChunked(scanner, text, 13);

  EXPECT_STREQ(scanner.title(), "Slope Strategy");
  EXPECT_EQ(scanner.sectionCount(), 9);
}

TEST(GolfTips, TitleIsTheFirstLine) {
  GolfTipScanner scanner;
  scanner.reset();
  const std::string text = "Wind Adjustments\n\nInto the wind\n- Club up\n";
  feedChunked(scanner, text, 5);
  EXPECT_STREQ(scanner.title(), "Wind Adjustments");
  EXPECT_FALSE(scanner.titleEmpty());
  EXPECT_EQ(scanner.sectionCount(), 1);
}

TEST(GolfTips, SlopeStrategySectionBulletCounts) {
  const std::string text = fixture("slope-strategy.txt");
  const uint8_t expected[9] = {5, 4, 5, 4, 4, 4, 4, 4, 3};
  uint8_t total = 0;
  for (uint16_t index = 0; index < 9; ++index) {
    const GolfTipSection section = readSection(text, index);
    EXPECT_TRUE(section.found) << "section " << index;
    EXPECT_EQ(section.count, 9);
    EXPECT_FALSE(section.overflow) << "section " << index;
    EXPECT_EQ(bulletCount(section), expected[index]) << "section " << index;
    EXPECT_EQ(section.lineCount, expected[index]) << "section " << index;
    total = static_cast<uint8_t>(total + bulletCount(section));
  }
  EXPECT_EQ(total, 37);
}

TEST(GolfTips, MaterialisesHeadingAndBullets) {
  const std::string text = fixture("slope-strategy.txt");
  const GolfTipSection section = readSection(text, 2);  // "Downhill lie ..." — 5 bullets
  EXPECT_STREQ(section.heading, "Downhill lie - low/fade bias");
  ASSERT_EQ(section.lineCount, 5);
  EXPECT_EQ(section.lines[0].kind, GolfTipLineKind::Bullet);
  EXPECT_STREQ(section.lines[0].text, "Ball slightly back");
  EXPECT_STREQ(section.lines[4].text, "Don't force an inside-out path; it risks a heavy strike");
}

TEST(GolfTips, MissingSectionIsNotFound) {
  const std::string text = fixture("slope-strategy.txt");
  const GolfTipSection section = readSection(text, 9);
  EXPECT_FALSE(section.found);
  EXPECT_EQ(section.count, 9);
  EXPECT_EQ(section.lineCount, 0);
}

TEST(GolfTips, OverflowingSectionIsMarkedNotTruncatedSilently) {
  std::string text = "Big Note\n\nToo much\n";
  for (int bullet = 0; bullet < GOLF_TIP_SECTION_MAX_LINES + 5; ++bullet) {
    text += "- bullet " + std::to_string(bullet) + "\n";
  }
  const GolfTipSection section = readSection(text, 0);
  EXPECT_TRUE(section.found);
  EXPECT_TRUE(section.overflow);
  EXPECT_EQ(section.lineCount, GOLF_TIP_SECTION_MAX_LINES);
  EXPECT_STREQ(section.lines[0].text, "bullet 0");
  EXPECT_STREQ(section.lines[GOLF_TIP_SECTION_MAX_LINES - 1].text, "bullet 9");
}

TEST(GolfTips, DashBulletsAndParagraphsAreDistinguished) {
  const std::string text =
      "N\n\nHead\nA plain paragraph line\n- a dash bullet\n"
      "\xE2\x80\xA2"
      " a real bullet\n";
  const GolfTipSection section = readSection(text, 0);
  ASSERT_EQ(section.lineCount, 3);
  EXPECT_EQ(section.lines[0].kind, GolfTipLineKind::Paragraph);
  EXPECT_STREQ(section.lines[0].text, "A plain paragraph line");
  EXPECT_EQ(section.lines[1].kind, GolfTipLineKind::Bullet);
  EXPECT_STREQ(section.lines[1].text, "a dash bullet");
  EXPECT_EQ(section.lines[2].kind, GolfTipLineKind::Bullet);
  EXPECT_STREQ(section.lines[2].text, "a real bullet");
}

TEST(GolfTips, MultipleBlankLinesDoNotCreateEmptySections) {
  GolfTipScanner scanner;
  scanner.reset();
  feedChunked(scanner, "Title\n\n\n\nOne\n\n\n\nTwo\n", 3);
  EXPECT_EQ(scanner.sectionCount(), 2);
}

TEST(GolfTips, CrlfLineEndingsParseTheSame) {
  GolfTipScanner scanner;
  scanner.reset();
  feedChunked(scanner,
              "Title\r\n\r\nOne\r\n"
              "\xE2\x80\xA2"
              " a\r\n\r\nTwo\r\n",
              4);
  EXPECT_STREQ(scanner.title(), "Title");
  EXPECT_EQ(scanner.sectionCount(), 2);
}

TEST(GolfTips, EmptyTipsFolderIsDistinctFromUnreadable) {
  EXPECT_EQ(golfTipsListState(false, false, 0), GolfTipsListState::Empty);
  EXPECT_EQ(golfTipsListState(false, false, 3), GolfTipsListState::Ready);
  EXPECT_EQ(golfTipsListState(true, false, 0), GolfTipsListState::Error);
  EXPECT_EQ(golfTipsListState(false, true, 0), GolfTipsListState::Error);
  // A folder with readable notes stays Ready even if one sibling file failed.
  EXPECT_EQ(golfTipsListState(false, true, 2), GolfTipsListState::Ready);
}
