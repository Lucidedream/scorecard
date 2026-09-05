#include "GolfTips.h"

#if defined(CROSSPOINT_GOLF)

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "GolfDirectoryScan.h"

namespace {

constexpr char TIPS_DIRECTORY[] = "/golf/tips";
constexpr size_t TIP_STREAM_CHUNK = 128;

bool hasTxtExtension(const char* filename) {
  const size_t length = std::strlen(filename);
  return length > 4 && std::strcmp(filename + length - 4, ".txt") == 0;
}

bool isSafeTipFilename(const char* filename) {
  return filename != nullptr && filename[0] != '\0' && std::strchr(filename, '/') == nullptr &&
         std::strchr(filename, '\\') == nullptr && hasTxtExtension(filename);
}

// Copies the filename without its ".txt" suffix, as the list title fallback for
// a note whose first line is empty.
void filenameStem(const char* filename, char* out, size_t size) {
  size_t length = std::strlen(filename);
  if (length > 4) length -= 4;
  if (length + 1 > size) length = size - 1;
  std::memcpy(out, filename, length);
  out[length] = '\0';
}

}  // namespace

GolfTipsListResult GolfTipsStore::enumerate(GolfTipEntry* files, uint8_t capacity) {
  GolfTipsListResult result{};
  if (!Storage.ensureDirectoryExists("/golf") || !Storage.ensureDirectoryExists(TIPS_DIRECTORY)) {
    LOG_ERR("GOLF", "Failed to create %s", TIPS_DIRECTORY);
    result.directoryError = true;
    return result;
  }
  if (capacity > GOLF_MAX_TIPS) capacity = GOLF_MAX_TIPS;

  HalFile directory = Storage.open(TIPS_DIRECTORY);
  if (!directory || !directory.isDirectory()) {
    LOG_ERR("GOLF", "Could not open %s", TIPS_DIRECTORY);
    result.directoryError = true;
    return result;
  }

  uint16_t txtFiles = 0;
  for (HalFile entry = directory.openNextFile(); entry; entry = directory.openNextFile()) {
    if (entry.isDirectory()) continue;
    char filename[GOLF_TIP_FILENAME_BUFFER_SIZE]{};
    if (entry.getName(filename, sizeof(filename)) == 0 || golfIsHiddenSidecarFilename(filename) ||
        !hasTxtExtension(filename))
      continue;
    ++txtFiles;
    if (result.count >= capacity) {
      result.overflow = true;
      continue;
    }

    // Count-only pass (files == nullptr): the home-tile detail needs a note
    // count and directory-error state, not per-file titles, so skip the reads.
    if (files == nullptr) {
      ++result.count;
      continue;
    }

    char path[sizeof(TIPS_DIRECTORY) + GOLF_TIP_FILENAME_BUFFER_SIZE + 1];
    if (snprintf(path, sizeof(path), "%s/%s", TIPS_DIRECTORY, filename) >= static_cast<int>(sizeof(path))) {
      result.fileError = true;
      continue;
    }
    HalFile file;
    if (!Storage.openFileForRead("GOLF", path, file)) {
      result.fileError = true;
      continue;
    }
    GolfTipScanner scanner;
    scanner.reset();
    bool readError = false;
    char chunk[TIP_STREAM_CHUNK];
    while (file.available() > 0) {
      const int bytesRead = file.read(chunk, sizeof(chunk));
      if (bytesRead <= 0) {
        readError = true;
        break;
      }
      scanner.feed(chunk, static_cast<size_t>(bytesRead));
    }
    scanner.finish();
    if (readError) {
      result.fileError = true;
      continue;
    }

    GolfTipEntry& target = files[result.count];
    target = {};
    std::memcpy(target.filename, filename, sizeof(filename));
    if (scanner.titleEmpty()) {
      filenameStem(filename, target.title, sizeof(target.title));
    } else {
      snprintf(target.title, sizeof(target.title), "%s", scanner.title());
    }
    target.sectionCount = scanner.sectionCount();
    ++result.count;
  }
  if (txtFiles > GOLF_MAX_TIPS) result.overflow = true;
  return result;
}

bool GolfTipsStore::readSection(const char* filename, const uint16_t sectionIndex, GolfTipSection& out) {
  out = {};
  if (!isSafeTipFilename(filename)) {
    LOG_ERR("GOLF", "Skipped tip: invalid filename");
    return false;
  }
  char path[sizeof(TIPS_DIRECTORY) + GOLF_TIP_FILENAME_BUFFER_SIZE + 1];
  if (snprintf(path, sizeof(path), "%s/%s", TIPS_DIRECTORY, filename) >= static_cast<int>(sizeof(path))) {
    LOG_ERR("GOLF", "Skipped tip %s: path too long", filename);
    return false;
  }
  HalFile file;
  if (!Storage.openFileForRead("GOLF", path, file)) {
    LOG_ERR("GOLF", "Could not read tip %s", filename);
    return false;
  }

  GolfTipSectionReader reader;
  reader.reset(out, sectionIndex);
  char chunk[TIP_STREAM_CHUNK];
  while (file.available() > 0) {
    const int bytesRead = file.read(chunk, sizeof(chunk));
    if (bytesRead <= 0) {
      LOG_ERR("GOLF", "Truncated read on tip %s", filename);
      return false;
    }
    reader.feed(chunk, static_cast<size_t>(bytesRead));
  }
  reader.finish();
  return true;
}

#endif  // CROSSPOINT_GOLF
