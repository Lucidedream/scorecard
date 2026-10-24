#pragma once
#include <ArduinoJson.h>
#include <HalStorage.h>
class PersistableStoreBase {
 public:
  static bool readDocFromFile(const char* path, JsonDocument& doc) {
    std::ifstream file(golfTestStorageRoot / (path + 1));
    return file && !deserializeJson(doc, file);
  }
};
