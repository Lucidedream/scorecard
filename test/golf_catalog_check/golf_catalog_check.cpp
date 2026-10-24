#include <ArduinoJson.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "GolfCourseParse.h"

int main() {
  const char* root = std::getenv("GOLF_CATALOG_DIR");
  if (!root || !root[0]) {
    std::cout << "GOLF_CATALOG_DIR unset; skipping catalog check\n";
    return 77;
  }
  const auto directory = std::filesystem::path(root) / "golf/catalog/courses";
  size_t count = 0;
  try {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
      std::ifstream input(entry.path());
      JsonDocument document;
      const auto error = deserializeJson(document, input);
      if (error) {
        std::cerr << entry.path() << ": " << error.c_str() << '\n';
        return 1;
      }
      GolfCourse course{};
      const auto result = golfParseCourseJson(document.as<JsonVariantConst>(), course);
      if (!result.valid) {
        std::cerr << entry.path() << ": " << golfCourseValidationErrorName(result.error) << " ("
                  << golfCourseFieldName(result.field) << ")\n";
        return 1;
      }
      ++count;
    }
  } catch (const std::filesystem::filesystem_error& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  if (!count) {
    std::cerr << "No catalog courses found in " << directory << '\n';
    return 1;
  }
  std::cout << "Validated " << count << " catalog courses with the firmware v2 parser\n";
  return 0;
}
