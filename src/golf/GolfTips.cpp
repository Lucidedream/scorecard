#include "GolfTips.h"

#if defined(CROSSPOINT_GOLF)

#include <cstdint>
#include <cstring>

namespace {

constexpr char BULLET_UTF8[] = "\xE2\x80\xA2";  // U+2022

bool isSpace(const char value) { return value == ' ' || value == '\t'; }

// Copies `src` into `dst` (capacity `size`), NUL-terminating. If the copy is
// truncated, any trailing partial UTF-8 sequence is dropped so the clipped
// string never ends mid-character.
void clipCopy(char* dst, const size_t size, const char* src) {
  if (size == 0) return;
  size_t written = 0;
  while (src[written] != '\0' && written + 1 < size) {
    dst[written] = src[written];
    ++written;
  }
  const bool truncated = src[written] != '\0';
  if (truncated) {
    while (written > 0 && (static_cast<unsigned char>(dst[written - 1]) & 0xC0) == 0x80) --written;
    if (written > 0 && (static_cast<unsigned char>(dst[written - 1]) & 0x80) != 0) --written;
  }
  dst[written] = '\0';
}

}  // namespace

bool golfTipLineIsBlank(const char* line) {
  for (const char* cursor = line; *cursor != '\0'; ++cursor) {
    if (!isSpace(*cursor)) return false;
  }
  return true;
}

GolfTipLineKind golfTipClassifyLine(const char* line, const char** textStart) {
  const char* cursor = line;
  while (isSpace(*cursor)) ++cursor;

  if (std::memcmp(cursor, BULLET_UTF8, 3) == 0) {
    cursor += 3;
    if (*cursor == ' ') ++cursor;
    *textStart = cursor;
    return GolfTipLineKind::Bullet;
  }
  if (*cursor == '-') {
    ++cursor;
    if (*cursor == ' ') ++cursor;
    *textStart = cursor;
    return GolfTipLineKind::Bullet;
  }
  *textStart = cursor;
  return GolfTipLineKind::Paragraph;
}

// Both readers below split a byte stream on LF, CRLF or a lone CR, keeping at
// most one line (clipped at GOLF_TIP_LINE_BUFFER_SIZE) in RAM. `GolfTipLineSink`
// captures the per-class acceptLine() so the loop lives in one place without a
// template or std::function.
namespace {

struct GolfTipLineSink {
  void (*accept)(void*) = nullptr;
  void* context = nullptr;
};

void feedLines(const char* data, size_t size, char* line, uint16_t& lineLength, bool& sawCarriage, bool& lineOverflow,
               const GolfTipLineSink& sink) {
  for (size_t index = 0; index < size; ++index) {
    const char byte = data[index];
    if (byte == '\r') {
      sawCarriage = true;
      continue;
    }
    const bool newline = byte == '\n';
    if (newline || sawCarriage) {
      line[lineLength] = '\0';
      sink.accept(sink.context);
      lineLength = 0;
      lineOverflow = false;
      sawCarriage = false;
      if (newline) continue;
    }
    if (lineLength + 1u < GOLF_TIP_LINE_BUFFER_SIZE) {
      line[lineLength++] = byte;
    } else {
      lineOverflow = true;
    }
  }
}

void flushPartial(char* line, uint16_t& lineLength, bool& lineOverflow, const GolfTipLineSink& sink) {
  if (lineLength == 0) return;
  line[lineLength] = '\0';
  sink.accept(sink.context);
  lineLength = 0;
  lineOverflow = false;
}

}  // namespace

// --- GolfTipScanner --------------------------------------------------------

void GolfTipScanner::reset() {
  line_[0] = '\0';
  title_[0] = '\0';
  lineLength_ = 0;
  lineNumber_ = 0;
  sectionCount_ = 0;
  inSection_ = false;
  sawCarriage_ = false;
  lineOverflow_ = false;
}

void GolfTipScanner::acceptLine() {
  if (lineNumber_ == 0) {
    clipCopy(title_, sizeof(title_), line_);
    lineNumber_ = 1;
    return;
  }
  const bool blank = !lineOverflow_ && golfTipLineIsBlank(line_);
  if (blank) {
    inSection_ = false;
  } else if (!inSection_) {
    inSection_ = true;
    if (sectionCount_ < UINT16_MAX) ++sectionCount_;
  }
}

void GolfTipScanner::feed(const char* data, const size_t size) {
  const GolfTipLineSink sink{[](void* self) { static_cast<GolfTipScanner*>(self)->acceptLine(); }, this};
  feedLines(data, size, line_, lineLength_, sawCarriage_, lineOverflow_, sink);
}

void GolfTipScanner::finish() {
  const GolfTipLineSink sink{[](void* self) { static_cast<GolfTipScanner*>(self)->acceptLine(); }, this};
  flushPartial(line_, lineLength_, lineOverflow_, sink);
}

// --- GolfTipSectionReader -------------------------------------------------

void GolfTipSectionReader::reset(GolfTipSection& out, const uint16_t targetIndex) {
  out = {};
  out_ = &out;
  targetIndex_ = targetIndex;
  line_[0] = '\0';
  lineLength_ = 0;
  lineNumber_ = 0;
  sectionNumber_ = 0;
  inSection_ = false;
  capturing_ = false;
  headingCaptured_ = false;
  sawCarriage_ = false;
  lineOverflow_ = false;
}

void GolfTipSectionReader::acceptLine() {
  if (out_ == nullptr) return;
  if (lineNumber_ == 0) {  // the title line belongs to no section
    lineNumber_ = 1;
    return;
  }

  const bool blank = !lineOverflow_ && golfTipLineIsBlank(line_);
  if (blank) {
    inSection_ = false;
    capturing_ = false;
    return;
  }

  if (!inSection_) {
    inSection_ = true;
    if (sectionNumber_ < UINT16_MAX) ++sectionNumber_;
    capturing_ = sectionNumber_ - 1u == targetIndex_;
    if (capturing_) headingCaptured_ = false;
  }
  if (!capturing_) return;

  if (!headingCaptured_) {
    const char* cursor = line_;
    while (isSpace(*cursor)) ++cursor;
    clipCopy(out_->heading, sizeof(out_->heading), cursor);
    headingCaptured_ = true;
    out_->found = true;
    return;
  }
  if (out_->lineCount >= GOLF_TIP_SECTION_MAX_LINES) {
    out_->overflow = true;
    return;
  }
  const char* text = nullptr;
  const GolfTipLineKind kind = golfTipClassifyLine(line_, &text);
  GolfTipSectionLine& target = out_->lines[out_->lineCount++];
  target.kind = kind;
  clipCopy(target.text, sizeof(target.text), text);
}

void GolfTipSectionReader::feed(const char* data, const size_t size) {
  const GolfTipLineSink sink{[](void* self) { static_cast<GolfTipSectionReader*>(self)->acceptLine(); }, this};
  feedLines(data, size, line_, lineLength_, sawCarriage_, lineOverflow_, sink);
}

void GolfTipSectionReader::finish() {
  const GolfTipLineSink sink{[](void* self) { static_cast<GolfTipSectionReader*>(self)->acceptLine(); }, this};
  flushPartial(line_, lineLength_, lineOverflow_, sink);
  if (out_ == nullptr) return;
  out_->index = targetIndex_;
  out_->count = sectionNumber_;
}

#endif  // CROSSPOINT_GOLF
