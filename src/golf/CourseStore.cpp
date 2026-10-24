#include "CourseStore.h"

#if defined(CROSSPOINT_GOLF)

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <PersistableStore.h>

#include <cstdio>
#include <cstring>

#include "CourseBuiltIns.h"
#include "CourseOrder.h"
#include "GolfCourseParse.h"
#include "GolfDirectoryScan.h"

namespace {

constexpr char COURSE_DIRECTORY[] = "/golf/courses";
constexpr char ROUNDS_DIRECTORY[] = "/golf/rounds";

bool hasJsonExtension(const char* filename) {
  const size_t length = strlen(filename);
  return length > 5 && strcmp(filename + length - 5, ".json") == 0;
}

bool isSafeFilename(const char* filename) {
  return filename != nullptr && filename[0] != '\0' && strchr(filename, '/') == nullptr &&
         strchr(filename, '\\') == nullptr && hasJsonExtension(filename);
}

void logValidationFailure(const char* filename, const GolfCourseValidationResult& validation) {
  LOG_ERR("GOLF", "Skipped course %s: %s (%s hole %u, value %u, expected %u)", filename,
          golfCourseValidationErrorName(validation.error), golfCourseFieldName(validation.field), validation.hole + 1,
          validation.value, validation.expected);
}

}  // namespace

GolfCourseListResult CourseStore::enumerate(GolfCourseFile* files, uint8_t capacity) {
  GolfCourseListResult result{};
  capacity = capacity > GOLF_MAX_COURSES ? GOLF_MAX_COURSES : capacity;
  // Reuse one large course and bounded names across the directory scan.
  auto scratch = makeUniqueNoThrow<GolfCourse>();
  auto names = makeUniqueNoThrow<char[][sizeof(GolfCourse::courseName)]>(GOLF_MAX_COURSES);
  if (!scratch || !names) {
    LOG_ERR("GOLF", "OOM: course enumeration scratch");
    return result;
  }
  bool overridden[GOLF_BUILT_IN_COURSE_COUNT]{};
  if (!Storage.ensureDirectoryExists("/golf") || !Storage.ensureDirectoryExists(COURSE_DIRECTORY) ||
      !Storage.ensureDirectoryExists(ROUNDS_DIRECTORY)) {
    LOG_ERR("GOLF", "Failed to create golf directories");
    return result;
  }
  // SD files sort by filename; the lexicographically first file wins duplicate names.
  // Built-ins fill free slots after SD files, retaining any overridden table slots.
  uint16_t jsonFiles = 0;
  HalFile directory = Storage.open(COURSE_DIRECTORY);
  for (HalFile entry = directory && directory.isDirectory() ? directory.openNextFile() : HalFile{}; entry;
       entry = directory.openNextFile()) {
    if (entry.isDirectory()) {
      continue;
    }
    char filename[GOLF_COURSE_FILENAME_BUFFER_SIZE]{};
    if (!entry.getName(filename, sizeof(filename)) || golfIsHiddenSidecarFilename(filename) ||
        !hasJsonExtension(filename)) {
      continue;
    }
    if (++jsonFiles > GOLF_MAX_COURSES) {
      result.overflow = true;
    }
    if (!load(filename, *scratch)) {
      continue;
    }
    uint8_t duplicate = result.count;
    for (uint8_t i = 0; i < result.count; ++i) {
      if (strcmp(names[i], scratch->courseName) == 0) {
        duplicate = i;
        break;
      }
    }
    if (duplicate < result.count) {
      if (strcmp(files[duplicate].filename, filename) < 0) {
        LOG_ERR("GOLF", "Duplicate course name %s in %s", scratch->courseName, filename);
        continue;
      }
      LOG_ERR("GOLF", "Duplicate course name %s in %s", scratch->courseName, files[duplicate].filename);
      for (uint8_t i = duplicate + 1; i < result.count; ++i) {
        files[i - 1] = files[i];
        memcpy(names[i - 1], names[i], sizeof(GolfCourse::courseName));
      }
      --result.count;
    }
    uint8_t position = 0;
    while (position < result.count && strcmp(files[position].filename, filename) < 0) {
      ++position;
    }
    if (!files || !capacity || position >= capacity) {
      result.overflow = true;
      continue;
    }
    if (result.count < capacity) {
      ++result.count;
    } else {
      result.overflow = true;
    }
    for (uint8_t i = result.count - 1; i > position; --i) {
      files[i] = files[i - 1];
      memcpy(names[i], names[i - 1], sizeof(GolfCourse::courseName));
    }
    files[position] = {};
    strcpy(files[position].filename, filename);
    strcpy(names[position], scratch->courseName);
    for (uint8_t i = 0; i < GOLF_BUILT_IN_COURSE_COUNT; ++i) {
      if (strcmp(scratch->courseName, GOLF_BUILT_IN_COURSES[i].courseName) == 0) {
        files[position].builtInIndex = i;
      }
    }
  }
  for (uint8_t i = 0; i < result.count; ++i) {
    if (files[i].builtInIndex >= 0) {
      overridden[files[i].builtInIndex] = true;
    }
  }
  for (uint8_t i = 0; i < GOLF_BUILT_IN_COURSE_COUNT; ++i) {
    if (overridden[i]) {
      continue;
    }
    if (files && result.count < capacity) {
      files[result.count] = {};
      files[result.count++].builtInIndex = i;
    } else {
      result.overflow = true;
    }
  }
  if (result.overflow) {
    LOG_ERR("GOLF", "More than %u course entries found; list is bounded", GOLF_MAX_COURSES);
  }
  return result;
}

