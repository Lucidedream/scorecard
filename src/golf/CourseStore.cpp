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
#include "GolfCourseValidate.h"

namespace {

constexpr char COURSE_DIRECTORY[] = "/golf/courses";
constexpr char ROUNDS_DIRECTORY[] = "/golf/rounds";

bool copyString(JsonVariantConst value, char* output, size_t capacity, bool required) {
  if (value.isNull() && !required) {
    output[0] = '\0';
    return true;
  }
  if (!value.is<const char*>()) {
    return false;
  }
  const char* source = value.as<const char*>();
  const size_t length = strlen(source);
  if ((required && length == 0) || length >= capacity || strpbrk(source, "\r\n") != nullptr) {
    return false;
  }
  memcpy(output, source, length + 1);
  return true;
}

template <typename T>
bool readArray(JsonVariantConst value, T* output, uint16_t& length, uint16_t maximum) {
  const JsonArrayConst array = value.as<JsonArrayConst>();
  if (array.isNull()) {
    length = 0;
    return false;
  }
  length = array.size() > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(array.size());
  const uint16_t readCount = length < GolfRound::MAX_HOLES ? length : GolfRound::MAX_HOLES;
  for (uint16_t hole = 0; hole < readCount; ++hole) {
    if (!array[hole].is<int>()) {
      return false;
    }
    const int item = array[hole].as<int>();
    if (item < 0 || item > maximum) {
      return false;
    }
    output[hole] = static_cast<T>(item);
  }
  return true;
}

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
  bool builtInOverridden[GOLF_BUILT_IN_COURSE_COUNT]{};
  if (!Storage.ensureDirectoryExists("/golf") || !Storage.ensureDirectoryExists(COURSE_DIRECTORY) ||
      !Storage.ensureDirectoryExists(ROUNDS_DIRECTORY)) {
    LOG_ERR("GOLF", "Failed to create golf directories");
    return result;
  }
  if (capacity > GOLF_MAX_COURSES) {
    capacity = GOLF_MAX_COURSES;
  }
  HalFile directory = Storage.open(COURSE_DIRECTORY);

  uint16_t jsonFiles = 0;
  for (HalFile entry = directory && directory.isDirectory() ? directory.openNextFile() : HalFile{}; entry;
       entry = directory.openNextFile()) {
    if (entry.isDirectory()) {
      continue;
    }
    char filename[GOLF_COURSE_FILENAME_BUFFER_SIZE]{};
    if (entry.getName(filename, sizeof(filename)) == 0 || !hasJsonExtension(filename)) {
      continue;
    }
    ++jsonFiles;
    GolfCourse course{};
    if (!load(filename, course)) {
      continue;
    }
    int8_t overrideIndex = -1;
    for (uint8_t builtIn = 0; builtIn < GOLF_BUILT_IN_COURSE_COUNT; ++builtIn) {
      if (strcmp(course.courseName, GOLF_BUILT_IN_COURSES[builtIn].courseName) == 0) {
        builtInOverridden[builtIn] = true;
        overrideIndex = static_cast<int8_t>(builtIn);
      }
    }
    if (files != nullptr && result.count < capacity) {
      memcpy(files[result.count].filename, filename, sizeof(filename));
      // CONTRACTS-V2 §7.1: an SD file overriding a built-in inherits its table slot.
      files[result.count].builtInIndex = overrideIndex;
      ++result.count;
    } else {
      result.overflow = true;
    }
  }
  if (jsonFiles > GOLF_MAX_COURSES) {
    result.overflow = true;
  }
  for (uint8_t builtIn = 0; builtIn < GOLF_BUILT_IN_COURSE_COUNT; ++builtIn) {
    if (builtInOverridden[builtIn]) continue;
    if (files != nullptr && result.count < capacity) {
      files[result.count] = {};
      files[result.count].builtInIndex = static_cast<int8_t>(builtIn);
      ++result.count;
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

  GolfCourse loaded{};
  const int version = doc["v"] | 0;
  const int holes = doc["holes"] | -1;
  if (version != 1 || holes < 0 || holes > GolfRound::MAX_HOLES ||
      !copyString(doc["name"], loaded.courseName, sizeof(loaded.courseName), true) ||
      !copyString(doc["tees"], loaded.tees, sizeof(loaded.tees), false)) {
    LOG_ERR("GOLF", "Skipped course %s: invalid metadata", filename);
    return false;
  }
  loaded.holeCount = static_cast<uint8_t>(holes);
  loaded.hasYards = !doc["yards"].isNull();
  loaded.hasSi = !doc["si"].isNull();
  GolfCourseArrayLengths lengths{};
  if (!readArray(doc["par"], loaded.par, lengths.par, UINT8_MAX) ||
      (loaded.hasYards && !readArray(doc["yards"], loaded.yards, lengths.yards, UINT16_MAX)) ||
      (loaded.hasSi && !readArray(doc["si"], loaded.si, lengths.si, UINT8_MAX))) {
    LOG_ERR("GOLF", "Skipped course %s: invalid array value", filename);
    return false;
  }
  const GolfCourseValidationResult validation = validateGolfCourse(loaded, lengths);
  if (!validation.valid) {
    logValidationFailure(filename, validation);
    return false;
  }
  course = loaded;
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

bool CourseStore::findByName(const char* courseName, GolfCourse& course) {
  if (courseName == nullptr || courseName[0] == '\0') return false;
  HalFile directory = Storage.open(COURSE_DIRECTORY);
  for (HalFile entry = directory && directory.isDirectory() ? directory.openNextFile() : HalFile{}; entry;
       entry = directory.openNextFile()) {
    if (entry.isDirectory()) continue;
    char filename[GOLF_COURSE_FILENAME_BUFFER_SIZE]{};
    if (entry.getName(filename, sizeof(filename)) == 0 || !hasJsonExtension(filename)) continue;
    GolfCourse candidate{};
    if (load(filename, candidate) && strcmp(candidate.courseName, courseName) == 0) {
      course = candidate;
      return true;
    }
  }
  for (const GolfCourse& builtIn : GOLF_BUILT_IN_COURSES) {
    if (strcmp(builtIn.courseName, courseName) == 0) {
      course = builtIn;
      return true;
    }
  }
  return false;
}

bool CourseStore::resolveAllTees(const char* courseName, GolfCourseTeeSet& result) {
  result = {};
  if (courseName == nullptr || courseName[0] == '\0') return false;

  auto files = makeUniqueNoThrow<GolfCourseFile[]>(GOLF_MAX_COURSES);
  auto courses = makeUniqueNoThrow<GolfCourse[]>(GOLF_MAX_COURSES);
  if (!files || !courses) {
    LOG_ERR("GOLF", "OOM: resolveAllTees scratch (%u courses)", GOLF_MAX_COURSES);
    return false;
  }

  const GolfCourseListResult listResult = enumerate(files.get(), GOLF_MAX_COURSES);
  uint8_t count = 0;
  for (uint8_t i = 0; i < listResult.count; ++i) {
    GolfCourse course{};
    if (!load(files[i], course)) continue;
    files[count] = files[i];
    courses[count] = course;
    ++count;
  }
  return golfResolveAllTeesFrom(files.get(), courses.get(), count, courseName, result);
}

#endif
