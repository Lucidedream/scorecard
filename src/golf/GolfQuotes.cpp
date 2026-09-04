#include "GolfQuotes.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

namespace {

bool isSpace(const char value) { return value == ' ' || value == '\t'; }

void clipCopy(char* destination, const size_t capacity, const char* source) {
  if (capacity == 0) return;
  size_t length = std::strlen(source);
  if (length >= capacity) length = capacity - 1;
  std::memcpy(destination, source, length);
  destination[length] = '\0';
}

void appendLine(char* destination, const size_t capacity, const char* line) {
  const size_t used = std::strlen(destination);
  if (used + 1 >= capacity) return;
  size_t cursor = used;
  if (cursor > 0) destination[cursor++] = ' ';
  size_t remaining = capacity - cursor - 1;
  size_t length = std::strlen(line);
  if (length > remaining) length = remaining;
  std::memcpy(destination + cursor, line, length);
  destination[cursor + length] = '\0';
}

}  // namespace

GolfQuoteReservoir::GolfQuoteReservoir(GolfQuoteRandomFn random) : random_(random) {}

void GolfQuoteReservoir::acceptLine() {
  line_[lineLength_] = '\0';
  char* start = line_;
  while (isSpace(*start)) ++start;
  char* end = line_ + lineLength_;
  while (end > start && isSpace(end[-1])) --end;
  *end = '\0';

  if (*start == '\0') {
    closeRecord();
    return;
  }

  if (recordHasLine_) appendLine(recordText_, sizeof(recordText_), recordLastLine_);
  clipCopy(recordLastLine_, sizeof(recordLastLine_), start);
  recordHasLine_ = true;
}

void GolfQuoteReservoir::closeRecord() {
  if (!recordHasLine_) return;

  if (recordCount_ < UINT32_MAX) ++recordCount_;
  const bool selected = random_ != nullptr ? random_(recordCount_) == 0 : recordCount_ == 1;
  if (selected) {
    pick_ = {};
    if (recordText_[0] == '\0') {
      clipCopy(pick_.text, sizeof(pick_.text), recordLastLine_);
    } else {
      clipCopy(pick_.text, sizeof(pick_.text), recordText_);
      clipCopy(pick_.author, sizeof(pick_.author), recordLastLine_);
      pick_.hasAuthor = pick_.author[0] != '\0';
    }
  }

  recordText_[0] = '\0';
  recordLastLine_[0] = '\0';
  recordHasLine_ = false;
}

void GolfQuoteReservoir::feed(const char* data, const size_t length) {
  if (finished_) return;
  for (size_t index = 0; index < length; ++index) {
    const char byte = data[index];
    if (sawCarriage_) {
      sawCarriage_ = false;
      if (byte == '\n') continue;
    }
    if (byte == '\r' || byte == '\n') {
      acceptLine();
      lineLength_ = 0;
      lineOverflow_ = false;
      sawCarriage_ = byte == '\r';
      continue;
    }
    if (lineLength_ + 1 < sizeof(line_)) {
      line_[lineLength_++] = byte;
    } else {
      lineOverflow_ = true;
    }
  }
}

void GolfQuoteReservoir::finish() {
  if (finished_) return;
  if (lineLength_ > 0 || lineOverflow_) acceptLine();
  closeRecord();
  finished_ = true;
}

#endif  // CROSSPOINT_GOLF
