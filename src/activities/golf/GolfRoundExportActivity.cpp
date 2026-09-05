#include "GolfRoundExportActivity.h"

#if defined(CROSSPOINT_GOLF)

#include <FontCacheManager.h>
#include <HalDisplay.h>
#include <I18n.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>

#include "GolfUiLayout.h"
#include "components/UITheme.h"
#include "golf/GolfPenalty.h"
#include "golf/GolfRoundFile.h"
#include "golf/GolfStats.h"

namespace fui = freeink::ui;

GolfRoundExportActivity::GolfRoundExportActivity(GfxRenderer& renderer, MappedInputManager& input,
                                                 const GolfRound& round, uint8_t slot, bool archived,
                                                 const char* filename)
    : Activity("GolfExport", renderer, input), UiAppHost(renderer) {
  data.round = round;
  data.playerSlot = slot;
  data.archived = archived;
  if (filename) snprintf(archiveFilename, sizeof(archiveFilename), "%s", filename);
  if (slot >= GolfRound::MAX_PLAYERS) return;
  const auto& player = round.players[slot];
  auto& summary = data.summary;
  snprintf(summary.course, sizeof(summary.course), "%s", round.courseName);
  snprintf(summary.playerName, sizeof(summary.playerName), "%s", player.name);
  summary.dateYmd = round.dateYmd;
  summary.playerSlot = slot;
  summary.holes = round.holeCount;
  summary.strokes = golfScore(round, player.score);
  summary.par = golfParTotal(round, player.score);
  summary.putts = golfPuttsTotal(round, player.score);
  summary.in100 = golfIn100Total(round, player.score);
  summary.out100 = golfLongTotal(round, player.score);
  summary.hazards = golfHazardsForRound(player.score, round.holeCount);
  summary.obs = golfObsForRound(player.score, round.holeCount);
  summary.penaltiesRecorded = false;  // If archive reload fails, old provenance is unknown.
}

GolfRoundExportActivity::GolfRoundExportActivity(GfxRenderer& renderer, MappedInputManager& input,
                                                 const GolfHistoryEntry& summary, const char* filename)
    : Activity("GolfExport", renderer, input), UiAppHost(renderer) {
  data.summary = summary;
  data.playerSlot = summary.playerSlot;
  data.detailed = false;
  data.penaltiesRecorded = summary.penaltiesRecorded;
  if (filename) snprintf(archiveFilename, sizeof(archiveFilename), "%s", filename);
}

void GolfRoundExportActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&GolfRoundExportActivity::screenTrampoline, this);
  connectionMessage = tr(STR_GOLF_EXPORT_READY);
  LOG_DBG("GOLFEXP", "Heap before export: %u", ESP.getFreeHeap());
  if (archiveFilename[0]) {
    GolfRoundFileInfo info{};
    const bool safe = strchr(archiveFilename, '/') == nullptr && strchr(archiveFilename, '\\') == nullptr;
    snprintf(archivePath, sizeof(archivePath), "/golf/rounds/%s", archiveFilename);
    data.detailed = safe && loadGolfRoundFile(archivePath, data.round, &info);
    data.penaltiesRecorded = data.detailed ? info.penaltiesRecorded : data.summary.penaltiesRecorded;
    data.repaired = data.detailed && info.repaired;
  }
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    errorMessage = tr(STR_GOLF_EXPORT_BUSY);
    requestUpdate();
    return;
  }
  if (auto* fonts = renderer.getFontCacheManager()) fonts->releaseSdFontCaches();
  ownsWifi = true;
  // Enable RF before generating credentials. No HTTP listener is exposed until
  // the password-protected AP configuration has replaced any retained config.
  if (!WiFi.mode(WIFI_AP)) {
    LOG_ERR("GOLFEXP", "Wi-Fi mode failed");
    stopNetwork();
    errorMessage = tr(STR_GOLF_EXPORT_ERROR);
    requestUpdate();
    return;
  }
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
    LOG_ERR("GOLFEXP", "Volatile Wi-Fi configuration failed");
    stopNetwork();
    errorMessage = tr(STR_GOLF_EXPORT_ERROR);
    requestUpdate();
    return;
  }
  uint8_t randomBytes[16];
  esp_fill_random(randomBytes, sizeof(randomBytes));
  static constexpr char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (uint8_t i = 0; i < 16; ++i) password[i] = ALPHABET[randomBytes[i] % (sizeof(ALPHABET) - 1)];
  password[16] = 0;
  snprintf(ssid, sizeof(ssid), "Scorecard-%04X", static_cast<unsigned>((ESP.getEfuseMac() >> 32) & 0xffff));
  if (!WiFi.softAP(ssid, password, 1, false, 1)) {
    LOG_ERR("GOLFEXP", "AP startup failed");
    stopNetwork();
    errorMessage = tr(STR_GOLF_EXPORT_ERROR);
    requestUpdate();
    return;
  }
  const IPAddress ip = WiFi.softAPIP();
  snprintf(url, sizeof(url), "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]);
  snprintf(joinPayload, sizeof(joinPayload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
  snprintf(passwordLine, sizeof(passwordLine), tr(STR_GOLF_EXPORT_PASSWORD), password);
  qrReady = golfqr::encode(joinQr, qrWorkspace, joinPayload) && golfqr::encode(reportQr, qrWorkspace, url);
  if (!qrReady) LOG_ERR("GOLFEXP", "QR generation failed");
  if (ESP.getFreeHeap() < 50 * 1024 || !server.begin(data, golfExportTranslate, millis())) {
    LOG_ERR("GOLFEXP", "Export server unavailable, heap: %u", ESP.getFreeHeap());
    stopNetwork();
    errorMessage = tr(STR_GOLF_EXPORT_ERROR);
  }
  LOG_DBG("GOLFEXP", "Heap with AP/server: %u", ESP.getFreeHeap());
  requestUpdate();
}

void GolfRoundExportActivity::stopNetwork() {
  server.stop();
  if (ownsWifi) {
    if (!WiFi.softAPdisconnect(true)) LOG_ERR("GOLFEXP", "AP disconnect failed");
    if (!WiFi.mode(WIFI_OFF)) LOG_ERR("GOLFEXP", "Wi-Fi shutdown failed");
    ownsWifi = false;
  }
}

void GolfRoundExportActivity::onExit() {
  stopNetwork();
  LOG_DBG("GOLFEXP", "Heap after network teardown: %u", ESP.getFreeHeap());
  Activity::onExit();
}

void GolfRoundExportActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (errorMessage) {
      finish();
      return;
    }
    {
      RenderLock lock(*this);
      showReport = !showReport;
    }
    requestUpdate();
  }
  if (!server.running()) return;
  const uint32_t now = millis();
  // Each poll does at most one nonblocking read/write or one bounded block.
  for (uint8_t i = 0; i < 4; ++i) server.poll(now);
  if (now - server.lastActivity() >= IDLE_TIMEOUT_MS) {
    stopNetwork();
    {
      RenderLock lock(*this);
      errorMessage = tr(STR_GOLF_EXPORT_TIMEOUT);
    }
    requestUpdate();
    return;
  }
  if (now - lastStatusCheck < 500) return;
  lastStatusCheck = now;
  const uint8_t stations = WiFi.softAPgetStationNum();
  const char* status = server.downloadsServed() ? tr(STR_GOLF_EXPORT_SERVED)
                       : server.reportsOpened() ? tr(STR_GOLF_EXPORT_OPENED)
                       : stations               ? tr(STR_GOLF_EXPORT_CONNECTED)
                                                : tr(STR_GOLF_EXPORT_READY);
  if (stations != stationCount || status != connectionMessage) {
    {
      RenderLock lock(*this);
      if (stations > stationCount) showReport = true;
      stationCount = stations;
      connectionMessage = status;
    }
    requestUpdate();
  }
}

