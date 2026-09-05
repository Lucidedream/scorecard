#pragma once

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "golf/GolfExportServer.h"
#include "golf/GolfPaths.h"
#include "golf/GolfQrCode.h"

class GolfRoundExportActivity final : public Activity, protected UiAppHost {
 public:
  GolfRoundExportActivity(GfxRenderer& renderer, MappedInputManager& input, const GolfRound& round, uint8_t playerSlot,
                          bool archived, const char* filename = nullptr);
  GolfRoundExportActivity(GfxRenderer& renderer, MappedInputManager& input, const GolfHistoryEntry& summary,
                          const char* filename = nullptr);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return server.running(); }

 private:
  static constexpr uint32_t IDLE_TIMEOUT_MS = 5 * 60 * 1000;
  // One checked activity allocation owns all large buffers for the session.
  GolfExportData data{};
  GolfExportServer server;
  golfqr::Code joinQr{};
  golfqr::Code reportQr{};
  golfqr::Workspace qrWorkspace{};
  char archiveFilename[GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  char archivePath[sizeof("/golf/rounds/") + GOLF_ROUND_FILENAME_BUFFER_SIZE]{};
  char ssid[24]{};
  char password[17]{};
  char url[40]{};
  char joinPayload[85]{};
  char passwordLine[64]{};
  const char* errorMessage = nullptr;
  const char* connectionMessage = nullptr;
  bool ownsWifi = false;
  bool showReport = false;
  bool qrReady = false;
  uint8_t stationCount = 0;
  uint32_t lastStatusCheck = 0;

  void stopNetwork();
  void buildScreen(UiScreen& screen);
  void drawQr(UiScreen& screen);
  static void screenTrampoline(UiScreen& screen, void* user);
};
