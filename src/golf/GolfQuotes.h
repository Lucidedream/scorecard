#pragma once

#include <cstddef>
#include <cstdint>

inline constexpr size_t GOLF_QUOTE_TEXT_CAPACITY = 240;
inline constexpr size_t GOLF_QUOTE_AUTHOR_CAPACITY = 64;

struct GolfQuote {
  char text[GOLF_QUOTE_TEXT_CAPACITY]{};
  char author[GOLF_QUOTE_AUTHOR_CAPACITY]{};
  bool hasAuthor = false;
};

// Draws a uniformly random uint32 in [0, bound).
using GolfQuoteRandomFn = uint32_t (*)(uint32_t bound);

// Streams blank-line-separated quote records and retains one uniformly chosen
// record without loading the file or allocating from the heap.
class GolfQuoteReservoir {
 public:
  explicit GolfQuoteReservoir(GolfQuoteRandomFn random);

  void feed(const char* data, size_t length);
  void finish();

  bool hasPick() const { return recordCount_ > 0; }
  const GolfQuote& pick() const { return pick_; }

 private:
  char line_[GOLF_QUOTE_TEXT_CAPACITY]{};
  char recordText_[GOLF_QUOTE_TEXT_CAPACITY]{};
  char recordLastLine_[GOLF_QUOTE_TEXT_CAPACITY]{};
  GolfQuote pick_{};
  GolfQuoteRandomFn random_ = nullptr;
  size_t lineLength_ = 0;
  uint32_t recordCount_ = 0;
  bool recordHasLine_ = false;
  bool sawCarriage_ = false;
  bool lineOverflow_ = false;
  bool finished_ = false;

  void acceptLine();
  void closeRecord();
};