bool CourseStore::load(const char* filename, GolfCourse& course) {
  if (!isSafeFilename(filename)) {
    LOG_ERR("GOLF", "Skipped course: invalid filename");
    return false;
  }
  char path[sizeof(COURSE_DIRECTORY) + GOLF_COURSE_FILENAME_BUFFER_SIZE + 1];
  const int pathLength = snprintf(path, sizeof(path), "%s/%s", COURSE_DIRECTORY, filename);
  if (pathLength < 0 || static_cast<size_t>(pathLength) >= sizeof(path)) {
    LOG_ERR("GOLF", "Skipped course %s: path is too long", filename);
    return false;
  }
  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(path, doc)) {
    LOG_ERR("GOLF", "Skipped course %s: could not read JSON", filename);
    return false;
  }

  const auto validation = golfParseCourseJson(doc.as<JsonVariantConst>(), course);
  if (!validation.valid) {
    if (doc["v"].is<int>() && doc["v"].as<int>() == 1) {
      LOG_ERR(
          "GOLF",
          "Skipped course %s: v1 course files are no longer supported; convert with scripts/golf_convert_courses_v2.py",
          filename);
    } else
      logValidationFailure(filename, validation);
    return false;
  }
  return true;
}

bool CourseStore::load(const GolfCourseFile& file, GolfCourse& course) {
  // A non-empty filename means an SD file: load it even when builtInIndex is set, since
  // that index only records which built-in slot the override inherits (CONTRACTS-V2 §7.1).
  if (file.filename[0] != '\0') {
    return load(file.filename, course);
  }
  if (file.builtInIndex >= 0 && file.builtInIndex < GOLF_BUILT_IN_COURSE_COUNT) {
    course = GOLF_BUILT_IN_COURSES[file.builtInIndex];
    return true;
  }
  return false;
}

bool CourseStore::findByName(const char* name, GolfCourseFile& file, GolfCourse& course) {
  if (!name || !name[0]) return false;
  auto files = makeUniqueNoThrow<GolfCourseFile[]>(GOLF_MAX_COURSES);
  auto scratch = makeUniqueNoThrow<GolfCourse>();
  if (!files || !scratch) {
    LOG_ERR("GOLF", "OOM: find course scratch");
    return false;
  }
  auto result = enumerate(files.get(), GOLF_MAX_COURSES);
  for (uint8_t i = 0; i < result.count; ++i)
    if (load(files[i], *scratch) && strcmp(name, scratch->courseName) == 0) {
      file = files[i];
      course = *scratch;
      return true;
    }
  return false;
}
#endif
