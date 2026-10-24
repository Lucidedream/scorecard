#pragma once
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
inline std::filesystem::path golfTestStorageRoot;
class HalFile {
  std::filesystem::path path;
  std::shared_ptr<std::ifstream> stream;
  std::vector<std::filesystem::path> children;
  size_t child = 0;

 public:
  HalFile() = default;
  explicit HalFile(std::filesystem::path p) : path(std::move(p)) {
    if (isDirectory()) {
      children.reserve(32);
      for (auto& e : std::filesystem::directory_iterator(path)) children.push_back(e.path());
    } else if (std::filesystem::exists(path))
      stream = std::make_shared<std::ifstream>(path, std::ios::binary);
  }
  explicit operator bool() const { return !path.empty() && std::filesystem::exists(path); }
  bool isDirectory() const { return !path.empty() && std::filesystem::is_directory(path); }
  HalFile openNextFile() { return child < children.size() ? HalFile(children[child++]) : HalFile{}; }
  bool getName(char* out, size_t n) {
    auto s = path.filename().string();
    if (s.size() >= n) return false;
    strcpy(out, s.c_str());
    return true;
  }
  size_t size() { return std::filesystem::file_size(path); }
  int available() { return stream && stream->peek() != EOF; }
  int read(void* out, size_t n) {
    if (!stream) return -1;
    stream->read(static_cast<char*>(out), n);
    return stream->gcount();
  }
  bool seek(size_t offset) {
    if (!stream) return false;
    stream->clear();
    stream->seekg(offset);
    return bool(*stream);
  }
};
class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool exists(const char* p) { return std::filesystem::exists(golfTestStorageRoot / (p + 1)); }
  bool ensureDirectoryExists(const char* p) {
    std::filesystem::create_directories(golfTestStorageRoot / (p + 1));
    return true;
  }
  HalFile open(const char* p) { return HalFile(golfTestStorageRoot / (p + 1)); }
  bool openFileForRead(const char*, const char* p, HalFile& out) {
    out = open(p);
    return bool(out);
  }
};
#define Storage HalStorage::getInstance()
