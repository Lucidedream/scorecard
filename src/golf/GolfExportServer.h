#pragma once

#include "GolfRoundExport.h"

// A single nonblocking client with fixed request/output storage. No filesystem
// paths, request bodies, general file manager, or background task are exposed.
class GolfExportServer {
 public:
  ~GolfExportServer();
  bool begin(const GolfExportData& data, GolfExportTranslate translate, uint32_t now, uint16_t port = 80);
  void poll(uint32_t now);
  void stop();
  bool running() const { return listener >= 0; }
  bool clientActive() const { return client >= 0; }
  uint16_t port() const;
  uint32_t lastActivity() const { return lastMeaningful; }
  uint32_t reportsOpened() const { return opened; }
  uint32_t downloadsServed() const { return downloads; }

 private:
  static constexpr uint32_t CLIENT_DEADLINE_MS = 15000;
  const GolfExportData* data = nullptr;
  GolfExportTranslate translate = nullptr;
  int listener = -1;
  int client = -1;
  uint32_t connectedAt = 0;
  uint32_t lastMeaningful = 0;
  uint32_t opened = 0;
  uint32_t downloads = 0;
  char request[1024]{};
  char output[GolfRoundExport::BLOCK_CAPACITY]{};
  size_t requestLength = 0;
  size_t outputLength = 0;
  size_t outputSent = 0;
  bool responding = false;
  bool document = false;
  bool download = false;
  GolfRoundExport cursor;

  void closeClient();
  bool prepare(uint32_t now);
  void error(const char* status);
};
