#pragma once

#include <cstddef>
#include <cstdint>

// Tips (CONTRACTS-V2 §25). Notes are plain text files at /golf/tips/*.txt that
// the owner writes on a computer and drops on the SD card. The format has four
// rules:
//   1. the first line of the file is the note TITLE (shown in the list)
//   2. a BLANK LINE starts a new section; each section is one screen
//   3. a section's first line is its HEADING
//   4. lines starting '*' (U+2022 bullet) or '-' are bullets; anything else is
//      a plain paragraph line
//
// Nothing here loads a whole file. The list scanner and the section reader both
// consume the file as a byte stream and keep only a single line plus, for the
// reader, a single materialised section in RAM (CONTRACTS-V2 §25.2).

inline constexpr uint8_t GOLF_MAX_TIPS = 24;
inline constexpr size_t GOLF_TIP_FILENAME_BUFFER_SIZE = 48;
inline constexpr size_t GOLF_TIP_TITLE_BUFFER_SIZE = 56;

// The longest source line the parsers reconstruct. Longer lines are clipped for
// display but still classified (blank vs not) and counted correctly.
inline constexpr size_t GOLF_TIP_LINE_BUFFER_SIZE = 128;

// One materialised section. A section that a note author overfills past these
// bounds is MARKED (`overflow`), never silently dropped (CONTRACTS-V2 §25.2).
inline constexpr size_t GOLF_TIP_HEADING_BUFFER_SIZE = 128;
inline constexpr uint8_t GOLF_TIP_SECTION_MAX_LINES = 10;
inline constexpr size_t GOLF_TIP_BODY_LINE_BUFFER_SIZE = 120;

enum class GolfTipLineKind : uint8_t { Paragraph, Bullet };

// Plain aggregates (like GolfHistoryEntry): zero-initialise with `= {}`.
struct GolfTipSectionLine {
  GolfTipLineKind kind;
  char text[GOLF_TIP_BODY_LINE_BUFFER_SIZE];
};

struct GolfTipSection {
  char heading[GOLF_TIP_HEADING_BUFFER_SIZE];
  GolfTipSectionLine lines[GOLF_TIP_SECTION_MAX_LINES];
  uint8_t lineCount;
  uint16_t index;  // 0-based index of the section this holds
  uint16_t count;  // total sections in the note
  bool found;      // the requested section exists and its heading was read
  bool overflow;   // the section had more body lines than fit the store
};

// True when a line is empty or only whitespace. A blank line is the section
// delimiter.
bool golfTipLineIsBlank(const char* line);

// Classifies one already-trimmed-of-newline line. `textStart` is advanced past a
// leading bullet marker ('*' as U+2022, or '-') and one optional following space.
GolfTipLineKind golfTipClassifyLine(const char* line, const char** textStart);

// Streams a note file to extract its title (line 1) and section count, without
// holding more than one line. Used to build the note list and the detail counts.
class GolfTipScanner {
 public:
  void reset();
  void feed(const char* data, size_t size);
  void finish();

  const char* title() const { return title_; }
  bool titleEmpty() const { return title_[0] == '\0'; }
  uint16_t sectionCount() const { return sectionCount_; }

 private:
  char line_[GOLF_TIP_LINE_BUFFER_SIZE] = {};
  char title_[GOLF_TIP_TITLE_BUFFER_SIZE] = {};
  uint16_t lineLength_ = 0;
  uint32_t lineNumber_ = 0;
  uint16_t sectionCount_ = 0;
  bool inSection_ = false;
  bool sawCarriage_ = false;
  bool lineOverflow_ = false;

  void acceptLine();
};

// Streams a note file and materialises exactly one section (by index) into a
// caller-owned GolfTipSection. Sections before and after the target are walked
// but not stored, so total section count is still known at finish().
class GolfTipSectionReader {
 public:
  // `out` is cleared and borrowed until finish(). `targetIndex` is 0-based.
  void reset(GolfTipSection& out, uint16_t targetIndex);
  void feed(const char* data, size_t size);
  void finish();

 private:
  char line_[GOLF_TIP_LINE_BUFFER_SIZE] = {};
  GolfTipSection* out_ = nullptr;
  uint16_t targetIndex_ = 0;
  uint16_t lineLength_ = 0;
  uint32_t lineNumber_ = 0;
  uint16_t sectionNumber_ = 0;  // 1-based count of sections started so far
  bool inSection_ = false;
  bool capturing_ = false;
  bool headingCaptured_ = false;
  bool sawCarriage_ = false;
  bool lineOverflow_ = false;

  void acceptLine();
};

// The note list has three distinct states, kept apart the way §22.4 keeps the
// player picker's: a readable but empty /golf/tips is not the same as one that
// could not be read.
enum class GolfTipsListState : uint8_t { Ready, Empty, Error };

constexpr GolfTipsListState golfTipsListState(const bool directoryError, const bool fileError,
                                              const uint8_t noteCount) {
  if (directoryError) return GolfTipsListState::Error;
  if (noteCount > 0) return GolfTipsListState::Ready;
  return fileError ? GolfTipsListState::Error : GolfTipsListState::Empty;
}

struct GolfTipEntry {
  char filename[GOLF_TIP_FILENAME_BUFFER_SIZE];
  char title[GOLF_TIP_TITLE_BUFFER_SIZE];
  uint16_t sectionCount;
};

struct GolfTipsListResult {
  uint8_t count = 0;
  bool overflow = false;        // more than `capacity` / GOLF_MAX_TIPS notes present
  bool directoryError = false;  // /golf/tips could not be created, opened or read
  bool fileError = false;       // at least one .txt entry could not be opened or read
};

// Scans /golf/tips the way CourseStore scans /golf/courses: one HalFile stream
// at a time, never the whole directory or a whole file in RAM.
class GolfTipsStore {
 public:
  // With `files == nullptr` this is a cheap count-and-error probe (no file
  // reads) for the home-tile detail; with a real buffer it also fills each
  // entry's title and section count for the note list.
  static GolfTipsListResult enumerate(GolfTipEntry* files, uint8_t capacity);
  // Reads section `sectionIndex` of the note `filename` (relative to /golf/tips)
  // into `out`. Returns false only on an I/O or filename error; a missing
  // section is reported through `out.found`.
  static bool readSection(const char* filename, uint16_t sectionIndex, GolfTipSection& out);
};