void GolfRoundExportActivity::screenTrampoline(UiScreen& screen, void* user) {
  static_cast<GolfRoundExportActivity*>(user)->buildScreen(screen);
}

void GolfRoundExportActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto chrome = golfui::chromeLayout(renderer, screen.frame().safeRect(), metrics.topPadding);
  screen.setContentMargin(chrome.contentMargins);
  auto style = screen.theme().smallText;
  style.align = fui::TextAlign::Center;
  const int16_t line = screen.target().lineHeight(style.font);
  if (errorMessage) {
    screen.centeredText(errorMessage);
    return;
  }
  screen.target().text(screen.takeTop(line * 2), showReport ? tr(STR_GOLF_EXPORT_OPEN) : tr(STR_GOLF_EXPORT_JOIN),
                       style);
  const auto statusRect = screen.takeBottom(line * 2);
  screen.target().text(statusRect, connectionMessage, style);
  if (!data.detailed) screen.target().text(screen.takeBottom(line * 2), tr(STR_GOLF_EXPORT_SUMMARY_NOTE), style);
  screen.target().text(screen.takeBottom(line * 2), showReport ? url : passwordLine, style);
  screen.target().text(screen.takeBottom(line * 2), showReport ? tr(STR_GOLF_EXPORT_SCAN_OPEN) : ssid, style);
  drawQr(screen);
}

// Keep QR drawing temporaries out of the text-layout frame on the C3.
__attribute__((noinline)) void GolfRoundExportActivity::drawQr(UiScreen& screen) {
  const auto body = screen.body();
  const int16_t pixels = golfui::minValue(body.width, body.height) / (golfqr::SIZE + 8);
  if (!qrReady || pixels < 3) {
    screen.centeredText(qrReady ? tr(STR_GOLF_EXPORT_TOO_SMALL) : tr(STR_GOLF_EXPORT_QR_ERROR));
    return;
  }
  const int16_t width = (golfqr::SIZE + 8) * pixels;
  const int16_t x = body.x + (body.width - width) / 2;
  const int16_t y = body.y + (body.height - width) / 2;
  screen.target().fill(fui::Rect{x, y, width, width}, fui::Paint::solid(fui::Color::White));
  const auto& code = showReport ? reportQr : joinQr;
  for (uint8_t row = 0; row < golfqr::SIZE; ++row)
    for (uint8_t col = 0; col < golfqr::SIZE; ++col) {
      if (golfqr::module(code, col, row))
        screen.target().fill(fui::Rect{static_cast<int16_t>(x + (col + 4) * pixels),
                                       static_cast<int16_t>(y + (row + 4) * pixels), pixels, pixels},
                             fui::Paint::solid(fui::Color::Black));
    }
}

void GolfRoundExportActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto chrome = golfui::chromeLayout(renderer, UITheme::getInstance().getMetrics().topPadding);
  golfui::drawHeader(renderer, chrome.header, tr(STR_GOLF_EXPORT_SEND),
                     data.detailed && data.playerSlot < GolfRound::MAX_PLAYERS
                         ? data.round.players[data.playerSlot].name
                         : data.summary.playerName);
  renderUi();
  const auto labels = mappedInput.mapLabels(tr(STR_GOLF_EXPORT_DONE),
                                            errorMessage ? ""
                                            : showReport ? tr(STR_GOLF_EXPORT_PREVIOUS)
                                                         : tr(STR_GOLF_EXPORT_NEXT),
                                            "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}

#endif
