#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "GolfQuotes.h"

namespace {

uint32_t alwaysPick(uint32_t) { return 0; }

uint32_t keepFirst(const uint32_t bound) { return bound - 1; }

GolfQuote parse(const std::string& input, GolfQuoteRandomFn random, const size_t chunkSize) {
  GolfQuoteReservoir reservoir(random);
  for (size_t offset = 0; offset < input.size(); offset += chunkSize) {
    reservoir.feed(input.data() + offset, std::min(chunkSize, input.size() - offset));
  }
  reservoir.finish();
  EXPECT_TRUE(reservoir.hasPick());
  return reservoir.pick();
}

}  // namespace

TEST(golf_quotes, OneFeedAndArbitraryChunksParseIdentically) {
  const std::string input = "First quote.\nFirst Author\n\nSecond quote.\nSecond Author\n";
  const GolfQuote whole = parse(input, alwaysPick, input.size());
  const GolfQuote chunked = parse(input, alwaysPick, 3);
  EXPECT_STREQ(whole.text, "Second quote.");
  EXPECT_STREQ(whole.author, "Second Author");
  EXPECT_STREQ(chunked.text, whole.text);
  EXPECT_STREQ(chunked.author, whole.author);
  EXPECT_EQ(chunked.hasAuthor, whole.hasAuthor);
}

TEST(golf_quotes, FinishRecoversTrailingRecord) {
  const GolfQuote quote = parse("A trailing quote\nIts Author", alwaysPick, 5);
  EXPECT_STREQ(quote.text, "A trailing quote");
  EXPECT_STREQ(quote.author, "Its Author");
  EXPECT_TRUE(quote.hasAuthor);
}

TEST(golf_quotes, OneLineRecordHasNoAuthor) {
  const GolfQuote quote = parse("Anonymous wisdom\n", alwaysPick, 2);
  EXPECT_STREQ(quote.text, "Anonymous wisdom");
  EXPECT_STREQ(quote.author, "");
  EXPECT_FALSE(quote.hasAuthor);
}

TEST(golf_quotes, JoinsMultilineQuoteAndTrimsWhitespace) {
  const GolfQuote quote = parse("  Play the shot  \n\tyou have today.\t\n  Annika Sorenstam  \n", alwaysPick, 7);
  EXPECT_STREQ(quote.text, "Play the shot you have today.");
  EXPECT_STREQ(quote.author, "Annika Sorenstam");
}

TEST(golf_quotes, AcceptsCrLfAndLf) {
  const std::string lf = "First line\nsecond line\nAuthor\n";
  const std::string crlf = "First line\r\nsecond line\r\nAuthor\r\n";
  const GolfQuote left = parse(lf, alwaysPick, 4);
  const GolfQuote right = parse(crlf, alwaysPick, 4);
  EXPECT_STREQ(left.text, right.text);
  EXPECT_STREQ(left.author, right.author);
}

TEST(golf_quotes, LongFieldsAreTruncatedAndTerminated) {
  const std::string longQuote(GOLF_QUOTE_TEXT_CAPACITY + 80, 'q');
  const std::string longAuthor(GOLF_QUOTE_AUTHOR_CAPACITY + 80, 'a');
  const GolfQuote quote = parse(longQuote + "\n" + longAuthor + "\n", alwaysPick, 11);
  EXPECT_EQ(std::strlen(quote.text), GOLF_QUOTE_TEXT_CAPACITY - 1);
  EXPECT_EQ(std::strlen(quote.author), GOLF_QUOTE_AUTHOR_CAPACITY - 1);
  EXPECT_EQ(quote.text[GOLF_QUOTE_TEXT_CAPACITY - 1], '\0');
  EXPECT_EQ(quote.author[GOLF_QUOTE_AUTHOR_CAPACITY - 1], '\0');
}

TEST(golf_quotes, AlwaysPickRetainsLastRecord) {
  const GolfQuote quote = parse("First\nA\n\nMiddle\nB\n\nLast\nC\n", alwaysPick, 6);
  EXPECT_STREQ(quote.text, "Last");
  EXPECT_STREQ(quote.author, "C");
}

TEST(golf_quotes, NonzeroDrawAfterFirstRetainsFirstRecord) {
  const GolfQuote quote = parse("First\nA\n\nMiddle\nB\n\nLast\nC\n", keepFirst, 8);
  EXPECT_STREQ(quote.text, "First");
  EXPECT_STREQ(quote.author, "A");
}

TEST(golf_quotes, EmptyInputHasNoPick) {
  GolfQuoteReservoir reservoir(alwaysPick);
  reservoir.finish();
  EXPECT_FALSE(reservoir.hasPick());
}
