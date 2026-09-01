#include "PersistableStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <cstring>
#include <limits>

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc) {
  if (path == nullptr || doc.overflowed()) {
    LOG_ERR("PERSIST", "Refused incomplete JSON document");
    return false;
  }
  const size_t measured = measureJson(doc);
  if (measured == 0 || measured > std::numeric_limits<unsigned int>::max()) {
    LOG_ERR("PERSIST", "Invalid serialized length for %s", path);
    return false;
  }

  // Reserve the measured payload once before opening/truncating the target.
  // A failed String allocation therefore cannot turn the old file into an
  // empty or partial one, and serialization performs no growth reallocations.
  String json;
  if (!json.reserve(static_cast<unsigned int>(measured))) {
    LOG_ERR("PERSIST", "OOM serializing %s (%u bytes)", path, static_cast<unsigned>(measured));
    return false;
  }
  const size_t written = serializeJson(doc, json);
  if (written != measured || json.length() != measured) {
    LOG_ERR("PERSIST", "Incomplete JSON serialization for %s (%u/%u bytes)", path,
            static_cast<unsigned>(written), static_cast<unsigned>(measured));
    return false;
  }

  Storage.mkdir("/.crosspoint");
  if (!Storage.writeFile(path, json)) {
    LOG_ERR("PERSIST", "Failed to write %s", path);
    return false;
  }
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
  if (!Storage.exists(path)) {
    return false;  // Expected on first boot — not an error.
  }
  String json = Storage.readFile(path);
  if (json.isEmpty()) {
    LOG_ERR("PERSIST", "Failed to read %s (empty)", path);
    return false;
  }
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("PERSIST", "JSON parse error in %s: %s", path, error.c_str());
    return false;
  }
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  bool valid = false;
  return extractPassword(doc, needsResave, std::numeric_limits<size_t>::max(), valid);
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave, const size_t maxLength,
                                                  bool& valid) {
  valid = true;
  bool ok = false;
  bool tooLong = false;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", maxLength, &ok, &tooLong);
  if (tooLong) {
    valid = false;
    return "";
  }
  if (!ok) {
    // Deobfuscation failed — fall back to legacy plaintext password.
    const char* legacyPassword = doc["password"] | "";
    const size_t legacyLength = strlen(legacyPassword);
    if (legacyLength > maxLength) {
      valid = false;
      return "";
    }
    pass.assign(legacyPassword, legacyLength);
    if (!pass.empty()) needsResave = true;
  }
  // A successfully decoded empty string is a legitimate value; preserve as-is.
  return pass;
}
